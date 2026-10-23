// Opt-in host-side compatibility shims for the installed ReXGlue SDK. Disabled unless the
// environment variable named below is set; with it unset every call is forwarded unchanged.
//
// ACV_COMPAT_MM_WC_RW: MmAllocatePhysicalMemoryEx rejects a protect value that has neither
// X_PAGE_READONLY (0x2) nor X_PAGE_READWRITE (0x4). ACV passes 0x20000400
// (X_MEM_LARGE_PAGES | X_PAGE_WRITECOMBINE) for two allocations (0x40000 and 0x800000 bytes),
// which the SDK refuses and answers with 0. The shim adds X_PAGE_READWRITE to exactly that
// shape and forwards to the SDK implementation. Retail kernel behaviour for this value is not
// established; this is an experiment toggle, not a verified fix.
#include "acv_linux_probe_pch.h"
#include "bake_chain_trace.h"

#include <dlfcn.h>
#include <atomic>
#include <mutex>
#include <fcntl.h>
#include <unistd.h>
#include <rex/system/thread_state.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xfile.h>
#include <rex/filesystem/device.h>
#include <rex/filesystem/entry.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <cerrno>
#include <unordered_map>
#include <algorithm>
#include <cstdarg>
#include <chrono>

// Bounded opt-in import-boundary evidence. This does not change VFS results,
// allocation protection, or guest memory. File tokens survive handle reuse in
// in-flight read records. Physical addresses use the SDK heap translation.
namespace {
struct AcvIoTrace {
  std::mutex mutex;
  FILE* file = nullptr;
  uint64_t sequence = 0, next_token = 0;
  std::unordered_map<uint32_t, uint64_t> files;
  struct Allocation { uint64_t epoch; uint32_t physical, bytes; };
  std::unordered_map<uint32_t, Allocation> allocations;
  bool enabled = false;
  AcvIoTrace() {
    const char* path = std::getenv("ACV_TRACE_RESOURCE_IO");
    if (!path || !*path) return;
    if (*path != '/') {
      std::fprintf(stderr,"[acv-resource] Absolute unique trace path required\n");
      return;
    }
    const int fd = ::open(path, O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC, 0600);
    if (fd < 0) {
      std::fprintf(stderr,"[acv-resource] Cannot create trace: errno=%d\n",errno);
      return;
    }
    file = ::fdopen(fd,"w");
    if (!file) { ::close(fd); return; }
    enabled = true;
    std::fprintf(stderr,"[acv-resource] Trace enabled: %s (max 16384 records)\n",path);
  }
  ~AcvIoTrace() { if(file) std::fclose(file); }
  void row(const char* format, ...) { // caller holds mutex
    if (!enabled) return;
    if (sequence >= 16384) {
      std::fputs("{\"event\":\"truncated\",\"limit\":16384}\n",file);
      std::fflush(file); enabled=false; return;
    }
    std::fprintf(file,"{\"sequence\":%llu,\"time_us\":%llu,",
        (unsigned long long)++sequence,
        (unsigned long long)std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    va_list args; va_start(args,format); std::vfprintf(file,format,args); va_end(args);
    std::fputs("}\n",file);
    if (std::fflush(file) != 0) {
      enabled=false;
      std::fprintf(stderr,"[acv-resource] Trace write failed: errno=%d\n",errno);
    }
  }
};
AcvIoTrace& AcvResources() { static AcvIoTrace trace; return trace; }
void AcvRecordAllocation(uint32_t address, uint32_t bytes, uint32_t lr) {
  auto& t=AcvResources();
  std::lock_guard<std::mutex> lock(t.mutex);
  if (!t.enabled) return;
  uint32_t physical=0, extent=0;
  auto* thread=rex::runtime::ThreadState::Get();
  if(address && thread) {
    auto* memory=thread->memory();
    physical=memory->GetPhysicalAddress(address);
    auto* heap=memory->LookupHeap(address);
    if(heap) heap->QuerySize(address,&extent);
  }
  const uint64_t epoch=++t.next_token;
  uint32_t overlaps=0;
  if(address && extent) {
    for(const auto& [va,a]:t.allocations)
      if(uint64_t(physical)<uint64_t(a.physical)+a.bytes &&
         uint64_t(a.physical)<uint64_t(physical)+extent) ++overlaps;
    t.allocations[address]={epoch,physical,extent};
  }
  // A failed allocation (null result) is an error event, never a live region.
  t.row("\"event\":\"%s\",\"epoch\":%llu,\"va\":%u,\"physical\":%u,"
        "\"requested\":%u,\"extent\":%u,\"overlapping_live_records\":%u,\"lr\":%u",
        address ? "allocate" : "allocate_failed",
        (unsigned long long)epoch,address,physical,bytes,extent,overlaps,lr);
}
} // namespace

namespace {
AcvBakeChainTrace& AcvCpuBakeTrace() {static AcvBakeChainTrace trace("cpu");return trace;}
void AcvTraceBakeCopy(PPCContext& ctx) {
  auto& t=AcvCpuBakeTrace();
  const uint32_t caller=uint32_t(ctx.lr);
  if(!t.enabled() || (caller!=0x82C82518 && caller!=0x82C8391C && caller!=0x82C83EC4) || !ctx.r4.u32 ||
     !ctx.r5.u32 || ctx.r5.u32>0x800000)return;
  auto* thread=rex::runtime::ThreadState::Get();if(!thread)return;
  auto* memory=thread->memory();
  if(caller==0x82C8391C && std::getenv("ACV_DEFER_RESOLVE_START")) {
    struct Gateway {
      void* plugin=nullptr;
      using Fn=bool(*)(uint32_t,uint32_t);
      Fn synchronize=nullptr;
      Gateway() {
        char executable[4096];const auto n=::readlink("/proc/self/exe",executable,sizeof(executable)-1);
        if(n<=0)return;executable[n]=0;
        std::string path(executable);const auto slash=path.find_last_of('/');
        if(slash==std::string::npos)return;
        path.resize(slash+1);path+="librexgpu-xenos.so";
        plugin=::dlopen(path.c_str(),RTLD_NOW|RTLD_NOLOAD);
        if(plugin)synchronize=reinterpret_cast<Fn>(::dlsym(plugin,"AcvResolveReadbackForCpu"));
      }
      ~Gateway(){if(plugin)::dlclose(plugin);}
    };
    static Gateway gateway;
    if(!gateway.synchronize || !gateway.synchronize(memory->GetPhysicalAddress(ctx.r4.u32),ctx.r5.u32)) {
      std::fprintf(stderr,"[acv-deferred] CPU consumer synchronization failed; aborting private test\n");
      std::abort();
    }
  }
  const auto* data=memory->TranslateVirtual<const uint8_t*>(ctx.r4.u32);
  static std::atomic<bool> source_saved{false};
  if(caller==0x82C8391C && ctx.r5.u32==0x200000 && !source_saved.exchange(true))
    AcvBakeSnapshot(".converter-source-first-2m.bin",data,ctx.r5.u32);

  t.Record("\"event\":\"%s\",\"lr\":%u,\"source\":%u,"
      "\"physical\":%u,\"bytes\":%u,\"destination\":%u,\"sampled\":%u,"
      "\"sample_fnv1a64\":\"%016llx\"",
      caller==0x82C82518?"cpu_copy_read":
      (caller==0x82C8391C?"converter_source_copy":"converter_dest_copy"),caller,ctx.r4.u32,
      memory->GetPhysicalAddress(ctx.r4.u32),ctx.r5.u32,ctx.r3.u32,
      std::min(ctx.r5.u32,4096u),(unsigned long long)AcvBakeSampleHash(data,ctx.r5.u32));
}
} // namespace

namespace {
void AcvTraceConverter(uint32_t converter,const char* event) {
  auto& t=AcvCpuBakeTrace();if(!t.enabled() || !converter)return;
  auto* thread=rex::runtime::ThreadState::Get();if(!thread)return;
  auto* memory=thread->memory();auto* base=memory->virtual_membase();
  const uint32_t table=REX_LOAD_U32(converter+0x14);
  const uint32_t source_resource=REX_LOAD_U32(converter+8);
  const uint32_t dest_resource=REX_LOAD_U32(converter+0xc);
  if(!table || !dest_resource)return;
  const uint32_t source_pitch=REX_LOAD_U32(table),source=REX_LOAD_U32(table+4);
  const uint32_t dest_pitch=REX_LOAD_U32(table+8),dest=REX_LOAD_U32(table+12);
  // Copy-source mode detaches +8 after populating the private input table.
  // Init checked equal dimensions, so the retained destination supplies height.
  const uint32_t height=REX_LOAD_U32(dest_resource+0x2c);
  const uint64_t bytes=uint64_t(source_pitch)*height,dest_bytes=uint64_t(dest_pitch)*height;
  const bool output=std::strcmp(event,"converter_output")==0;
  const uint32_t address=output?dest:source;
  const uint64_t length=output?dest_bytes:bytes;
  if(!address || !length || length>0x800000 || uint64_t(address)+length>0x100000000ULL)return;
  const uint32_t sampled=uint32_t(std::min(length,uint64_t(4096)));
  const uint8_t* data=memory->TranslateVirtual<const uint8_t*>(address);
  t.Record("\"event\":\"%s\",\"converter\":%u,\"source_resource\":%u,\"dest_resource\":%u,"
      "\"source\":%u,\"source_physical\":%u,\"dest\":%u,\"dest_physical\":%u,"
      "\"source_pitch\":%u,\"dest_pitch\":%u,\"height\":%u,\"bytes\":%llu,"
      "\"source_format\":%u,\"dest_format\":%u,\"sampled\":%u,\"sample_fnv1a64\":\"%016llx\"",
      event,converter,source_resource,dest_resource,source,memory->GetPhysicalAddress(source),
      dest,memory->GetPhysicalAddress(dest),source_pitch,dest_pitch,height,(unsigned long long)length,
      source_resource ? REX_LOAD_U32(source_resource+0x20) : UINT32_MAX,
      REX_LOAD_U32(dest_resource+0x20),sampled,
      (unsigned long long)AcvBakeSampleHash(data,sampled));
}
}
// @retail 0x82948B48/0x82948B60 execute/finish the tiled-to-linear converter
// before preparing the compressor. Tables are initialized by 0x82C836D8.
REX_EXTERN(__imp__sub_82C835E0);
extern "C" REX_FUNC(sub_82C835E0) {
  if(uint32_t(ctx.lr)==0x82948B4C) AcvTraceConverter(ctx.r3.u32,"converter_input");
  __imp__sub_82C835E0(ctx,base);
}
REX_EXTERN(__imp__sub_82C83DE8);
extern "C" REX_FUNC(sub_82C83DE8) {
  const uint32_t converter=ctx.r3.u32,caller=uint32_t(ctx.lr);
  __imp__sub_82C83DE8(ctx,base);
  if(caller==0x82948B64) AcvTraceConverter(converter,"converter_output");
}

// @retail 0x82BA3900 tail-calls 0x82BA3190 after shuffling r4..r7.
// Verified caller 0x82C8239C: native resource, mip, out {pitch,pixels}, rect,
// flags. Original helper remains responsible for resource synchronization.
REX_EXTERN(__imp__sub_82BA3900);
extern "C" REX_FUNC(sub_82BA3900) {
  auto& t=AcvCpuBakeTrace();
  const uint32_t caller=uint32_t(ctx.lr),native=ctx.r3.u32,mip=ctx.r4.u32;
  const uint32_t output=ctx.r5.u32;
  const bool record=t.enabled() && (caller==0x82C823A0 || caller==0x82C823B8);
  if(record)t.Record("\"event\":\"lock_enter\",\"lr\":%u,\"native\":%u,\"mip\":%u,\"out\":%u",
      caller,native,mip,output);
  __imp__sub_82BA3900(ctx,base);
  if(record && output) {
    const uint32_t pitch=REX_LOAD_U32(output),pixels=REX_LOAD_U32(output+4);
    auto* thread=rex::runtime::ThreadState::Get();
    t.Record("\"event\":\"lock_return\",\"lr\":%u,\"native\":%u,\"mip\":%u,\"pitch\":%u,"
        "\"pixels\":%u,\"physical\":%u",caller,native,mip,pitch,pixels,
        pixels && thread ? thread->memory()->GetPhysicalAddress(pixels) : 0);
  }
}

// Optional first-generation physical write trace. Uses SDK memory watches, not
// a separate signal handler. The callback only writes fixed binary records;
// guest LR is a context snapshot, not the faulting PPC instruction address.
namespace {
struct AcvWriterProbe {
  std::mutex lifecycle_mutex;
  std::atomic<bool> attempted{false}, stopping{false};
  std::atomic<uint32_t> count{0}, io_failures{0};
  rex::memory::Memory* memory = nullptr;
  void* handle = nullptr;
  int fd = -1;
  uint32_t page_size = 4096;
  uint32_t start = 0x1F6B3000;
  static constexpr uint32_t length = 0x80000, limit = 256;
};
AcvWriterProbe acv_writer_probe;

std::pair<uint32_t,uint32_t> AcvWriterCallback(void* opaque, uint32_t start,
                                            uint32_t length, bool exact) {
  auto& probe=*static_cast<AcvWriterProbe*>(opaque);
  if (!length || uint64_t(start)+length <= probe.start ||
      uint64_t(start) >= uint64_t(probe.start)+probe.length) return {0,UINT32_MAX};
  const uint32_t index=probe.count.fetch_add(1,std::memory_order_relaxed);
  if (index < probe.limit) {
    // Little-endian host u32 fields; Linux x86-64 is the current native target.
    uint32_t row[18]={index,start,length,uint32_t(exact)};
    if (auto* thread=rex::runtime::ThreadState::Get()) {
      const auto* ctx=thread->context();
      row[4]=thread->thread_id(); row[5]=uint32_t(ctx->lr); row[6]=ctx->ctr.u32;
      row[7]=ctx->r1.u32;
      row[8]=ctx->r3.u32; row[9]=ctx->r4.u32; row[10]=ctx->r5.u32;
      row[11]=ctx->r6.u32; row[12]=ctx->r7.u32; row[13]=ctx->r8.u32;
      row[14]=ctx->r9.u32; row[15]=ctx->r10.u32;
      row[16]=ctx->r11.u32; row[17]=ctx->r12.u32;
    }
    if (::write(probe.fd,row,sizeof(row)) != sizeof(row))
      probe.io_failures.fetch_add(1,std::memory_order_relaxed);
  }
  // Restrict automatic unwatching to one faulted system page, leaving other
  // watched pages armed. Exact API invalidations remain distinguished above.
  return {start & ~(probe.page_size-1),probe.page_size};
}
void AcvMaybeArmTextureWriter(uint32_t allocated_address,uint32_t size) {
  static const char* path=std::getenv("ACV_TRACE_TEXTURE_WRITES");
  auto& probe=acv_writer_probe;
  if (!path || !*path || !allocated_address) return;
  std::lock_guard<std::mutex> guard(probe.lifecycle_mutex);
  if (probe.stopping.load()) return;
  if (!probe.attempted.load()) {
    if (const char* selected=std::getenv("ACV_TEXTURE_WATCH_START")) {
      char* end=nullptr;errno=0;
      const auto parsed=std::strtoull(selected,&end,0);
      if(errno || end==selected || *end || parsed%4096 || parsed>0x20000000-probe.length) {
        probe.attempted.store(true);
        std::fprintf(stderr,"[acv-writer] Invalid physical watch address; disabled\n");
        return;
      }
      probe.start=uint32_t(parsed);
    }
  }
  auto* thread=rex::runtime::ThreadState::Get();
  if (!thread) return;
  auto* memory=thread->memory();
  const uint32_t physical=memory->GetPhysicalAddress(allocated_address);
  if (physical>probe.start || uint64_t(physical)+size < uint64_t(probe.start)+probe.length) return;
  if (probe.attempted.exchange(true)) return;
  if (*path!='/') { std::fprintf(stderr,"[acv-writer] Absolute output path required; disabled\n");return; }
  probe.fd=::open(path,O_WRONLY|O_CREAT|O_EXCL|O_APPEND|O_CLOEXEC,0600);
  if (probe.fd<0) { std::fprintf(stderr,"[acv-writer] Cannot create unique trace file; disabled\n");return; }
  probe.memory=memory;probe.page_size=uint32_t(rex::memory::page_size());
  probe.handle=memory->RegisterPhysicalMemoryInvalidationCallback(AcvWriterCallback,&probe);
  memory->EnablePhysicalMemoryAccessCallbacks(probe.start,probe.length,true,false);
  std::fprintf(stderr,"[acv-writer] Armed physical=0x%08x length=0x%x after allocation=0x%08x physical=0x%08x size=0x%x file=%s\n",
      probe.start,probe.length,allocated_address,physical,size,path);
}
} // namespace

void AcvStopTextureWriterProbe() {
  auto& probe=acv_writer_probe;
  std::lock_guard<std::mutex> guard(probe.lifecycle_mutex);
  probe.stopping.store(true);
  // ReXApp calls OnShutdown before destroying runtime/memory. Unregister under
  // the SDK's global lock before closing the file used by concurrent callbacks.
  if (probe.handle) {
    probe.memory->UnregisterPhysicalMemoryInvalidationCallback(probe.handle);
    probe.handle=nullptr;
  }
  if (probe.fd>=0) {
    ::close(probe.fd);probe.fd=-1;
    std::fprintf(stderr,"[acv-writer] Stopped: callbacks=%u captured_limit=%u io_failures=%u\n",
        probe.count.load(),probe.limit,probe.io_failures.load());
  }
}

// @retail 0x82C83100: aligned-copy wrapper calling 0x830D54A8.
// Generated Linux public entries are overridable weak aliases; __imp__ remains
// the original implementation. Record only the verified texture-converter call.
REX_EXTERN(__imp__sub_82C83100);
extern "C" REX_FUNC(sub_82C83100) {
  AcvTraceBakeCopy(ctx);
  static const char* path=std::getenv("ACV_TRACE_TEXTURE_WRITES");
  static std::atomic<bool> captured{false};
  static std::atomic<bool> origin_captured{false};
  static std::atomic<bool> rgba_origin_captured{false};
  // @retail 0x82C824E8..0x82C82514: copy original LockRect input into
  // compressor staging before compression. r25=source, r24=destination.
  if(path && *path && ctx.lr==0x82C82518 && ctx.r5.u32==0x200000 &&
     REX_LOAD_U32(ctx.r24.u32+0x28)==1024 &&
     REX_LOAD_U32(ctx.r24.u32+0x2C)==512 &&
     REX_LOAD_U32(ctx.r24.u32+0x20)==9 && !rgba_origin_captured.exchange(true)) {
    if(auto* thread=rex::runtime::ThreadState::Get()) {
      auto* memory=thread->memory();
      auto dump=[&](const char* suffix,uint32_t address,uint32_t length) {
        const std::string file=std::string(path)+suffix;
        const int fd=::open(file.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
        if(fd<0)return int64_t(-1);
        const auto* data=memory->TranslateVirtual<const uint8_t*>(address);
        uint32_t done=0;
        while(done<length) {
          const auto n=::write(fd,data+done,length-done);
          if(n<0 && errno==EINTR)continue;
          if(n<=0)break;
          done+=uint32_t(n);
        }
        ::close(fd);return int64_t(done);
      };
      const uint32_t source=ctx.r25.u32,native=REX_LOAD_U32(source+0x10);
      const auto bytes=dump(".rgba-origin.bin",ctx.r4.u32,ctx.r5.u32);
      dump(".rgba-resource.bin",source,0x38);
      dump(".rgba-native.bin",native,0x34);
      const uint32_t physical=ctx.r4.u32>=0xA0000000?memory->GetPhysicalAddress(ctx.r4.u32):UINT32_MAX;
      std::fprintf(stderr,"[acv-writer] RGBA origin src=0x%08x physical=0x%08x dest=0x%08x bytes=%lld resource=0x%08x native=0x%08x converter=0x%08x\n",
          ctx.r4.u32,physical,ctx.r3.u32,(long long)bytes,source,native,ctx.r29.u32);
    }
  }
  // @retail 0x82C8390C..0x82C83918: preserve the original input before
  // the converter copies it into its private staging allocation.
  if (path && *path && ctx.lr==0x82C8391C && ctx.r5.u32==0x80000 &&
      REX_LOAD_U32(ctx.r24.u32+0x28)==1024 &&
      REX_LOAD_U32(ctx.r24.u32+0x2C)==512 &&
      REX_LOAD_U32(ctx.r24.u32+0x20)==8 && !origin_captured.exchange(true)) {
    auto* thread=rex::runtime::ThreadState::Get();
    if(thread) {
      auto* memory=thread->memory();
      auto dump_origin=[&](const char* suffix,uint32_t address,uint32_t length) {
        const std::string file=std::string(path)+suffix;
        const int fd=::open(file.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
        if(fd<0)return int64_t(-1);
        const auto* data=memory->TranslateVirtual<const uint8_t*>(address);
        uint32_t done=0;
        while(done<length) {
          auto n=::write(fd,data+done,length-done);
          if(n<0 && errno==EINTR)continue;
          if(n<=0)break;
          done+=uint32_t(n);
        }
        ::close(fd);return int64_t(done);
      };
      auto bytes=dump_origin(".origin.bin",ctx.r4.u32,ctx.r5.u32);
      dump_origin(".origin-resource.bin",ctx.r26.u32,0x80);
      if(ctx.r4.u32>=4096)dump_origin(".origin-prefix.bin",ctx.r4.u32-4096,4096);
      std::fprintf(stderr,"[acv-writer] Original converter input=0x%08x dest=0x%08x bytes=%lld resource=0x%08x converter=0x%08x\n",
          ctx.r4.u32,ctx.r3.u32,(long long)bytes,ctx.r26.u32,ctx.r30.u32);
    }
  }
  if (path && *path && ctx.lr==0x82C83EC4 && ctx.r5.u32==0x80000 &&
      ctx.r3.u32>=0xA0000000) {
    auto* thread=rex::runtime::ThreadState::Get();
    if (thread && thread->memory()->GetPhysicalAddress(ctx.r3.u32)==0x1F6B3000 &&
        !captured.exchange(true)) {
      auto* memory=thread->memory();
      auto dump=[&](const char* suffix,uint32_t guest,uint32_t length) {
        const std::string name=std::string(path)+suffix;
        const int fd=::open(name.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
        if(fd<0)return int64_t(-1);
        const auto* data=memory->TranslateVirtual<const uint8_t*>(guest);
        uint32_t done=0;
        while(done<length) {
          // Linux returns EFAULT for an inaccessible source without delivering
          // a nested userspace fault; retain the actual partial length.
          const auto n=::write(fd,data+done,length-done);
          if(n<0 && errno==EINTR)continue;
          if(n<=0)break;
          done+=uint32_t(n);
        }
        ::close(fd);return int64_t(done);
      };
      const uint32_t converter=ctx.r30.u32;
      // @retail 0x82C83E9C..0x82C83EBC: converter+0x0C and +0x14.
      const uint32_t resource=REX_LOAD_U32(converter+0x0C);
      const uint32_t source_object=REX_LOAD_U32(converter+0x14);
      const auto payload_bytes=dump(".source.bin",ctx.r4.u32,ctx.r5.u32);
      const auto converter_bytes=dump(".converter.bin",converter,0x2C);
      const auto resource_bytes=dump(".resource.bin",resource,0x80);
      // @retail 0x82C834EC..0x82C83520: +0x14 is a 16-byte-per-level
      // table: source pitch/pointer, destination pitch/pointer. Preserve the
      // earlier .source-object filename solely for trace compatibility.
      const auto object_bytes=dump(".source-object.bin",source_object,0x80);
      const uint32_t input_source=REX_LOAD_U32(source_object+4);
      const auto input_bytes=dump(".input.bin",input_source,ctx.r5.u32);
      std::fprintf(stderr,"[acv-writer] Converter input=0x%08x bounded_dump=%lld source_pitch=0x%x destination_pitch=0x%x\n",
          input_source,(long long)input_bytes,REX_LOAD_U32(source_object),REX_LOAD_U32(source_object+8));
      // A bounded source prefix can expose a container header; no interpretation
      // or assumption about allocation ownership follows from these bytes.
      const auto prefix_bytes=ctx.r4.u32>=4096 ? dump(".prefix.bin",ctx.r4.u32-4096,4096) : -1;
      const std::string json_path=std::string(path)+".copy.json";
      const int fd=::open(json_path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
      if(fd>=0){
        char json[1024];
        const int n=std::snprintf(json,sizeof(json),
          "{\"lr\":%u,\"destination\":%u,\"source\":%u,\"bytes\":%u,\"converter\":%u,\"resource\":%u,\"source_object\":%u,\"payload_bytes\":%lld,\"converter_bytes\":%lld,\"resource_bytes\":%lld,\"object_bytes\":%lld,\"prefix_bytes\":%lld}\n",
          uint32_t(ctx.lr),ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,converter,resource,source_object,
          (long long)payload_bytes,(long long)converter_bytes,(long long)resource_bytes,
          (long long)object_bytes,(long long)prefix_bytes);
        if(n>0 && n<int(sizeof(json)))::write(fd,json,n);
        ::close(fd);
      }
      std::fprintf(stderr,"[acv-writer] Converter copy source=0x%08x dest=0x%08x bytes=0x%x source_dump=%lld\n",
                   ctx.r4.u32,ctx.r3.u32,ctx.r5.u32,(long long)payload_bytes);
    }
  }
  __imp__sub_82C83100(ctx,base);
}

// @retail 0x82C82130: DLTextureCompressor finish; capture its input/output
// table before the original finish copies the compressed result to the resource.
REX_EXTERN(__imp__sub_82C82130);
static void AcvCaptureCompressor(PPCContext& ctx,uint8_t* base,bool before) {
  static const char* path=std::getenv("ACV_TRACE_TEXTURE_WRITES");
  static std::atomic<bool> captured[2]{};
  if(path && *path && REX_LOAD_U8(ctx.r3.u32+0x10)) {
    const uint32_t converter=ctx.r3.u32,resource=REX_LOAD_U32(converter+0x0C);
    if(resource && REX_LOAD_U32(resource+0x28)==1024 &&
       REX_LOAD_U32(resource+0x2C)==512 && REX_LOAD_U32(resource+0x20)==9 &&
       !captured[before?1:0].exchange(true)) {
      auto* thread=rex::runtime::ThreadState::Get();
      if(thread) {
        auto* memory=thread->memory();
        auto dump=[&](const char* suffix,uint32_t address,uint32_t length) {
          const std::string file=std::string(path)+(before?".before":"")+suffix;
          const int fd=::open(file.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
          if(fd<0)return int64_t(-1);
          const auto* data=memory->TranslateVirtual<const uint8_t*>(address);
          uint32_t done=0;
          while(done<length) {
            const auto n=::write(fd,data+done,length-done);
            if(n<0 && errno==EINTR)continue;
            if(n<=0)break;
            done+=uint32_t(n);
          }
          ::close(fd);return int64_t(done);
        };
        const uint32_t table=REX_LOAD_U32(converter+0x14);
        const uint32_t input=REX_LOAD_U32(table+4),output=REX_LOAD_U32(table+12);
        const uint32_t pitch=REX_LOAD_U32(table);
        const uint64_t requested=uint64_t(pitch)*512;
        const auto bytes=requested && requested<=0x800000 && uint64_t(input)+requested<=0x100000000ULL
            ? dump(".compressor-input.bin",input,uint32_t(requested)) : -1;
        if(!before)dump(".compressor-output.bin",output,0x80000);
        dump(".compressor-table.bin",table,16);
        dump(".compressor-state.bin",converter,0x2C);
        const uint32_t source_resource=REX_LOAD_U32(converter+8);
        if(source_resource)dump(".compressor-source-resource.bin",source_resource,0x38);
        std::fprintf(stderr,"[acv-writer] Compressor boundary=%s lr=0x%08x converter=0x%08x input=0x%08x output=0x%08x pitch=0x%x captured=%lld\n",
            before?"before":"after",uint32_t(ctx.lr),converter,input,output,pitch,(long long)bytes);
      }
    }
  }
}

extern "C" REX_FUNC(sub_82C82130) {
  AcvCaptureCompressor(ctx,base,false);
  __imp__sub_82C82130(ctx,base);
}
// @retail 0x82948C74..0x82948C78: convert all levels before finish.
REX_EXTERN(__imp__sub_82C82090);
extern "C" REX_FUNC(sub_82C82090) {
  AcvCaptureCompressor(ctx,base,true);
  __imp__sub_82C82090(ctx,base);
}

// @retail 0x82B8F618: general aligned-copy dispatcher. Read-only provenance
// for the original texture source, before its first watched CPU write.
REX_EXTERN(__imp__sub_82B8F618);
extern "C" REX_FUNC(sub_82B8F618) {
  static const char* path=std::getenv("ACV_TRACE_TEXTURE_WRITES");
  static std::atomic<bool> captured{false};
  if(path && *path && ctx.r3.u32>=0xA0000000 && ctx.r5.u32>=0x80000 &&
     ctx.r5.u32<=0x100000) {
    auto* thread=rex::runtime::ThreadState::Get();
    if(thread && thread->memory()->GetPhysicalAddress(ctx.r3.u32)==0x1E112000 &&
       !captured.exchange(true)) {
      auto* memory=thread->memory();
      auto dump=[&](const char* suffix,uint32_t address,uint32_t length) {
        const std::string file=std::string(path)+suffix;
        const int fd=::open(file.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
        if(fd<0)return int64_t(-1);
        const auto* data=memory->TranslateVirtual<const uint8_t*>(address);
        uint32_t done=0;
        while(done<length) {
          const auto n=::write(fd,data+done,length-done);
          if(n<0 && errno==EINTR)continue;
          if(n<=0)break;
          done+=uint32_t(n);
        }
        ::close(fd);return int64_t(done);
      };
      const auto bytes=dump(".producer-input.bin",ctx.r4.u32,ctx.r5.u32);
      if(ctx.r4.u32>=0x4000)dump(".producer-prefix.bin",ctx.r4.u32-0x4000,0x4000);
      // ABI context only: these stack bytes are not an inferred call chain.
      dump(".producer-stack.bin",ctx.r1.u32,0x400);
      std::fprintf(stderr,"[acv-writer] Source producer copy lr=0x%08x source=0x%08x destination=0x%08x requested=0x%x captured=%lld sp=0x%08x\n",
          uint32_t(ctx.lr),ctx.r4.u32,ctx.r3.u32,ctx.r5.u32,(long long)bytes,ctx.r1.u32);
    }
  }
  __imp__sub_82B8F618(ctx,base);
}

extern "C" REX_FUNC(__imp__MmAllocatePhysicalMemoryEx) {
  using Fn = void (*)(PPCContext&, uint8_t*);
  // Next definition after this executable: the SDK's own export in librexruntime.
  static const Fn original = reinterpret_cast<Fn>(dlsym(RTLD_NEXT, "__imp__MmAllocatePhysicalMemoryEx"));
  static const bool enabled = std::getenv("ACV_COMPAT_MM_WC_RW") != nullptr;
  static const bool trace = std::getenv("ACV_TRACE_PHYSICAL_ALLOCS") != nullptr;
  static std::atomic<uint32_t> trace_count{0};
  const bool record = trace && trace_count.fetch_add(1, std::memory_order_relaxed) < 128;
  // Capture input arguments before the SDK clobbers the PPC return registers.
  const uint32_t caller_lr = static_cast<uint32_t>(ctx.lr);
  // @retail 0x82B90210: stw LR,-8(sp), then stwu sp,-96(sp).
  // At its import call the saved caller LR is at current guest sp+0x58.
  const uint32_t parent_lr = record && caller_lr == 0x82B9026C
      ? REX_LOAD_U32(ctx.r1.u32 + 0x58) : 0;
  const uint32_t flags = ctx.r3.u32, size = ctx.r4.u32;
  const uint32_t min_address = ctx.r6.u32, max_address = ctx.r7.u32, alignment = ctx.r8.u32;
  if (original == nullptr) {
    std::fprintf(stderr, "[acv-compat] SDK __imp__MmAllocatePhysicalMemoryEx not found\n");
    std::abort();
  }
  constexpr uint32_t kReadOnly = 0x2, kReadWrite = 0x4, kWriteCombine = 0x400;
  const uint32_t protect = ctx.r5.u32;  // third argument (flags, size, protect, ...)
  if (enabled && (protect & (kReadOnly | kReadWrite)) == 0 && (protect & kWriteCombine) != 0) {
    ctx.r5.u64 = protect | kReadWrite;
    std::fprintf(stderr, "[acv-compat] MmAllocatePhysicalMemoryEx size=0x%x protect 0x%x -> 0x%x\n",
                 ctx.r4.u32, protect, ctx.r5.u32);
  }
  original(ctx, base);
  AcvMaybeArmTextureWriter(ctx.r3.u32, size);
  AcvRecordAllocation(ctx.r3.u32, size, caller_lr);
  if (record) {
    std::fprintf(stderr,
        "[acv-mm-trace] lr=0x%08x parent_lr=0x%08x flags=0x%x size=0x%x protect_in=0x%x protect_effective=0x%x min=0x%x max=0x%x align=0x%x result=0x%08x\n",
        caller_lr, parent_lr, flags, size, protect, (enabled && !(protect & (kReadOnly | kReadWrite)) &&
        (protect & kWriteCombine)) ? protect | kReadWrite : protect,
        min_address, max_address, alignment, ctx.r3.u32);
  }
}

// ---------------------------------------------------------------------------------------------
// Profile names from acv.config (passed by probe_linux.py through the environment).
//   ACV_PILOT_NAME : text the game's pilot-name keyboard prompt (XamShowKeyboardUI) returns.
//   ACV_GAMERTAG   : name returned by XamUserGetName / XamUserGetGamerTag (SDK default: "User").
// With neither variable set every call is forwarded unchanged. The keyboard prompt is answered
// without a dialog only when the SDK runs with --headless=true (it then copies default_text into
// the output buffer and completes the overlapped request itself).
namespace {

using HostFn = void (*)(PPCContext&, uint8_t*);

HostFn NextExport(const char* symbol) {
  auto fn = reinterpret_cast<HostFn>(dlsym(RTLD_NEXT, symbol));
  if (fn == nullptr) {
    std::fprintf(stderr, "[acv-compat] SDK export %s not found\n", symbol);
    std::abort();
  }
  return fn;
}

// Decodes UTF-8 leniently (invalid bytes become '?'); BMP only, longer sequences become '?'.
std::u16string Utf8ToUtf16(const char* text, size_t max_units) {
  std::u16string out;
  const auto* p = reinterpret_cast<const unsigned char*>(text);
  while (*p != 0 && out.size() < max_units) {
    uint32_t cp = '?';
    int extra = 0;
    if (*p < 0x80) { cp = *p; extra = 0; }
    else if ((*p & 0xE0) == 0xC0) { cp = *p & 0x1F; extra = 1; }
    else if ((*p & 0xF0) == 0xE0) { cp = *p & 0x0F; extra = 2; }
    else { extra = -1; }
    ++p;
    if (extra < 0) { out.push_back(u'?'); continue; }
    bool ok = true;
    for (int i = 0; i < extra; ++i, ++p) {
      if ((*p & 0xC0) != 0x80) { ok = false; break; }
      cp = (cp << 6) | (*p & 0x3F);
    }
    out.push_back(ok && cp <= 0xFFFF ? static_cast<char16_t>(cp) : u'?');
  }
  return out;
}

// Writes `text` as NUL-terminated big-endian UTF-16 into guest memory (capacity in char16 units).
void StoreGuestWide(uint8_t* base, uint32_t guest, uint32_t capacity, const std::u16string& text) {
  if (guest == 0 || capacity == 0) return;
  const size_t n = text.size() < capacity - 1 ? text.size() : capacity - 1;
  for (size_t i = 0; i < n; ++i) REX_STORE_U16(guest + i * 2, static_cast<uint16_t>(text[i]));
  REX_STORE_U16(guest + n * 2, 0);
}

}  // namespace

extern "C" REX_FUNC(__imp__XamShowKeyboardUI) {
  static const HostFn original = NextExport("__imp__XamShowKeyboardUI");
  // (user_index r3, flags r4, default_text r5, title r6, description r7, buffer r8, length r9, overlapped r10)
  const char* name = std::getenv("ACV_PILOT_NAME");
  const uint32_t buffer = ctx.r8.u32, length = ctx.r9.u32;
  if (name != nullptr && *name != '\0' && buffer != 0 && length > 1) {
    StoreGuestWide(base, buffer, length, Utf8ToUtf16(name, length - 1));
    ctx.r5.u64 = buffer;  // headless path copies default_text (now our name) into the buffer
    std::fprintf(stderr, "[acv-compat] XamShowKeyboardUI: pilot name \"%s\"\n", name);
  }
  original(ctx, base);
}

extern "C" REX_FUNC(__imp__XamUserGetName) {
  static const HostFn original = NextExport("__imp__XamUserGetName");
  const uint32_t buffer = ctx.r4.u32, length = ctx.r5.u32;
  original(ctx, base);
  const char* name = std::getenv("ACV_GAMERTAG");
  if (name != nullptr && *name != '\0' && ctx.r3.u32 == 0 && buffer != 0 && length > 0) {
    const uint32_t cap = length < 16 ? length : 16;  // the SDK limits the name to 16 bytes
    uint32_t i = 0;
    for (; name[i] != '\0' && i + 1 < cap; ++i) REX_STORE_U8(buffer + i, static_cast<uint8_t>(name[i]));
    REX_STORE_U8(buffer + i, 0);
  }
}

extern "C" REX_FUNC(__imp__XamUserGetGamerTag) {
  static const HostFn original = NextExport("__imp__XamUserGetGamerTag");
  const uint32_t buffer = ctx.r4.u32, length = ctx.r5.u32;
  original(ctx, base);
  const char* name = std::getenv("ACV_GAMERTAG");
  if (name != nullptr && *name != '\0' && ctx.r3.u32 == 0 && buffer != 0 && length >= 16) {
    StoreGuestWide(base, buffer, 16, Utf8ToUtf16(name, 15));
  }
}

// Host compatibility experiment. SDK xboxkrnl_video.cpp:150-157 returns a
// hard-coded BT.709 type (2). Primary Xenia ACV compatibility issue #280 reports
// sRGB type (1). Preserve the SDK output unless explicitly requested; no retail
// instruction or gamma-ramp write is replaced by this hook.
extern "C" REX_FUNC(__imp__VdGetCurrentDisplayGamma) {
  static const HostFn original = NextExport("__imp__VdGetCurrentDisplayGamma");
  const uint32_t type_address = ctx.r3.u32;
  original(ctx, base);
  const char* setting = std::getenv("ACV_DISPLAY_GAMMA_TYPE");
  if (setting == nullptr || setting[0] < '0' || setting[0] > '3' || setting[1] != '\0') return;
  if (type_address == 0) return;
  const uint32_t previous = REX_LOAD_U32(type_address);
  const uint32_t selected = static_cast<uint32_t>(setting[0] - '0');
  REX_STORE_U32(type_address, selected);
  std::fprintf(stderr, "[acv-compat] VdGetCurrentDisplayGamma type %u -> %u\n", previous, selected);
}


// SDK import ABI: xboxkrnl_io.cpp NtCreateFile/NtOpenFile/NtReadFile_entry.
// The pinned SDK performs Read synchronously even when returning STATUS_PENDING;
// iosb.status/information describe the completed payload. PENDING IOSB is never
// hashed. Implicit offsets use a pre-call SDK position snapshot; this is an
// observation, not atomic with Read. Validate it against source bytes before
// accepting an effective offset under concurrent file access.
namespace {
void AcvTraceOpen(PPCContext& ctx, uint8_t* base, HostFn original) {
  auto& t=AcvResources();
  const uint32_t out=ctx.r3.u32, attrs=ctx.r5.u32, lr=uint32_t(ctx.lr);
  std::string path_hex;
  uint32_t root=0;
  {
    std::lock_guard<std::mutex> lock(t.mutex);
    if(t.enabled && attrs) {
      root=REX_LOAD_U32(attrs);
      const uint32_t name=REX_LOAD_U32(attrs+4);
      if(name) {
        const uint32_t length=std::min<uint32_t>(REX_LOAD_U16(name),1024);
        const uint32_t data=REX_LOAD_U32(name+4);
        if(data && uint64_t(data)+length<=0x100000000ULL) {
          static constexpr char hex[]="0123456789abcdef";
          for(uint32_t i=0;i<length;++i) {
            const uint8_t c=REX_LOAD_U8(data+i);
            path_hex+=hex[c>>4]; path_hex+=hex[c&15];
          }
        }
      }
    }
  }
  original(ctx,base);
  const uint32_t status=ctx.r3.u32;
  const uint32_t handle=status==0 && out ? REX_LOAD_U32(out) : 0;
  // Selected file after VFS ResolvePath: device mount path and entry path of the
  // opened XFile. Hex-encoded so arbitrary guest names stay valid JSON.
  std::string device_hex, entry_hex;
  unsigned long long entry_size=0;
  if(handle) {
    auto* kernel=rex::runtime::current_kernel_state();
    auto file=kernel ? kernel->object_table()->LookupObject<rex::system::XFile>(handle) : nullptr;
    if(file && file->entry()) {
      static constexpr char hex[]="0123456789abcdef";
      auto encode=[&](const std::string& s, std::string& outhex) {
        for(unsigned char c : s.substr(0, 1024)) { outhex+=hex[c>>4]; outhex+=hex[c&15]; }
      };
      if(file->device()) encode(file->device()->mount_path(), device_hex);
      encode(file->entry()->absolute_path(), entry_hex);
      entry_size=file->entry()->size();
    }
  }
  std::lock_guard<std::mutex> lock(t.mutex);
  if(!t.enabled) return;
  // token is the open number: a numeric handle may be reused after NtClose.
  const uint64_t token=handle ? ++t.next_token : 0;
  if(handle) t.files[handle]=token;
  t.row("\"event\":\"open\",\"handle\":%u,\"token\":%llu,\"root\":%u,"
        "\"status\":%u,\"lr\":%u,\"path_hex\":\"%s\",\"device_hex\":\"%s\","
        "\"entry_hex\":\"%s\",\"entry_size\":%llu",
        handle,(unsigned long long)token,root,status,lr,path_hex.c_str(),
        device_hex.c_str(),entry_hex.c_str(),entry_size);
}
}
extern "C" REX_FUNC(__imp__NtCreateFile) {
  static const HostFn original=NextExport("__imp__NtCreateFile");
  AcvTraceOpen(ctx,base,original);
}
extern "C" REX_FUNC(__imp__NtOpenFile) {
  static const HostFn original=NextExport("__imp__NtOpenFile");
  AcvTraceOpen(ctx,base,original);
}
extern "C" REX_FUNC(__imp__NtReadFile) {
  static const HostFn original=NextExport("__imp__NtReadFile");
  auto& t=AcvResources();
  const uint32_t handle=ctx.r3.u32, iosb=ctx.r7.u32, buffer=ctx.r8.u32;
  const uint32_t requested=ctx.r9.u32, offset_ptr=ctx.r10.u32, lr=uint32_t(ctx.lr);
  uint64_t token=0, offset=0; bool record=false;
  {
    std::lock_guard<std::mutex> lock(t.mutex);
    record=t.enabled;
    if(record) {
      auto it=t.files.find(handle); if(it!=t.files.end()) token=it->second;
      if(offset_ptr) offset=REX_LOAD_U64(offset_ptr);
    }
  }
  uint64_t observed_offset=offset;
  bool offset_observed=record && offset_ptr && offset!=UINT64_MAX;
  if(record && !offset_observed) {
    auto* kernel=rex::runtime::current_kernel_state();
    auto file=kernel->object_table()->LookupObject<rex::system::XFile>(handle);
    if(file) { observed_offset=file->position(); offset_observed=true; }
  }
  original(ctx,base);
  if(!record) return;
  const uint32_t status=ctx.r3.u32;
  // Only inspect IOSB on a successful or pending API return. On immediate
  // failure its bytes may belong to an earlier request.
  const bool iosb_valid=iosb && (status==0 || status==0x103);
  const uint32_t io_status=iosb_valid ? REX_LOAD_U32(iosb) : status;
  const uint32_t actual=iosb_valid ? REX_LOAD_U32(iosb+4) : 0;
  const uint32_t sampled=io_status==0 && actual<=requested && buffer &&
      uint64_t(buffer)+actual<=0x100000000ULL ? std::min(actual,8u*1024*1024) : 0;
  uint64_t hash=14695981039346656037ULL; uint32_t nonzero=0;
  for(uint32_t i=0;i<sampled;++i) {
    uint8_t c=REX_LOAD_U8(buffer+i); hash=(hash^c)*1099511628211ULL; nonzero+=(c!=0);
  }
  std::lock_guard<std::mutex> lock(t.mutex);
  t.row("\"event\":\"read\",\"handle\":%u,\"token\":%llu,\"offset_explicit\":%s,"
        "\"requested_offset\":%llu,\"offset_observed\":%s,\"observed_offset\":%llu,\"buffer\":%u,\"requested\":%u,\"actual\":%u,"
        "\"status\":%u,\"iosb_status\":%u,\"sampled\":%u,\"nonzero\":%u,"
        "\"fnv1a64\":\"%016llx\",\"lr\":%u",
        handle,(unsigned long long)token,offset_ptr?"true":"false",
        (unsigned long long)offset,offset_observed?"true":"false",
        (unsigned long long)observed_offset,buffer,requested,actual,status,io_status,
        sampled,nonzero,(unsigned long long)hash,lr);
}
extern "C" REX_FUNC(__imp__NtClose) {
  static const HostFn original=NextExport("__imp__NtClose");
  auto& t=AcvResources(); const uint32_t handle=ctx.r3.u32;
  uint64_t token=0;
  {std::lock_guard<std::mutex> lock(t.mutex);
   if(t.enabled) {auto it=t.files.find(handle);if(it!=t.files.end())token=it->second;}}
  original(ctx,base);
  if(!token) return;
  std::lock_guard<std::mutex> lock(t.mutex);
  auto it=t.files.find(handle);
  if(ctx.r3.u32==0 && it!=t.files.end() && it->second==token) t.files.erase(it);
  t.row("\"event\":\"close\",\"handle\":%u,\"token\":%llu,\"status\":%u",
        handle,(unsigned long long)token,ctx.r3.u32);
}
extern "C" REX_FUNC(__imp__MmAllocatePhysicalMemory) {
  static const HostFn original=NextExport("__imp__MmAllocatePhysicalMemory");
  const uint32_t bytes=ctx.r4.u32, lr=uint32_t(ctx.lr);
  original(ctx,base); AcvRecordAllocation(ctx.r3.u32,bytes,lr);
}
extern "C" REX_FUNC(__imp__MmFreePhysicalMemory) {
  static const HostFn original=NextExport("__imp__MmFreePhysicalMemory");
  auto& t=AcvResources(); const uint32_t address=ctx.r4.u32,lr=uint32_t(ctx.lr);
  uint64_t epoch=0;
  {std::lock_guard<std::mutex> lock(t.mutex);
   if(t.enabled) {auto it=t.allocations.find(address);
     if(it!=t.allocations.end()) {epoch=it->second.epoch;t.allocations.erase(it);}
     t.row("\"event\":\"free_request\",\"va\":%u,\"epoch\":%llu,\"lr\":%u",
           address,(unsigned long long)epoch,lr);}}
  // The SDK export returns void. This records intent, not proof of release or
  // GPU-reference retirement; unmatched aliases require heap-level follow-up.
  original(ctx,base);
}
