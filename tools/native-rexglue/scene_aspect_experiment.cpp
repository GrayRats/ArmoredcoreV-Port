// Diagnostic-only perspective experiment. Disabled without the explicit env var.
// Retail proof: logs/acv-ultrawide-20261024/822B4428.txt, call at 0x822B450C.
// Native override ABI: generated/default/acv_linux_probe_pch.h DEFINE_REX_FUNC;
// weak sub_* alias may be overridden; __imp__sub_* always calls original code.
#include "acv_linux_probe_pch.h"
#include <rex/logging.h>
#include <algorithm>
#include <array>
#include <bit>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <mutex>
#include <string_view>

REX_EXTERN(__imp__sub_82BC8980);
REX_EXTERN(__imp__sub_82BC8CC0);

// Read-only census of the projection helpers (ACV_PROJECTION_CENSUS=1): calls, argument ranges and the
// return address of every call site, logged every 10 s. It never changes guest registers or memory.
namespace {
// Tracks only the current native camera build on this thread; no persistent guest pointer.
thread_local bool* g_camera_projection_expanded = nullptr;

struct CensusRow {
  uint64_t count = 0;
  double lo[6], hi[6];
};
std::mutex g_census_mutex;
std::map<std::pair<int, uint32_t>, CensusRow> g_census;  // (0 perspective / 1 orthographic, LR)
std::chrono::steady_clock::time_point g_census_next{};

bool CensusEnabled() {
  static const bool on = [] { const char* v = std::getenv("ACV_PROJECTION_CENSUS"); return v && *v == '1'; }();
  return on;
}

void CensusRecord(int kind, uint32_t lr, const double* args, int n) {
  std::lock_guard<std::mutex> lock(g_census_mutex);
  auto [it, inserted] = g_census.try_emplace({kind, lr});
  CensusRow& row = it->second;
  for (int i = 0; i < n; ++i) {
    row.lo[i] = inserted || row.count == 0 ? args[i] : std::min(row.lo[i], args[i]);
    row.hi[i] = inserted || row.count == 0 ? args[i] : std::max(row.hi[i], args[i]);
  }
  ++row.count;
  const auto now = std::chrono::steady_clock::now();
  if (g_census_next == std::chrono::steady_clock::time_point{}) g_census_next = now + std::chrono::seconds(10);
  if (now < g_census_next) return;
  g_census_next = now + std::chrono::seconds(10);
  for (const auto& [key, r] : g_census) {
    if (key.first == 0) {
      REXLOG_INFO("[acv-census] perspective lr={:08X} n={} fov=[{:.4f},{:.4f}] aspect=[{:.4f},{:.4f}] near=[{:.3f},{:.3f}] far=[{:.1f},{:.1f}]",
                  key.second, r.count, r.lo[0], r.hi[0], r.lo[1], r.hi[1], r.lo[2], r.hi[2], r.lo[3], r.hi[3]);
    } else {
      REXLOG_INFO("[acv-census] ortho lr={:08X} n={} left=[{:.1f},{:.1f}] right=[{:.1f},{:.1f}] bottom=[{:.1f},{:.1f}] top=[{:.1f},{:.1f}] near=[{:.2f},{:.2f}] far=[{:.1f},{:.1f}]",
                  key.second, r.count, r.lo[0], r.hi[0], r.lo[1], r.hi[1], r.lo[2], r.hi[2], r.lo[3], r.hi[3],
                  r.lo[4], r.hi[4], r.lo[5], r.hi[5]);
    }
  }
}
}  // namespace

// @retail 0x82BC8CC0: orthographic projection, bounds/near/far in f1..f6.
// Caller proof: decompilation/evidence/acv/822B52D8-20261027T145443.871783Z.
static double ExperimentalHudAspect() {
  static const double value = [] {
    const char* text = std::getenv("ACV_EXPERIMENTAL_HUD_ASPECT");
    if (!text || !*text) return 0.0;
    char* end = nullptr;
    const double parsed = std::strtod(text, &end);
    if (end == text || *end || !std::isfinite(parsed) || parsed < 1.0 || parsed > 4.0) return 0.0;
    REXLOG_WARN("[acv-aspect-experiment] HUD centring for aspect={}; ortho 0x822B5368 only", parsed);
    return parsed;
  }();
  return value;
}

REX_EXTERN(sub_82BC8CC0) {
  // Retail 0x822B52D8 derives this rectangle from its viewport, not a fixed
  // 1280x720 constant. Only the measured main HUD site and native 16:9
  // rectangles qualify; other orthographic passes stay unchanged.
  if (CensusEnabled()) {
    const double args[6] = {ctx.f1.f64, ctx.f2.f64, ctx.f3.f64,
                            ctx.f4.f64, ctx.f5.f64, ctx.f6.f64};
    CensusRecord(1, uint32_t(ctx.lr), args, 6);
  }
  const double hud = ExperimentalHudAspect();
  const double left = ctx.f1.f64, right = ctx.f2.f64;
  const double bottom = ctx.f3.f64, top = ctx.f4.f64;
  const double width = right - left, height = top - bottom;
  const bool selected = hud != 0.0 && ctx.lr == 0x822B5368 &&
      left == 0.0 && top == 0.0 &&
      std::isfinite(width) && std::isfinite(height) && width > 0.0 && height > 0.0 &&
      std::abs(width / height - 16.0 / 9.0) < 1e-5;
  if (!selected) {
    __imp__sub_82BC8CC0(ctx, base);
    return;
  }
  struct RestoreHudInputs {
    PPCContext& context;
    double left, right, bottom, top;
    ~RestoreHudInputs() {
      context.f1.f64 = left; context.f2.f64 = right;
      context.f3.f64 = bottom; context.f4.f64 = top;
    }
  } restore{ctx, left, right, bottom, top};
  if (hud >= width / height) {
    const double extra = (height * hud - width) / 2.0;
    ctx.f1.f64 = left - extra;
    ctx.f2.f64 = right + extra;
  } else {
    const double extra = (width / hud - height) / 2.0;
    ctx.f3.f64 = bottom - extra;
    ctx.f4.f64 = top + extra;
  }
  // The symmetric rectangle keeps the reticle centre and the native HUD
  // proportions. World-projected target markers need separate guest proof.
  __imp__sub_82BC8CC0(ctx, base);
}

static double ExperimentalAspect() {
  static const double value = [] {
    const char* text = std::getenv("ACV_EXPERIMENTAL_SCENE_ASPECT");
    if (!text || !*text) return 0.0;
    char* end = nullptr;
    const double parsed = std::strtod(text, &end);
    if (end == text || *end || !std::isfinite(parsed) || parsed < 1.0 || parsed > 4.0) {
      REXLOG_WARN("[acv-aspect-experiment] Invalid aspect; experiment disabled");
      return 0.0;
    }
    REXLOG_WARN("[acv-aspect-experiment] Enabled aspect={}; projection only, HUD/culling/output geometry unverified", parsed);
    return parsed;
  }();
  return value;
}

// Census of 2026-10-27 (logs/acv-native-linux/20261026T193756597623Z): two perspective call sites carry the 16:9 view
// (FOV 0.52-1.05 rad, far 13000): 0x822B4510 (3981 calls) and the generic builder 0x82300978 (29049 calls, aspects
// 16:9 and 2:1). Only calls whose original aspect is 16:9 are widened, so 2:1 passes and unseen sites stay untouched.
static bool IsSceneCameraSite(uint32_t lr) { return lr == 0x822B4510 || lr == 0x82300978; }

// @retail 0x82BC8980
// Change the 16:9 perspective calls of the two measured sites; other callers pass through.
// This does not claim a complete UltraWide implementation. Do not publish as one.
REX_EXTERN(sub_82BC8980) {
  if (CensusEnabled()) {
    const double a[4] = {ctx.f1.f64, ctx.f2.f64, ctx.f3.f64, ctx.f4.f64};  // fov, aspect, near, far
    CensusRecord(0, uint32_t(ctx.lr), a, 4);
  }
  const double aspect = ExperimentalAspect();
  const double original = ctx.f2.f64;
  const bool selected = aspect != 0.0 && IsSceneCameraSite(uint32_t(ctx.lr)) &&
      std::isfinite(original) && std::abs(original - 16.0 / 9.0) < 1e-5 &&
      std::isfinite(ctx.f1.f64) && ctx.f1.f64 > 0.0 && ctx.f1.f64 < 3.141592653589793;
  if (!selected) {
    __imp__sub_82BC8980(ctx, base);
    return;
  }
  // The render target is stretched over the output, so only the projection aspect against the SCREEN matters.
  // Wider than 16:9: keep the vertical FOV, widen x (horizontal view grows). Narrower (16:10, 4:3): keep the
  // horizontal view and enlarge the vertical FOV: tan(fov'/2) = tan(fov/2) * (16/9) / aspect.
  const double fov = ctx.f1.f64;
  double new_fov = fov;
  if (aspect < 16.0 / 9.0) {
    new_fov = 2.0 * std::atan(std::tan(fov / 2.0) * (16.0 / 9.0) / aspect);
    if (!(new_fov > 0.0 && new_fov < 3.141592653589793)) {
      __imp__sub_82BC8980(ctx, base);
      return;
    }
  }
  const uint32_t caller = uint32_t(ctx.lr);
  static std::atomic<unsigned> samples[2]{};
  const size_t camera = caller == 0x822B4510 ? 0 : 1;
  if (samples[camera].fetch_add(1, std::memory_order_relaxed) < 2) {
    REXLOG_INFO("[acv-aspect-experiment] lr={:08X} old_aspect={} new_aspect={} old_fov={} new_fov={} near={} far={}",
                caller, original, aspect, fov, new_fov, ctx.f3.f64, ctx.f4.f64);
  }
  struct RestorePerspectiveInputs {
    PPCContext& context;
    double fov, aspect;
    ~RestorePerspectiveInputs() {
      context.f1.f64 = fov;
      context.f2.f64 = aspect;
    }
  } restore{ctx, fov, original};
  if (g_camera_projection_expanded && caller==0x82300978u && aspect>original)
    *g_camera_projection_expanded=true;
  ctx.f1.f64 = new_fov;
  ctx.f2.f64 = aspect;
  __imp__sub_82BC8980(ctx, base);
}


// @retail 0x82302520: perspective camera builder. It writes local side planes
// at +0x60/+0x70 before 0x82300948 invokes the projection hook. Consequently
// local sphere tests (0x82300CE0) can still reject objects inside the widened
// rendered view. The derived world planes already use the hooked projection.
// Raw evidence: decompilation/evidence/acv/82302520-20261028T010652.093791Z;
// local-plane consumer: .../82300CE0-20261028T010747.112364Z.
REX_EXTERN(__imp__sub_82302520);

namespace {
bool NativeFrustumSyncEnabled() {
  static const bool enabled = [] {
    const char* value = std::getenv("ACV_NATIVE_FRUSTUM_SYNC");
    return !value || std::string_view(value) != "0";
  }();
  return enabled;
}

float CameraFloat(uint8_t* base, uint32_t address) {
  return std::bit_cast<float>(uint32_t(REX_LOAD_U32(address)));
}
void CameraStoreFloat(uint8_t* base, uint32_t address, float value) {
  REX_STORE_U32(address, std::bit_cast<uint32_t>(value));
}

// All fields are checked before any write. Only the four horizontal normal
// coefficients change. No allocation, reference count, viewport or IO changes.
bool SyncLocalSidePlanes(uint8_t* base, uint32_t camera, double original_fov,
                         double target_aspect) {
  if (!base || (camera & 3u) || camera > UINT32_MAX - 0x80u) return false;
  std::array<float, 8> planes{};
  for (unsigned i=0; i<8; ++i) planes[i]=CameraFloat(base,camera+0x60u+4u*i);
  const double slope=std::tan(original_fov/2.0)*(16.0/9.0);
  if (!std::isfinite(slope) || slope<=0.0) return false;
  for (unsigned side=0; side<2; ++side) {
    const float x=planes[side*4], y=planes[side*4+1];
    const float z=planes[side*4+2], d=planes[side*4+3];
    if (!std::isfinite(x) || !std::isfinite(z) || y!=0.0f || d!=0.0f ||
        (side==0 ? x>=0.0f : x<=0.0f) || z>=0.0f ||
        std::abs(double(x)*x+double(z)*z-1.0)>1e-4 ||
        std::abs((-double(z)/std::abs(double(x)))/slope-1.0)>1e-4) return false;
  }
  std::array<float,4> corrected{};
  for (unsigned side=0; side<2; ++side) {
    const double x=planes[side*4];
    const double z=double(planes[side*4+2])*target_aspect/(16.0/9.0);
    const double norm=std::hypot(x,z);
    corrected[side*2]=float(x/norm);corrected[side*2+1]=float(z/norm);
  }
  CameraStoreFloat(base,camera+0x60,corrected[0]);
  CameraStoreFloat(base,camera+0x68,corrected[1]);
  CameraStoreFloat(base,camera+0x70,corrected[2]);
  CameraStoreFloat(base,camera+0x78,corrected[3]);
  return true;
}

void FrustumTrace(uint32_t caller,uint32_t camera,bool eligible,bool patched) {
  static const bool enabled=[] {
    const char* value=std::getenv("ACV_FRUSTUM_TRACE");return value && *value=='1';
  }();
  if (!enabled) return;
  using Clock=std::chrono::steady_clock;
  static const auto start=Clock::now();
  struct Stats {uint64_t calls=0,eligible=0,patched=0;Clock::time_point next{};};
  static thread_local Stats stats;
  const auto now=Clock::now();
  const auto age=std::chrono::duration_cast<std::chrono::milliseconds>(now-start).count();
  if (age<90000) return;
  ++stats.calls;stats.eligible+=eligible;stats.patched+=patched;
  if (stats.next==Clock::time_point{}) stats.next=now+std::chrono::seconds(10);
  if (now<stats.next) return;
  stats.next=now+std::chrono::seconds(10);
  REXLOG_INFO("[acv-frustum] age_ms={} calls={} eligible={} patched={} lr={:08X} camera={:08X}",
              age,stats.calls,stats.eligible,stats.patched,caller,camera);
}
}  // namespace

REX_EXTERN(sub_82302520) {
  const uint32_t camera=ctx.r3.u32, caller=uint32_t(ctx.lr);
  const uint32_t width=ctx.r8.u32,height=ctx.r9.u32;
  const double fov=ctx.f1.f64,aspect=ExperimentalAspect();
  // Scope the correction to the camera whose projection was actually widened
  // during this build. Retail has additional callers in VMX listing gaps;
  // a guessed outer-caller whitelist would miss the main rendering path.
  struct ProjectionScope {
    bool expanded=false;
    bool* previous=g_camera_projection_expanded;
    ProjectionScope(){g_camera_projection_expanded=&expanded;}
    ~ProjectionScope(){g_camera_projection_expanded=previous;}
  } scope;
  __imp__sub_82302520(ctx,base);
  const bool eligible=scope.expanded && width!=0 && height!=0 &&
      std::abs(double(width)/height-16.0/9.0)<1e-5 &&
      aspect>16.0/9.0 && std::isfinite(fov) && fov>0.0 && fov<3.141592653589793;
  const bool patched=eligible && NativeFrustumSyncEnabled() &&
      SyncLocalSidePlanes(base,camera,fov,aspect);
  FrustumTrace(caller,camera,eligible,patched);
}
