// Optional bounded diagnostic, compiled only into the private upload-probe plugin.
// Records CPU-origin staging bytes immediately after the physical-memory memcpy.
#pragma once
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>

namespace {
struct AcvUploadProbeConfig {
  const char* directory = nullptr;
  uint32_t start = 0, length = 0;
  uint32_t remaining = 32;
  AcvUploadProbeConfig() {
    const char* dir = std::getenv("ACV_GPU_UPLOAD_PROBE_DIR");
    const char* start_text = std::getenv("ACV_GPU_UPLOAD_PROBE_START");
    const char* length_text = std::getenv("ACV_GPU_UPLOAD_PROBE_LENGTH");
    if (!dir || !*dir || !start_text || !length_text) return;
    auto number = [](const char* text, unsigned long long& value) {
      char* end = nullptr;
      errno = 0;
      value = std::strtoull(text, &end, 0);
      return !errno && end != text && !*end && *text != '-';
    };
    unsigned long long first, count;
    if (!number(start_text, first) || !number(length_text, count) ||
        !count || count > (8u << 20) || first >= (1u << 29) ||
        count > (1u << 29) - first) {
      std::fprintf(stderr, "[acv-upload-probe] Invalid physical range; disabled\n");
      return;
    }
    start = uint32_t(first); length = uint32_t(count); directory = dir;
  }
};
void AcvRecordUpload(const uint8_t* staging, uint32_t physical_start,
                     uint64_t bytes, uint64_t submission, VkBuffer source_buffer,
                     VkDeviceSize source_offset) {
  // UploadRanges is executed by the GPU command processor thread.
  static AcvUploadProbeConfig config;
  if (!config.directory || !config.remaining) return;
  const uint64_t first = std::max(uint64_t(physical_start), uint64_t(config.start));
  const uint64_t end = std::min(uint64_t(physical_start) + bytes,
                                uint64_t(config.start) + config.length);
  if (first >= end) return;
  const auto sequence = 33 - config.remaining--;
  char filename[160];
  std::snprintf(filename, sizeof(filename), "upload_%02u_sub%llu_phys%08llx_len%llu",
                sequence, (unsigned long long)submission, (unsigned long long)first,
                (unsigned long long)(end-first));
  const auto base = std::filesystem::path(config.directory) / filename;
  // The directory is prepared by the launcher. Do not create arbitrary parents.
  std::ofstream raw(base.string()+".bin", std::ios::binary);
  raw.write(reinterpret_cast<const char*>(staging + first-physical_start), end-first);
  raw.close();
  std::ofstream metadata(base.string()+".json");
  metadata << "{\"sequence\":" << sequence << ",\"submission\":" << submission
      << ",\"physical_start\":" << first << ",\"bytes\":" << end-first
      << ",\"upload_buffer_handle\":" << (uint64_t)(uintptr_t)source_buffer
      << ",\"upload_buffer_offset\":" << source_offset+first-physical_start
      << ",\"scope\":\"CPU-origin staging snapshot before deferred copy; not later live CPU memory\"}\n";
  metadata.close();
  if (!raw || !metadata) {
    std::fprintf(stderr,"[acv-upload-probe] Snapshot write failed; disabling further snapshots\n");
    config.remaining=0;
    return;
  }
  std::fprintf(stderr,"[acv-upload-probe] %s\n",filename);
}
} // namespace
