#!/usr/bin/env python3
"""JSON bridge for TRG; the existing profile/settings/probe code owns policy."""
import argparse, dataclasses, json, os, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"tools/native-rexglue"))
import acv_paths, acv_settings as S, acv_profiles as P, acv_shm, acv_install, acv_sessions

def snapshot(root, draft=None):
    path=root/"settings.json"
    saved=S.load_json(path,"settings") if path.exists() else {"schema_version":1,"graphics":{},"launcher":{}}
    if draft is not None: saved=normalize(root,draft)
    saved["graphics"]=S.resolve_graphics(saved.get("graphics",{}))
    saved.setdefault("launcher",{})
    saved["launcher"].setdefault("build",str(acv_paths.EXECUTABLE))
    saved["launcher"].setdefault("video_driver","x11")
    saved["launcher"].setdefault("allow_experimental",False)
    profiles=[dataclasses.asdict(p) for p in P.ProfileStore(root).list()]
    if profiles: saved["launcher"].setdefault("profile",profiles[0]["uuid"])
    exe=Path(saved["launcher"]["build"])
    features=S.runtime_features(exe.parent)
    caps=S.capabilities()
    for key, cap in caps.items():
        for option in cap["options"] or []:
            # An unrelated invalid selection must not disable every recovery control.
            candidate=S.resolve_graphics()
            dependencies={"wide_view": ("resolution",),
                          "color_depth": ("dither",), "dither": ("color_depth",)}
            for dependency in dependencies.get(key,()):
                candidate[dependency]=saved["graphics"][dependency]
            # Budget controls must be editable together before Save. Capability
            # checks use a compatible counterpart; Save validates the actual pair.
            if key == "texture_cache_soft_mb":
                candidate["texture_cache_hard_mb"]=max(S.GRAPHICS_SCHEMA["texture_cache_hard_mb"][1])
            elif key == "texture_cache_hard_mb":
                candidate["texture_cache_soft_mb"]=min(S.GRAPHICS_SCHEMA["texture_cache_soft_mb"][1])
            candidate[key]=option["value"]
            if key=="color_depth" and option["value"]==10: candidate["dither"]=False
            try:
                S.graphics_to_cvars(candidate, saved["launcher"]["allow_experimental"], features)
                option["selectable"]=True
            except S.SettingsError as e:
                option["selectable"]=False
                option.setdefault("reason",str(e))
    try:
        install=acv_install.verify(acv_paths.GAME,full=False)
    except (ValueError,OSError) as error:
        install={"ok":False,"reason":str(error)}
    return {"settings":saved,"profiles":profiles,"capabilities":caps,
            "resolution_presets":S.RESOLUTION_PRESETS,"bundled_build":str(acv_paths.EXECUTABLE),"presets":S.GRAPHICS_PRESETS,"features":sorted(S.runtime_features(exe.parent)),"build_exists":exe.is_file(),
            "game_data_exists":install["ok"],"installation":install,"game_root":str(acv_paths.GAME),
            "display_info":acv_sessions.latest(acv_paths.LOGS),
            "profiles_root":str(root),"settings_path":str(path),"logs_root":str(acv_paths.LOGS),"shm":acv_shm.status()}

def normalize(root,doc):
    if not isinstance(doc,dict):raise ValueError("settings must be an object")
    if type(doc.get("schema_version",1)) is not int or doc.get("schema_version",1)!=S.SCHEMA_VERSION:raise ValueError("Unsupported settings version")
    if not isinstance(doc.get("graphics",{}),dict):raise ValueError("graphics must be an object")
    if set(doc)-{"schema_version","graphics","launcher"}: raise ValueError("Unknown settings section")
    graphics=S.resolve_graphics(doc.get("graphics",{})); launcher=doc.get("launcher",{})
    if not isinstance(launcher,dict):raise ValueError("launcher must be an object")
    build=launcher.get("build",str(acv_paths.EXECUTABLE))
    if not isinstance(build,str) or not build:raise ValueError("build must be a non-empty path")
    exe=Path(build).expanduser().resolve()
    launcher=dict(launcher,build=str(exe))
    allow=launcher.get("allow_experimental",False)
    if not isinstance(allow,bool):raise ValueError("allow_experimental must be boolean")
    if launcher.get("video_driver","x11") not in ("x11","wayland","auto"):raise ValueError("Invalid video driver")
    profile=launcher.get("profile")
    if profile is not None and not isinstance(profile,str):raise ValueError("profile must be a UUID string")
    if profile:P.ProfileStore(root).get(profile)
    return dict(doc,schema_version=S.SCHEMA_VERSION,graphics=graphics,launcher=launcher)

def save(root,doc):
    doc=normalize(root,doc); graphics=doc["graphics"]; launcher=doc["launcher"]
    S.graphics_to_cvars(graphics,launcher.get("allow_experimental",False),S.runtime_features(Path(launcher["build"]).parent))
    old=S.load_json(root/"settings.json","settings") if (root/"settings.json").exists() else {}
    old.update(schema_version=S.SCHEMA_VERSION,graphics=graphics)
    old["launcher"]={**old.get("launcher",{}),**launcher}
    S.save_json_atomic(root/"settings.json",old)
    return {"ok":True}

def main():
    ap=argparse.ArgumentParser();ap.add_argument("--profiles-root",type=Path,default=acv_paths.PROFILES)
    ap.add_argument("command",choices=["snapshot","preview","save","reset-graphics","verify-install","publish-install"]);ap.add_argument("--staging",type=Path);a=ap.parse_args();root=a.profiles_root.resolve()
    try:
        if a.command=="snapshot":out=snapshot(root)
        elif a.command=="preview":out=snapshot(root,json.load(sys.stdin))
        elif a.command=="save":out=save(root,json.load(sys.stdin))
        elif a.command in ("verify-install","publish-install"):
            if a.staging is None:raise ValueError("staging is required")
            out=acv_install.verify(a.staging) if a.command=="verify-install" else acv_install.publish(a.staging,acv_paths.GAME)
        else:
            doc=snapshot(root)["settings"];doc["graphics"]=S.resolve_graphics();out=save(root,doc)
        print(json.dumps(out,ensure_ascii=False));return 0
    except (ValueError,OSError,P.ProfileBusy,P.BackupError,S.SettingsError,KeyError) as e:
        print(json.dumps({"error":str(e)},ensure_ascii=False));return 1
if __name__=="__main__":sys.exit(main())
