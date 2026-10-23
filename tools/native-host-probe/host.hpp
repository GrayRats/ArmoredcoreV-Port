#pragma once
// Deliberately bounded x86-64 test host for the hedge-dev PPC ABI.
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace acv_probe {
class Memory {
    static constexpr uint64_t size = uint64_t{1} << 32;
    static uint64_t host_page_size() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        const auto result = info.dwPageSize;
#else
        const long result = sysconf(_SC_PAGESIZE);
#endif
        if (result <= 0 || uint64_t(result) > size || size % uint64_t(result) ||
            (uint64_t(result) & (uint64_t(result) - 1)))
            throw std::runtime_error("invalid host page size");
        return uint64_t(result);
    }
    uint8_t* data_;
    uint64_t page_;
    std::vector<bool> pages_;
public:
    Memory() : data_(nullptr), page_(host_page_size()), pages_(size / page_) {
        static_assert(sizeof(void*) == 8);
#ifdef _WIN32
        auto p = VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_NOACCESS);
        if (!p) throw std::runtime_error("guest address reservation failed");
#else
        auto p = mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        if (p == MAP_FAILED) throw std::runtime_error("guest address reservation failed");
#endif
        data_ = static_cast<uint8_t*>(p);
    }
    ~Memory() {
#ifdef _WIN32
        VirtualFree(data_, 0, MEM_RELEASE);
#else
        munmap(data_, size);
#endif
    }
    Memory(const Memory&) = delete;
    Memory& operator=(const Memory&) = delete;
    uint8_t* base() const { return data_; }
    uint64_t page_size() const { return page_; }
    void commit(uint64_t address, uint64_t length) {
        if (!length || address >= size || length > size - address ||
            address % page_ || length % page_) throw std::out_of_range("invalid guest commit");
#ifdef _WIN32
        if (VirtualAlloc(data_ + address, length, MEM_COMMIT, PAGE_READWRITE) != data_ + address)
#else
        if (mprotect(data_ + address, length, PROT_READ | PROT_WRITE))
#endif
            throw std::runtime_error("guest page commit failed");
        for (auto p = address / page_; p < (address + length) / page_; ++p) pages_[p] = true;
    }
    uint8_t* checked(uint8_t* base, uint64_t address, uint64_t length) const {
        if (base != data_ || !length || address >= size || length > size - address)
            throw std::out_of_range("guest access outside 32-bit memory");
        for (auto p = address / page_; p <= (address + length - 1) / page_; ++p)
            if (!pages_[p]) throw std::out_of_range("guest access to uncommitted page");
        return data_ + address;
    }
};
// One explicitly scoped test host per thread; no claim of Xbox threading support.
inline thread_local Memory* active_memory = nullptr;
struct MemoryScope {
    Memory* previous = active_memory;
    explicit MemoryScope(Memory& memory) { active_memory = &memory; }
    ~MemoryScope() { active_memory = previous; }
};
template<class T> T load(uint8_t* base, uint64_t address) {
    if (!active_memory) throw std::logic_error("no guest memory scope");
    auto p = active_memory->checked(base, address, sizeof(T));
    T result = 0;
    for (unsigned i = 0; i < sizeof(T); ++i) result = T((result << 8) | p[i]);
    return result;
}
template<class T> void store(uint8_t* base, uint64_t address, T value) {
    if (!active_memory) throw std::logic_error("no guest memory scope");
    auto p = active_memory->checked(base, address, sizeof(T));
    for (unsigned i = 0; i < sizeof(T); ++i) p[sizeof(T) - i - 1] = uint8_t(value >> (i * 8));
}
} // namespace acv_probe

// These override points are supplied by the generated ppc_context.h itself.
#define PPC_LOAD_U8(x) acv_probe::load<uint8_t>(base, (x))
#define PPC_LOAD_U16(x) acv_probe::load<uint16_t>(base, (x))
#define PPC_LOAD_U32(x) acv_probe::load<uint32_t>(base, (x))
#define PPC_LOAD_U64(x) acv_probe::load<uint64_t>(base, (x))
#define PPC_STORE_U8(x, y) acv_probe::store<uint8_t>(base, (x), (y))
#define PPC_STORE_U16(x, y) acv_probe::store<uint16_t>(base, (x), (y))
#define PPC_STORE_U32(x, y) acv_probe::store<uint32_t>(base, (x), (y))
#define PPC_STORE_U64(x, y) acv_probe::store<uint64_t>(base, (x), (y))
#define PPC_CALL_INDIRECT_FUNC(x) acv_probe::dispatch((x), ctx, base)
#include "ppc_context.h"

namespace acv_probe {
class Dispatcher {
    std::map<uint32_t, PPCFunc*> functions_;
public:
    void add(uint32_t va, PPCFunc* function) {
        if ((va & 3) || !function || !functions_.emplace(va, function).second)
            throw std::invalid_argument("invalid or duplicate guest function");
    }
    void call(uint64_t va, PPCContext& ctx, uint8_t* base) const {
        if (va > UINT32_MAX || (va & 3)) throw std::out_of_range("invalid guest function address");
        auto found = functions_.find(uint32_t(va));
        if (found == functions_.end()) throw std::out_of_range("unregistered guest function");
        found->second(ctx, base);
    }
};
inline thread_local Dispatcher* active_dispatcher = nullptr;
struct DispatcherScope {
    Dispatcher* previous = active_dispatcher;
    explicit DispatcherScope(Dispatcher& dispatcher) { active_dispatcher = &dispatcher; }
    ~DispatcherScope() { active_dispatcher = previous; }
};
inline void dispatch(uint64_t va, PPCContext& ctx, uint8_t* base) {
    if (!active_dispatcher) throw std::logic_error("no guest dispatcher scope");
    active_dispatcher->call(va, ctx, base);
}
} // namespace acv_probe
