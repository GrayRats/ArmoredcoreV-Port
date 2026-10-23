#!/usr/bin/env python3
"""Orphaned guest-memory files in /dev/shm.

Runtimes without the shm_unlink-at-creation patch (patches/launcher-runtime-limiter-xuid-shm.patch)
leave a 4.8 GB sparse xenia_memory_* file behind after every hard exit. Enough of them fill the
tmpfs and the next start dies with SIGBUS in Memory::Initialize. Only files owned by the current user
that no live process maps are ever listed for removal, and removal is a separate explicit call."""
import glob, os, shutil

SHM = "/dev/shm"


def _mapped(proc="/proc"):
    mapped = set()
    for maps in glob.glob(f"{proc}/[0-9]*/maps"):
        try:
            with open(maps) as stream:
                for line in stream:
                    if "xenia_memory_" in line:
                        mapped.add(line.split(None, 5)[-1].strip().replace(" (deleted)", ""))
        except OSError:
            continue
    return mapped


def orphans(shm=SHM, proc="/proc"):
    mapped = _mapped(proc)
    found = []
    for path in sorted(glob.glob(os.path.join(shm, "xenia_memory_*"))):
        try:
            st = os.stat(path)
        except OSError:
            continue
        if st.st_uid == os.getuid() and path not in mapped:
            found.append({"path": path, "allocated": st.st_blocks * 512})
    return found


def status(shm=SHM, proc="/proc"):
    usage = shutil.disk_usage(shm)
    items = orphans(shm, proc)
    return {"free": usage.free, "total": usage.total, "orphans": len(items),
            "orphan_bytes": sum(i["allocated"] for i in items)}


def clean(shm=SHM, proc="/proc"):
    """Remove orphans; returns the removed files. Re-checks mappings right before each unlink."""
    removed = []
    for item in orphans(shm, proc):
        if item["path"] in _mapped(proc):
            continue
        try:
            os.unlink(item["path"])
            removed.append(item)
        except OSError:
            pass
    return removed
