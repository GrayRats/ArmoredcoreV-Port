#!/usr/bin/env python3
"""Profile management CLI used by the launcher and by hand. Every command prints one JSON document."""
import argparse, dataclasses, json, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import acv_profiles as P
import acv_settings as S

ROOT = Path(__file__).resolve().parents[2]


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--profiles-root", type=Path, default=ROOT / "tmp/acv-user-data")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("list")
    sub.add_parser("capabilities")
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
        if a.cmd == "capabilities":
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
