"""Verify extracted data and publish a new installation without replacing any path."""
import ctypes,errno,hashlib,importlib.util,json,os,sys
from types import SimpleNamespace
from pathlib import Path
MANIFEST=Path(__file__).with_name("game_data_manifest.json")

def _is_link(path):
    path=Path(path)
    if path.is_symlink():return True
    if os.name!="nt":return False
    kernel32=ctypes.WinDLL("kernel32",use_last_error=True)
    get_attributes=kernel32.GetFileAttributesW
    get_attributes.argtypes=[ctypes.c_wchar_p]
    get_attributes.restype=ctypes.c_ulong
    attributes=get_attributes(str(path))
    return attributes!=0xFFFFFFFF and bool(attributes & 0x400)  # FILE_ATTRIBUTE_REPARSE_POINT

def _load_tool(name):
    """Import a helper script that sits next to this module (package) or one level up (development tree)."""
    here=Path(__file__).resolve().parent
    for base in (here,here.parent):
        f=base/name
        if f.is_file():
            module_name=name[:-3];spec=importlib.util.spec_from_file_location(module_name,f)
            module=importlib.util.module_from_spec(spec);sys.modules[module_name]=module;spec.loader.exec_module(module)
            return module
    raise ValueError("Не найден инструмент разбиения архива: "+name)

def _file_digest(path):
    with Path(path).open("rb") as stream:return hashlib.file_digest(stream,"sha256").hexdigest()

def shard_staging(staging,manifest=MANIFEST):
    """A fresh disc extraction carries bind/dvdbnd.bdt. The game reads shards instead, so check the archive against
    the manifest, split it into bind/shards, verify the split byte for byte and delete the archive.
    Returns False when the staging directory has no archive (nothing to do)."""
    staging=Path(staging)
    bdt=staging/"bind/dvdbnd.bdt";bhd=staging/"bind/dvdbnd5.bhd"
    if not bdt.is_file():return False
    document=json.loads(Path(manifest).read_text())
    source=document.get("source_archive",{}).get("bind/dvdbnd.bdt")
    header=document["files"].get("bind/dvdbnd5.bhd")
    if not source or not header:raise ValueError("Манифест не описывает исходный архив для разбиения")
    if bdt.stat().st_size!=source["size"] or _file_digest(bdt)!=source["sha256"]:raise ValueError("Контрольная сумма исходного архива не совпадает")
    if not bhd.is_file() or bhd.stat().st_size!=header["size"] or _file_digest(bhd)!=header["sha256"]:raise ValueError("Заголовок архива отсутствует или не совпадает")
    out=staging/"bind/shards"
    if out.exists():raise ValueError("Каталог шардов уже существует в установке")
    tool=_load_tool("acv_shard_dvdbnd.py")
    tool.build(SimpleNamespace(bhd=bhd,bdt=bdt,manifest=Path(__file__).with_name("dvdbnd_names.tsv.gz"),out=out,max_shard_mib=768,overwrite=False))
    result=tool.verify(out,bdt)
    if result["bad"] or result["covered_bytes"]!=result["bdt_size"]:raise ValueError("Разбиение архива на шарды не прошло проверку")
    (out/"index.json").unlink()  # build metadata, not part of the installation
    bdt.unlink()
    return True

def verify(directory,full=True,manifest=MANIFEST):
    directory=Path(directory)
    expected=json.loads(Path(manifest).read_text())["files"]
    if _is_link(directory) or not directory.is_dir():raise ValueError("Каталог установки отсутствует или является ссылкой")
    for name,record in expected.items():
        relative=Path(name)
        if relative.is_absolute() or ".." in relative.parts:raise ValueError("Invalid manifest path")
        f=directory/relative
        if any(_is_link(directory/Path(*relative.parts[:i])) for i in range(1,len(relative.parts)+1)):
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
    shard_staging(staging,manifest)
    verify(staging,full=True,manifest=manifest)
    if os.name=="nt":
        kernel32=ctypes.WinDLL("kernel32",use_last_error=True)
        move=kernel32.MoveFileExW
        move.argtypes=[ctypes.c_wchar_p,ctypes.c_wchar_p,ctypes.c_ulong]
        move.restype=ctypes.c_int
        if not move(str(staging),str(destination),0x00000008):  # MOVEFILE_WRITE_THROUGH, no replace.
            code=ctypes.get_last_error()
            if code in (80,183):raise ValueError("Установка уже существует; её файлы сохранены")
            raise OSError(code,ctypes.FormatError(code))
    else:
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
