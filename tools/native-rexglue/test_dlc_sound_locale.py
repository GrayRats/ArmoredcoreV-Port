#!/usr/bin/env python3
"""Verify DLC sound names expand from retail VoiceLang before BHD lookup."""
import runpy

module = runpy.run_path("tools/build_dlc_sound_dir.py")
assert module["configured_voice_language"]() == "jp"
resolve = module["resolve_voice_language"]
assert resolve("acv_talk_$(voicelang).fev", "jp") == "acv_talk_jp.fev"
assert resolve("acv_voice_ch09.fev", "jp") == "acv_voice_ch09.fev"
assert resolve("acv_talk_$(VoiceLang).fev", None) is None
print("PASS: literal DLC voice token resolves to the retail-configured locale")
