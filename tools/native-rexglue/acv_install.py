"""Verify extracted data and publish a new installation without replacing any path."""
import ctypes,errno,hashlib,json,os
from pathlib import Path
MANIFEST=Path(__file__).with_name("game_data_manifest.json")

def verify(directory,full=True,manifest=MANIFEST):
    directory=Path(directory)
    expected=json.loads(Path(manifest).read_text())["files"]
    if directory.is_symlink() or not directory.is_dir():raise ValueError("Каталог установки отсутствует или является ссылкой")
    for name,record in expected.items():
        relative=Path(name)
        if relative.is_absolute() or ".." in relative.parts:raise ValueError("Invalid manifest path")
        f=directory/relative
        if any((directory/Path(*relative.parts[:i])).is_symlink() for i in range(1,len(relative.parts)+1)):
            raise ValueError("Ссылки в игровых данных не допускаются")
        if not f.is_file() or f.stat().st_size!=record["size"]:raise ValueError("Файл отсутствует или имеет неверный размер: "+name)
        if full or name=="default.xex":
            with f.open("rb") as stream:actual=hashlib.file_digest(stream,"sha256").hexdigest()
            if actual!=record["sha256"]:raise ValueError("Контрольная сумма не совпадает: "+name)
    return {"ok":True,"files":len(expected),"full_hash_check":full}

def publish(staging,destination,manifest=MANIFEST):
    staging=Path(staging).resolve();destination=Path(destination).absolute()
    # Verify every file before committing; rename never replaces an installation.
    if staging.parent!=destination.parent or not staging.name.startswith(destination.name+".staging-"):
        raise ValueError("Неверный staging-каталог")
    verify(staging,full=True,manifest=manifest)
    libc=ctypes.CDLL(None,use_errno=True)
    rename=libc.renameat2
    rename.argtypes=[ctypes.c_int,ctypes.c_char_p,ctypes.c_int,ctypes.c_char_p,ctypes.c_uint]
    rename.restype=ctypes.c_int
    if rename(-100,os.fsencode(staging),-100,os.fsencode(destination),1):
        code=ctypes.get_errno()
        if code==errno.EEXIST:raise ValueError("Установка уже существует; её файлы сохранены")
        raise OSError(code,os.strerror(code))
    fd=os.open(destination.parent,os.O_RDONLY|os.O_DIRECTORY)
    try:os.fsync(fd)
    finally:os.close(fd)
    return {"ok":True,"installed":str(destination)}
