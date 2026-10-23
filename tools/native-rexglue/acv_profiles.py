#!/usr/bin/env python3
"""Persistent profile store: per-profile user_data_root, lock, backup/restore, import (PLAN_GUI.MD stage B).

Layout under <root>/profiles/<uuid>/:
  profile.json   identity (acv_settings.Profile)
  userdata/      user_data_root passed to the runtime: the game saves live here
  cache/         cache_root (shader cache etc.), never part of backups
  backups/       <UTC stamp>/ verified copies of userdata/ with manifest.json
  quarantine/    originals replaced by a restore/import, kept for recovery
  .lock          flock target; the kernel releases it if the owner dies
"""
import datetime, fcntl, hashlib, json, os, shutil, uuid
from pathlib import Path
import acv_settings as s


class ProfileBusy(RuntimeError):
    pass


class BackupError(RuntimeError):
    pass


def _stamp():
    return datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")


def _digest(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(1 << 20):
            h.update(chunk)
    return h.hexdigest()


def tree_manifest(directory):
    directory = Path(directory)
    return {p.relative_to(directory).as_posix(): {"size": p.stat().st_size, "sha256": _digest(p)}
            for p in sorted(directory.rglob("*")) if p.is_file()}


def _fsync_dir(path):
    fd = os.open(path, os.O_RDONLY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


class ProfileLock:
    """Exclusive non-blocking lock on a profile; released on close or process death."""

    def __init__(self, directory):
        self.path = Path(directory) / ".lock"
        self.fd = None

    def acquire(self):
        self.path.parent.mkdir(parents=True, exist_ok=True)
        fd = os.open(self.path, os.O_RDWR | os.O_CREAT, 0o600)
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            os.close(fd)
            raise ProfileBusy(f"profile is in use: {self.path.parent.name}") from None
        self.fd = fd
        return self

    def release(self):
        if self.fd is not None:
            os.close(self.fd)
            self.fd = None

    __enter__ = acquire

    def __exit__(self, *exc):
        self.release()


class ProfileStore:
    def __init__(self, root):
        self.root = Path(root)

    def _dir(self, profile_uuid):
        # Only canonical lowercase UUIDs name directories: no separators, no traversal.
        try:
            canonical = str(uuid.UUID(profile_uuid))
        except (ValueError, AttributeError, TypeError):
            canonical = None
        if canonical is None or canonical != profile_uuid:
            raise s.SettingsError("profile uuid is invalid")
        return self.root / "profiles" / profile_uuid

    def userdata(self, profile_uuid):
        return self._dir(profile_uuid) / "userdata"

    def cache(self, profile_uuid):
        return self._dir(profile_uuid) / "cache"

    def create(self, display_name, **kw):
        profile = s.new_profile(display_name, **kw)
        s.save_profile(self.root, profile)
        self.userdata(profile.uuid).mkdir(parents=True)
        return profile

    def get(self, profile_uuid):
        self._dir(profile_uuid)
        return s.load_profile(self.root, profile_uuid)

    def list(self):
        base = self.root / "profiles"
        found = []
        for entry in sorted(base.iterdir()) if base.is_dir() else []:
            try:
                found.append(self.get(entry.name))
            except (s.SettingsError, OSError, ValueError):
                continue  # damaged entry: never delete it, the launcher can report it separately
        return found

    def rename(self, profile_uuid, display_name):
        profile = s.rename_profile(self.get(profile_uuid), display_name)
        s.save_profile(self.root, profile)
        return profile

    def set_names(self, profile_uuid, gamertag=None, pilot_name=None):
        with self.lock(profile_uuid):  # not while the game runs: the runtime reads names at start
            profile = s.set_names(self.get(profile_uuid), gamertag, pilot_name)
            s.save_profile(self.root, profile)
        return profile

    def lock(self, profile_uuid):
        return ProfileLock(self._dir(profile_uuid))

    # --- backup / restore -------------------------------------------------------------------
    def backup(self, profile_uuid):
        """Copy userdata/ to backups/<stamp>/, verify every file, then publish atomically.
        The caller must hold the profile lock or know the game is stopped."""
        pdir = self._dir(profile_uuid)
        source = pdir / "userdata"
        expected = tree_manifest(source)
        name = _stamp()
        staging = pdir / "backups" / (name + ".tmp")
        staging.mkdir(parents=True)
        try:
            shutil.copytree(source, staging / "userdata")
            if tree_manifest(staging / "userdata") != expected:
                raise BackupError("backup verification failed: copy differs from source")
            if tree_manifest(source) != expected:
                raise BackupError("userdata changed during backup; stop the game and retry")
            (staging / "manifest.json").write_text(json.dumps(
                {"schema_version": s.SCHEMA_VERSION, "files": expected}, indent=2, sort_keys=True))
            _fsync_dir(staging)
            final = pdir / "backups" / name
            os.rename(staging, final)
            _fsync_dir(final.parent)
        except BaseException:
            shutil.rmtree(staging, ignore_errors=True)
            raise
        return final

    def backups(self, profile_uuid):
        base = self._dir(profile_uuid) / "backups"
        return sorted(p for p in base.iterdir() if p.is_dir() and not p.name.endswith(".tmp")) \
            if base.is_dir() else []

    def verify_backup(self, backup_dir):
        backup_dir = Path(backup_dir)
        manifest = json.loads((backup_dir / "manifest.json").read_text())
        return tree_manifest(backup_dir / "userdata") == manifest["files"]

    def _replace_userdata(self, profile_uuid, new_tree, ignore=None):
        pdir = self._dir(profile_uuid)
        current = pdir / "userdata"
        staged = pdir / ("userdata.new." + _stamp())
        shutil.copytree(new_tree, staged, ignore=ignore)
        quarantined = None
        try:
            if current.exists():
                quarantined = pdir / "quarantine" / _stamp()
                quarantined.parent.mkdir(exist_ok=True)
                os.rename(current, quarantined)
            os.rename(staged, current)
        except BaseException:
            if quarantined is not None and not current.exists():
                os.rename(quarantined, current)
            shutil.rmtree(staged, ignore_errors=True)
            raise
        _fsync_dir(pdir)
        return quarantined

    def restore(self, profile_uuid, backup_dir):
        """Replace userdata/ with a verified backup; the old tree is kept in quarantine/."""
        backup_dir = Path(backup_dir)
        if not self.verify_backup(backup_dir):
            raise BackupError(f"backup is damaged: {backup_dir}")
        with self.lock(profile_uuid):  # fails if the game is running
            return self._replace_userdata(profile_uuid, backup_dir / "userdata")

    def import_saves(self, profile_uuid, source):
        """Import a user_data_root tree. Content is copied byte for byte: owner XUIDs inside
        files are not rewritten (stage B3: no blind XUID migration)."""
        source = Path(source)
        if not source.is_dir() or not any(p.is_file() for p in source.rglob("*") if "cache" not in p.relative_to(source).parts[:1]):
            raise BackupError(f"nothing to import in {source}")
        if any(p.is_symlink() for p in source.rglob("*")):
            raise BackupError("import source must not contain symlinks")
        # Shader/other caches are not progress: keep them out of the profile's saves and backups.
        skip_cache = shutil.ignore_patterns("cache") if (source / "cache").is_dir() else None
        with self.lock(profile_uuid):
            return self._replace_userdata(profile_uuid, source, skip_cache)
