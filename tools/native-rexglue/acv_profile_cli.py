#!/usr/bin/env python3
"""Profile management CLI used by the launcher and by hand. Every command prints one JSON document."""
import argparse, dataclasses, json, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import acv_profiles as P
import acv_shm
import acv_paths
import acv_settings as S

ROOT = Path(__file__).resolve().parents[2]


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--profiles-root", type=Path, default=acv_paths.PROFILES)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("list")
    sub.add_parser("capabilities")
    sub.add_parser("last-run", help="summary of the newest launch: output, pacing, errors")
    sub.add_parser("shm-status")
    sub.add_parser("shm-clean", help="remove orphaned xenia_memory_* files not mapped by a live process")
    ft = sub.add_parser("features", help="runtime features of a staged build")
    ft.add_argument("--executable", type=Path)
    sn = sub.add_parser("set-names"); sn.add_argument("uuid")
    sn.add_argument("--gamertag"); sn.add_argument("--pilot")
    ck = sub.add_parser("check", help="validate saved settings against a build before launching")
    ck.add_argument("--settings", type=Path, required=True)
    ck.add_argument("--executable", type=Path)
    ck.add_argument("--allow-experimental", action="store_true")
    c = sub.add_parser("create"); c.add_argument("name"); c.add_argument("--gamertag"); c.add_argument("--pilot")
    r = sub.add_parser("rename"); r.add_argument("uuid"); r.add_argument("name")
    m = sub.add_parser("migrate-legacy", help="create a profile from acv.config [profile], optionally importing a user_data_root")
    m.add_argument("--config", type=Path, default=ROOT / "tools/native-rexglue/acv.config")
    m.add_argument("--userdata", type=Path)
    for name in ("backup", "backups"):
        b = sub.add_parser(name); b.add_argument("uuid")
    rs = sub.add_parser("restore"); rs.add_argument("uuid"); rs.add_argument("backup", type=Path)
    i = sub.add_parser("import"); i.add_argument("uuid"); i.add_argument("source", type=Path)
    a = ap.parse_args(argv)
    store = P.ProfileStore(a.profiles_root.resolve())
    try:
        if a.cmd == "last-run":
            import json as _json
            runs = sorted((ROOT / "logs/acv-native-linux").glob("2*"))
            if not runs:
                out = {"error": "no launch evidence found"}
            else:
                run = runs[-1]
                launch = _json.loads((run / "launch.json").read_text()) if (run / "launch.json").is_file() else {}
                display = launch.get("display", {})
                pacing = display.get("pacing", [])
                out = {"evidence": str(run), "child_returncode": launch.get("child_returncode"),
                       "fatal_error_count": launch.get("fatal_error_count", 0),
                       "output": display.get("output"), "applied": display.get("applied"),
                       "pacing_last": pacing[-1] if pacing else None,
                       "limit_reached": launch.get("limit_reached")}
        elif a.cmd == "shm-status":
            out = acv_shm.status()
        elif a.cmd == "shm-clean":
            removed = acv_shm.clean()
            out = {"removed": len(removed), "freed_bytes": sum(r["allocated"] for r in removed)}
        elif a.cmd == "features":
            exe = a.executable or acv_paths.EXECUTABLE
            data = exe.parent / "librexruntime.so"
            out = {"executable": str(exe), "exists": exe.is_file(),
                   "features": sorted(S.runtime_features(exe.parent)),
                   "runtime_sha256": P._digest(data) if data.is_file() else None,
                   "game_data": (acv_paths.GAME / "default.xex").is_file()}
        elif a.cmd == "set-names":
            out = dataclasses.asdict(store.set_names(a.uuid, a.gamertag, a.pilot))
        elif a.cmd == "check":
            exe_dir = a.executable.parent if a.executable else acv_paths.EXECUTABLE.parent
            saved = S.load_json(a.settings, "settings")
            S.graphics_to_cvars(saved.get("graphics", {}), a.allow_experimental, S.runtime_features(exe_dir))
            out = {"ok": True}
        elif a.cmd == "capabilities":
            out = S.capabilities()
        elif a.cmd == "list":
            out = [dataclasses.asdict(p) for p in store.list()]
        elif a.cmd == "create":
            out = dataclasses.asdict(store.create(a.name, gamertag=a.gamertag, pilot_name=a.pilot))
        elif a.cmd == "rename":
            out = dataclasses.asdict(store.rename(a.uuid, a.name))
        elif a.cmd == "migrate-legacy":
            profile = S.migrate_legacy_config(a.config)
            S.save_profile(store.root, profile)
            store.userdata(profile.uuid).mkdir(parents=True)
            out = dataclasses.asdict(profile)
            has_saves = a.userdata and any(f.is_file() for f in a.userdata.rglob("*")
                                           if "cache" not in f.relative_to(a.userdata).parts[:1])
            if has_saves:
                store.import_saves(profile.uuid, a.userdata)  # copies; the source tree stays untouched
            elif a.userdata:
                out["imported"] = False  # legacy directory only held cache: nothing to carry over
        elif a.cmd == "backup":
            with store.lock(a.uuid):
                out = {"backup": str(store.backup(a.uuid))}
        elif a.cmd == "backups":
            out = [{"path": str(b), "valid": store.verify_backup(b)} for b in store.backups(a.uuid)]
        elif a.cmd == "restore":
            q = store.restore(a.uuid, a.backup)
            out = {"quarantined": str(q) if q else None}
        elif a.cmd == "import":
            q = store.import_saves(a.uuid, a.source)
            out = {"quarantined": str(q) if q else None}
    except (S.SettingsError, P.ProfileBusy, P.BackupError, OSError, ValueError) as e:
        print(json.dumps({"error": str(e)}, ensure_ascii=False))
        return 1
    print(json.dumps(out, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
