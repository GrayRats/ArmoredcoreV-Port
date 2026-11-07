#!/usr/bin/env python3
"""Rebuild tmp/acv-native-adjusting-thunks.cpp from a previously compiled object of it.

generate_adjusting_thunks.py needs scratch inputs that are not kept (tmp/scan, older codegen output). The thunk
shape is fixed (lwz r3,4(r3); b target), so the address of each AcvThunk<addr> comes from the object's symbols and
its target sub_<addr> from the GOT64 relocation that follows the function's start.
Usage: recover_adjusting_thunks.py OBJECT.o OUTPUT.cpp
"""
import re, subprocess, sys
from pathlib import Path

obj, out = Path(sys.argv[1]), Path(sys.argv[2])
dump = subprocess.run(["objdump", "-dr", "-C", "--no-show-raw-insn", str(obj)], check=True, capture_output=True, text=True).stdout
thunks, current = {}, None
for line in dump.splitlines():
    header = re.match(r"^[0-9a-f]+ <AcvThunk([0-9A-F]{8})\(", line)
    if header:
        current = int(header.group(1), 16)
        continue
    if re.match(r"^[0-9a-f]+ <", line):
        current = None
        continue
    reloc = re.search(r"R_X86_64_GOT64\s+sub_([0-9A-F]{8})", line)
    if current is not None and reloc:
        thunks[current] = int(reloc.group(1), 16)
if not thunks:
    sys.exit("no thunks found in object")
lines = ["// Recovered by tools/native-rexglue/recover_adjusting_thunks.py from the compiled object; do not hand-edit.",
         "// Exact raw PPC translations only (lwz r3,4(r3); b target). Tail-target semantics remain unverified.",
         '#include "acv_linux_probe_pch.h"',
         "#include <rex/system/function_dispatcher.h>", "#include <stdexcept>"]
for target in sorted(set(thunks.values())):
    lines.append(f"REX_EXTERN(sub_{target:08X});")
for address, target in sorted(thunks.items()):
    lines += [f"// @retail 0x{address:08X}",
              f"static void AcvThunk{address:08X}(PPCContext& ctx, uint8_t* base) {{",
              "  ctx.r3.u64 = REX_LOAD_U32(ctx.r3.u32 + 4);",
              f"  sub_{target:08X}(ctx, base);", "}"]
lines += ["void AcvRegisterAdjustingThunks(rex::runtime::FunctionDispatcher& dispatcher) {"]
for address in sorted(thunks):
    lines += [f"  if (dispatcher.GetFunction(0x{address:08X}) != nullptr ||",
              f"      !dispatcher.SetFunction(0x{address:08X}, AcvThunk{address:08X}))",
              f'    throw std::runtime_error("ACV thunk 0x{address:08X} already exists or registration failed");']
lines += ["}"]
out.write_text("\n".join(lines) + "\n")
print(f"thunks={len(thunks)} targets={len(set(thunks.values()))} -> {out}")
