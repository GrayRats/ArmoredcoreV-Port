// Host test for rex::filesystem::ShardedArchive (no game data, no GPU).
#include <rex/filesystem/devices/sharded_archive.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;
using rex::filesystem::ShardedArchive;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

static void le(std::string& out, uint64_t value, int bytes) {
  for (int i = 0; i < bytes; ++i) out.push_back(char((value >> (8 * i)) & 0xFF));
}
struct Spec { std::string shard; uint64_t shard_size; };
struct Ext { uint64_t voff; uint32_t size; uint16_t shard; uint64_t soff; };

static std::string make_index(uint64_t virtual_size, const std::vector<Spec>& shards, const std::vector<Ext>& extents) {
  std::string out("ACVSHD01", 8);
  le(out, 1, 4); le(out, 2048, 4); le(out, virtual_size, 8); le(out, shards.size(), 4); le(out, extents.size(), 4);
  for (auto& s : shards) { le(out, s.shard.size(), 2); out += s.shard; le(out, s.shard_size, 8); }
  for (auto& e : extents) { le(out, e.voff, 8); le(out, e.size, 4); le(out, e.shard, 2); le(out, 0, 2); le(out, e.soff, 8); }
  return out;
}
static void write(const fs::path& path, const std::string& data) { std::ofstream(path, std::ios::binary) << data; }

// Virtual file layout (size 10000): [0,100) hole, A=[100,1100) shard0@0, B=[1100,1600) shard1@4096, hole [1600,5000), C=[5000,9000) shard0@1000.
// Expected byte at virtual position p: A -> 'a'+(p-100)%7, B -> 'b'+(p-1100)%5, C -> 'c'+(p-5000)%3 ; holes -> 0.
static unsigned char expected(uint64_t p) {
  if (p >= 100 && p < 1100) return 'a' + (p - 100) % 7;
  if (p >= 1100 && p < 1600) return 'b' + (p - 1100) % 5;
  if (p >= 5000 && p < 9000) return 'c' + (p - 5000) % 3;
  return 0;
}

int main() {
  fs::path dir = fs::temp_directory_path() / ("acv-shard-test-" + std::to_string(std::rand()));
  fs::create_directories(dir);
  std::string s0(5000, '\0'), s1(5000, '\0');
  for (uint64_t i = 0; i < 1000; ++i) s0[i] = char('a' + i % 7);
  for (uint64_t i = 0; i < 4000; ++i) s0[1000 + i] = char('c' + i % 3);
  for (uint64_t i = 0; i < 500; ++i) s1[4096 + i] = char('b' + i % 5);
  write(dir / "s0.pak", s0); write(dir / "s1.pak", s1);
  const std::vector<Spec> shards{{"s0.pak", 5000}, {"s1.pak", 5000}};
  const std::vector<Ext> extents{{100, 1000, 0, 0}, {1100, 500, 1, 4096}, {5000, 4000, 0, 1000}};
  write(dir / "shards.idx", make_index(10000, shards, extents));

  std::string error;
  auto archive = ShardedArchive::Load(dir / "shards.idx", &error);
  CHECK(archive && error.empty());
  if (!archive) return 1;
  CHECK(archive->size() == 10000 && archive->shard_count() == 2 && archive->extent_count() == 3);

  auto check_read = [&](uint64_t offset, size_t length, size_t want) {
    std::vector<unsigned char> buffer(length + 8, 0xEE);
    size_t got = 12345;
    bool ok = archive->Read(offset, buffer.data(), length, &got);
    CHECK(ok); CHECK(got == want);
    for (size_t i = 0; i < got; ++i) if (buffer[i] != expected(offset + i)) { CHECK(buffer[i] == expected(offset + i)); break; }
    for (size_t i = got; i < buffer.size(); ++i) if (buffer[i] != 0xEE) { CHECK(buffer[i] == 0xEE); break; }  // no overrun
  };
  check_read(100, 1000, 1000);        // exactly one extent
  check_read(150, 200, 200);          // inside one extent
  check_read(1000, 300, 300);         // across A -> B (contiguous)
  check_read(0, 100, 100);            // leading hole -> zeros
  check_read(1500, 3600, 3600);       // B -> hole -> C
  check_read(8900, 5000, 1100);       // clamped at the virtual end
  check_read(0, 10000, 10000);        // whole file
  check_read(10000, 16, 0);           // at end
  check_read(20000, 16, 0);           // past end
  check_read(9999, 1, 1);

  // Concurrent readers.
  std::atomic<int> bad{0};
  std::vector<std::thread> threads;
  for (int t = 0; t < 8; ++t) threads.emplace_back([&, t] {
    for (int i = 0; i < 2000; ++i) {
      uint64_t off = (uint64_t(t) * 977 + uint64_t(i) * 131) % 9800; unsigned char b[200]; size_t got = 0;
      if (!archive->Read(off, b, sizeof b, &got) || got != 200) { ++bad; continue; }
      for (size_t k = 0; k < got; ++k) if (b[k] != expected(off + k)) { ++bad; break; }
    }
  });
  for (auto& th : threads) th.join();
  CHECK(bad == 0);

  // Rejections.
  auto expect_reject = [&](const char* name, const std::string& data) {
    write(dir / "bad.idx", data);
    std::string why; CHECK(!ShardedArchive::Load(dir / "bad.idx", &why)); CHECK(!why.empty());
    std::fprintf(stderr, "  reject %-24s -> %s\n", name, why.c_str());
  };
  expect_reject("missing shard", make_index(10000, {{"nope.pak", 5000}}, {}));
  expect_reject("wrong shard size", make_index(10000, {{"s0.pak", 4999}}, {}));
  expect_reject("path traversal", make_index(10000, {{"../s0.pak", 5000}}, {}));
  expect_reject("overlapping extents", make_index(10000, shards, {{100, 1000, 0, 0}, {900, 100, 0, 0}}));
  expect_reject("extent past shard", make_index(10000, shards, {{100, 1000, 0, 4500}}));
  expect_reject("extent past virtual end", make_index(500, shards, {{100, 1000, 0, 0}}));
  expect_reject("bad shard number", make_index(10000, shards, {{100, 10, 7, 0}}));
  expect_reject("bad magic", "XXXXXXXX" + make_index(1, shards, {}).substr(8));
  expect_reject("truncated", make_index(10000, shards, extents).substr(0, 60));
  CHECK(!ShardedArchive::Load(dir / "does-not-exist.idx"));

  fs::remove_all(dir);
  if (failures) { std::fprintf(stderr, "%d failure(s)\n", failures); return 1; }
  std::puts("sharded_archive_test: PASS");
  return 0;
}
