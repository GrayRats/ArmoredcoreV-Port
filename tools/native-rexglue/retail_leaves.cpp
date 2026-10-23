// Instruction-derived ACV leaves missing from independent ReXGlue discovery.
// Evidence: logs/acv-native-linux/20261020T233032290883Z/runtime.log;
// original PPC listing recorded by operation d5608098-929d-4560-91ac-fd634ae0ef9d.
#include "acv_linux_probe_pch.h"
#include <rex/system/function_dispatcher.h>
#include <stdexcept>

// @retail 0x82CBB8A0
// Confirmed instruction translation only; semantic purpose remains unknown.
// [0x82CBB8A0,0x82CBB8AC): lis r11,-31856; addi r3,r11,-13137; blr.
static void AcvLeaf82CBB8A0(PPCContext& ctx, uint8_t*) {
  ctx.r11.s64 = -2087714816;
  ctx.r3.s64 = ctx.r11.s64 - 13137;
}

REX_EXTERN(sub_82350B58);
void AcvRegisterAdjustingThunks(rex::runtime::FunctionDispatcher& dispatcher);

// @retail 0x82350E88
// Confirmed two-instruction adjusting thunk, not a semantic reconstruction
// of its existing generated tail target. lwz r3,4(r3); b 0x82350B58.
static void AcvLeaf82350E88(PPCContext& ctx, uint8_t* base) {
  ctx.r3.u64 = REX_LOAD_U32(ctx.r3.u32 + 4);
  sub_82350B58(ctx, base);  // b preserves guest LR.
}

// @retail 0x82CEB570
// Confirmed three-instruction leaf, reached through a data pointer at
// 0x82116F7C/0x8211CCE4/0x8211CD54. Raw words 0x81630004 0x806B0004 0x4E800020:
// lwz r11,4(r3); lwz r3,4(r11); blr. Semantic purpose remains unknown.
static void AcvLeaf82CEB570(PPCContext& ctx, uint8_t* base) {
  ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 4);
  ctx.r3.u64 = REX_LOAD_U32(ctx.r11.u32 + 4);
}

void AcvRegisterRetailLeaves(rex::runtime::FunctionDispatcher& dispatcher) {
  // SDK: include/rex/system/function_dispatcher.h:85, public registration API.
  if (dispatcher.GetFunction(0x82CEB570) != nullptr ||
      !dispatcher.SetFunction(0x82CEB570, AcvLeaf82CEB570)) {
    throw std::runtime_error("ACV leaf 0x82CEB570 already registered or registration failed");
  }
  if (dispatcher.GetFunction(0x82CBB8A0) != nullptr) {
    throw std::runtime_error("ACV leaf already registered; review generated discovery");
  }
  if (!dispatcher.SetFunction(0x82CBB8A0, AcvLeaf82CBB8A0)) {
    throw std::runtime_error("ACV leaf address outside module function table");
  }
  if (dispatcher.GetFunction(0x82350E88) != nullptr ||
      !dispatcher.SetFunction(0x82350E88, AcvLeaf82350E88)) {
    throw std::runtime_error("ACV adjusting thunk registration failed or already exists");
  }
  AcvRegisterAdjustingThunks(dispatcher);
}
