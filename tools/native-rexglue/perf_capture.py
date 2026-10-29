#!/usr/bin/env python3
"""Capture concurrent CPU/GPU and user-IP evidence from an existing Linux game."""
import argparse
import hashlib
import json
import os
import subprocess
import sys
import time
from pathlib import Path

from perf_gate import process_identity, wait_for_process_age

HERE = Path(__file__).resolve().parent


def find_process(executable, pid=None):
    expected = executable.resolve(strict=True)
    if pid is not None:
        if (Path("/proc") / str(pid) / "exe").resolve(strict=True) != expected:
            raise RuntimeError("Unexpected executable for requested PID")
        return pid
    matches = []
    for entry in Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        try:
            if (entry / "exe").resolve(strict=True) == expected:
                matches.append(int(entry.name))
        except OSError:
            continue
    if len(matches) != 1:
        raise RuntimeError(f"Expected one running game, found {matches}; start game or select --pid")
    return matches[0]


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def artifact_records(executable, maps):
    paths = {str(executable.resolve(strict=True))}
    devices = {}
    for line in maps.splitlines():
        fields = line.split(maxsplit=5)
        if len(fields) == 6 and Path(fields[5]).name.startswith(("librex", "libffx")):
            path = Path(fields[5])
            stat = path.stat()
            if stat.st_ino != int(fields[4]):
                raise RuntimeError(f"Mapped artifact inode differs on disk: {path}")
            # Some filesystems expose the backing device in maps and a different
            # st_dev via stat. Preserve the mismatch, never claim exact identity.
            devices.setdefault(fields[5], set()).add(fields[3])
            paths.add(fields[5])
    records = []
    for name in sorted(paths):
        path = Path(name)
        stat = path.stat()
        mapped = sorted(devices.get(name, []))
        stat_device = f"{os.major(stat.st_dev):02x}:{os.minor(stat.st_dev):02x}"
        records.append({"path": name, "sha256": digest(path), "inode": stat.st_ino,
                        "stat_device": stat_device, "mapped_devices": mapped,
                        "device_matches": all(tuple(int(x, 16) for x in d.split(":")) ==
                                              (os.major(stat.st_dev), os.minor(stat.st_dev))
                                              for d in mapped) if mapped else None})
    return records


def _capture(pid, executable, output, scene, seconds, after_start):
    """after_start=0 is reserved for host fixtures; CLI enforces >=90 seconds."""
    expected = executable.resolve(strict=True)
    gate = wait_for_process_age(pid, expected, after_start)
    root = Path("/proc") / str(pid)
    output.mkdir(parents=True, exist_ok=False)
    maps = (root / "maps").read_text()
    command = [s.decode(errors="replace") for s in (root / "cmdline").read_bytes().split(b"\0") if s]
    metadata = {
        "pid": pid, "scene": scene, "seconds_requested": seconds,
        "gate": gate, "command": command,
        "artifacts_before": artifact_records(expected, maps),
        "scope": "Guest swap FPS and per-frame 1% low are not measured. GPU utilization is device-wide. Artifact hashes are disk bytes; device mismatches are retained and do not prove loaded-byte identity.",
        "started_unix": time.time(),
        "instrumentation_complete": False, "problems": ["Capture not finished"],
    }
    (output / "maps.txt").write_text(maps)
    (output / "session.json").write_text(json.dumps(metadata, indent=2, ensure_ascii=False))
    jobs = []
    results = {}
    try:
        # Both children gate against the same PID; start them together, without waiting for either.
        for name, script in (("cpu_gpu", "perf_sample.py"), ("ips", "perf_ips.py")):
            log = (output / (name + ".log")).open("w")
            try:
                job = subprocess.Popen([
                    sys.executable, str(HERE / script), "--pid", str(pid),
                    "--executable", str(expected), "--output", str(output / name),
                    "--seconds", str(seconds), "--after-start", str(after_start),
                ], stdout=log, stderr=subprocess.STDOUT)
            except BaseException:
                log.close()
                raise
            jobs.append((name, job, log))
        print(f"Recording {seconds}s: pid={pid}, scene={scene}, evidence={output}", flush=True)
        deadline = time.monotonic() + seconds + 15
        for name, job, log in jobs:
            try:
                results[name] = job.wait(timeout=max(.01, deadline - time.monotonic()))
            except subprocess.TimeoutExpired:
                job.terminate()
                results[name] = "timeout"
    finally:
        # Only profiler children are ours. Never signal the game.
        for name, job, log in jobs:
            if job.poll() is None:
                job.terminate()
                try:
                    job.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    job.kill()
                    job.wait()
            log.close()
            print((output / (name + ".log")).read_text(), end="", flush=True)
        metadata["finished_unix"] = time.time()
        metadata["collectors"] = results
        (output / "session.json").write_text(json.dumps(metadata, indent=2, ensure_ascii=False))

    problems = []
    if process_identity(pid) != gate["process_start_ticks"] or (root / "exe").resolve() != expected:
        problems.append("Game identity changed")
    for name, result in results.items():
        if result != 0:
            problems.append(f"{name} collector failed: {result}")
    samples = output / "cpu_gpu" / "samples.jsonl"
    rows = [json.loads(line) for line in samples.read_text().splitlines()] if samples.exists() else []
    if not rows or rows[-1]["elapsed"] < seconds - 2:
        problems.append("CPU/GPU window incomplete")
    if not any(row.get("gpu_returncode") == 0 and row.get("gpu_csv") for row in rows):
        problems.append("No GPU utilization obtained")
    ips_path = output / "ips" / "samples.json"
    ips = json.loads(ips_path.read_text()) if ips_path.exists() else {}
    if ips.get("unavailable"):
        problems.append("Some user-IP perf events unavailable; see ips/samples.json")
    if sum(t["lost"] for t in ips.get("threads", [])):
        problems.append("User-IP samples lost")
    metadata["artifacts_after"] = artifact_records(expected, (root / "maps").read_text())
    if metadata["artifacts_before"] != metadata["artifacts_after"]:
        problems.append("Loaded artifact file set or hashes changed during capture")
    # Preserve log bytes and launch metadata; do not read or copy saves/config/profile data.
    log_args = [arg.split("=", 1)[1] for arg in command if arg.startswith("--log_file=")]
    if len(log_args) == 1:
        runtime = Path(log_args[0])
        if not runtime.is_absolute():
            runtime = (root / "cwd").resolve() / runtime
        metadata["runtime_source"] = str(runtime)
        (output / "runtime.log").write_bytes(runtime.read_bytes())
        launch = runtime.parent / "launch.json"
        if launch.is_file():
            (output / "launch.json").write_bytes(launch.read_bytes())
    else:
        problems.append("Runtime log not identified from --log_file; attach same-session log separately")
    metadata["problems"] = problems
    metadata["instrumentation_complete"] = not problems
    (output / "session.json").write_text(json.dumps(metadata, indent=2, ensure_ascii=False))
    print(json.dumps({"evidence": str(output), "instrumentation_complete": not problems,
                      "problems": problems}, ensure_ascii=False), flush=True)
    return 1 if problems else 0



def capture(pid, executable, output, scene, seconds, after_start):
    existed = output.exists()
    try:
        return _capture(pid, executable, output, scene, seconds, after_start)
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        if existed:
            raise
        session = output / "session.json"
        if session.is_file():
            metadata = json.loads(session.read_text())
            metadata["instrumentation_complete"] = False
            metadata["problems"] = [f"Capture failed: {type(error).__name__}: {error}"]
            session.write_text(json.dumps(metadata, indent=2, ensure_ascii=False))
        print(f"Capture failed: {type(error).__name__}: {error}", file=sys.stderr, flush=True)
        return 1

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int)
    parser.add_argument("--executable", type=Path, default=HERE.parents[2] / "ACV-Linux-release-1.0/bin/game/acv_linux_probe")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--scene", required=True, help="Mission, route and camera action for this window")
    parser.add_argument("--seconds", type=int, default=60, help="Recording duration, 3..300 seconds")
    parser.add_argument("--after-start", type=float, default=120)
    args = parser.parse_args()
    if not 3 <= args.seconds <= 300:
        parser.error("--seconds must be 3..300")
    if not 90 <= args.after_start <= 300:
        parser.error("--after-start must be 90..300 seconds")
    if not args.scene.strip():
        parser.error("--scene must identify the mission/action")
    pid = find_process(args.executable, args.pid)
    return capture(pid, args.executable, args.output, args.scene, args.seconds, args.after_start)


if __name__ == "__main__":
    sys.exit(main())
