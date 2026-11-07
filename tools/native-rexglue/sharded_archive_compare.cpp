// Compare a ShardedArchive with the original archive: full sequential pass plus random reads.
#include <rex/filesystem/devices/sharded_archive.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <vector>
using rex::filesystem::ShardedArchive;
int main(int argc, char** argv) {
  if (argc < 3) { std::fprintf(stderr, "usage: %s shards.idx dvdbnd.bdt [random_reads]\n", argv[0]); return 2; }
  std::string error;
  auto archive = ShardedArchive::Load(argv[1], &error);
  if (!archive) { std::fprintf(stderr, "load failed: %s\n", error.c_str()); return 1; }
  std::ifstream original(argv[2], std::ios::binary);
  original.seekg(0, std::ios::end);
  const uint64_t size = original.tellg();
  if (size != archive->size()) { std::fprintf(stderr, "size mismatch %llu vs %llu\n", (unsigned long long)size, (unsigned long long)archive->size()); return 1; }
  uint64_t mismatching = 0, first_bad = UINT64_MAX;
  std::vector<char> a(1 << 20), b(1 << 20);
  auto compare = [&](uint64_t offset, size_t length) {
    original.clear(); original.seekg(offset); original.read(b.data(), length);
    size_t got = 0;
    if (!archive->Read(offset, a.data(), length, &got) || got != length || size_t(original.gcount()) != length) { ++mismatching; if (offset < first_bad) first_bad = offset; return; }
    for (size_t i = 0; i < length; ++i) if (a[i] != b[i]) { ++mismatching; if (offset + i < first_bad) first_bad = offset + i; break; }
  };
  for (uint64_t offset = 0; offset < size; offset += a.size()) compare(offset, size_t(std::min<uint64_t>(a.size(), size - offset)));
  std::printf("sequential pass: %llu mismatching chunk(s)", (unsigned long long)mismatching);
  if (mismatching) std::printf(", first bad byte at %llu", (unsigned long long)first_bad);
  std::printf("\n");
  const int randoms = argc > 3 ? std::atoi(argv[3]) : 20000;
  std::mt19937_64 rng(12345);
  uint64_t before = mismatching;
  for (int i = 0; i < randoms; ++i) {
    size_t length = 1 + rng() % 65536; uint64_t offset = rng() % size;
    length = size_t(std::min<uint64_t>(length, size - offset)); compare(offset, length);
  }
  std::printf("random reads: %d, mismatching %llu\n", randoms, (unsigned long long)(mismatching - before));
  std::printf("extents=%zu shards=%zu size=%llu\n", archive->extent_count(), archive->shard_count(), (unsigned long long)size);
  return mismatching ? 1 : 0;
}
