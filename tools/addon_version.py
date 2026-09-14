"""Native Windows VERSIONINFO and the pinned official About-label operands."""
import struct
import subprocess
import ctypes
import re
from datetime import datetime

VERSION = "1.0.3"
ABOUT_PATCHES = {
    0x199D8: bytes.fromhex("4C 8D 05 F1 76 20 00"),  # append source
    0x199ED: bytes.fromhex("BA 13 00 00 00"),        # growth
    0x199F2: bytes.fromhex("41 B9 13 00 00 00"),     # source length
}


def compile_version(output, build, release=False, version=VERSION):
    if not 1 <= build <= 65535:
        raise ValueError("build number must fit a Windows version component")
    stamp = datetime.now().strftime("D: %Y-%m-%d | T: %H:%M:%S")
    output.parent.mkdir(parents=True, exist_ok=True)
    source = output.parent / "addon-version.rc"
    result = output.parent / "addon-version.res"
    source.write_text(f'''#include <winver.h>
1 VERSIONINFO
 FILEVERSION {version.replace('.', ',')},{build}
 PRODUCTVERSION {version.replace('.', ',')},{build}
 FILEFLAGSMASK VS_FFI_FILEFLAGSMASK
 FILEFLAGS {0 if release else 'VS_FF_PRERELEASE'}
 FILEOS VOS_NT_WINDOWS32
 FILETYPE VFT_DLL
BEGIN
 BLOCK "StringFileInfo"
 BEGIN
  BLOCK "040904B0"
  BEGIN
   VALUE "FileDescription", "RenoDX DLSS_A"
   VALUE "FileVersion", "{version}.{build}"
   VALUE "ProductName", "DLAssAss 5 Tool Addon"
   VALUE "ProductVersion", "{version if release else version + '-preview'}.{build}"
   VALUE "OriginalFilename", "renodx-dlss5-super-anus.addon64"
   VALUE "Comments", "Build {build} | {stamp}"
  END
 END
 BLOCK "VarFileInfo"
 BEGIN
  VALUE "Translation", 0x0409, 1200
 END
END
''', encoding="ascii")
    subprocess.run(["rc.exe", "/nologo", "/fo", str(result.resolve()), str(source.resolve())], check=True)
    data = result.read_bytes()
    cursor = 0
    versions = []
    while cursor < len(data):
        size, header = struct.unpack_from("<II", data, cursor)
        if header < 8 or cursor + header + size > len(data):
            raise ValueError("invalid compiled resource")
        if size and data[cursor + 8:cursor + 16] == struct.pack("<4H", 0xFFFF, 16, 0xFFFF, 1):
            versions.append(data[cursor + header:cursor + header + size])
        cursor = (cursor + header + size + 3) & ~3
    if len(versions) != 1:
        raise ValueError("expected exactly one native VERSIONINFO")
    return versions[0], f"V{version} | {stamp}".encode("ascii")


def resource_leaves(image):
    rva, size = image.directory(2)
    root = image.rva_to_offset(rva)
    data = bytes(image.data[root:root + size])
    leaves = {}
    def walk(offset, path):
        if len(path) > 2 or offset + 16 > size:
            raise ValueError("unexpected resource tree")
        named, ids = struct.unpack_from("<HH", data, offset + 12)
        if named:
            raise ValueError("expected pinned numeric resource IDs")
        for i in range(ids):
            key, child = struct.unpack_from("<II", data, offset + 16 + i * 8)
            if child & 0x80000000:
                walk(child & 0x7FFFFFFF, path + (key,))
            else:
                leaves[path + (key,)] = child
    walk(0, ())
    return data, leaves


def version_resources(base, root_rva, version):
    original, leaves = resource_leaves(base)
    result = bytearray(original)
    result.extend(b"\0" * ((-len(result)) & 3))
    version_rva = root_rva + len(result)
    result.extend(version)
    selected = [offset for path, offset in leaves.items() if path[0] == 16]
    if len(selected) != 1:
        raise ValueError("expected one existing version resource")
    struct.pack_into("<II", result, selected[0], version_rva, len(version))
    # Keep every resource inside the relocated tree for Windows' loader.
    original_rva, _ = base.directory(2)
    for path, offset in leaves.items():
        if path[0] != 16:
            rva, size = struct.unpack_from("<II", original, offset)
            relative = rva - original_rva
            if relative < 0 or relative + size > len(original):
                raise ValueError("resource data outside pinned tree")
            struct.pack_into("<I", result, offset, root_rva + relative)
    return result


def validate_version(base, addon, path, build, release=False, version=VERSION):
    offset = addon.rva_to_offset(0x199D8)
    assert bytes(addon.data[offset:offset + 3]) == b"\x4C\x8D\x05"
    about_rva = 0x199DF + struct.unpack_from("<i", addon.data, offset + 3)[0]
    about_offset = addon.rva_to_offset(about_rva)
    about = bytes(addon.data[about_offset:addon.data.index(0, about_offset)]).decode("ascii")
    assert re.fullmatch(rf"V{re.escape(version)} \| D: \d{{4}}-\d{{2}}-\d{{2}} \| T: \d{{2}}:\d{{2}}:\d{{2}}", about)
    for rva, prefix in ((0x199ED, b"\xBA"), (0x199F2, b"\x41\xB9")):
        offset = addon.rva_to_offset(rva)
        assert bytes(addon.data[offset:offset + len(prefix)]) == prefix
        assert struct.unpack_from("<I", addon.data, offset + len(prefix))[0] == len(about)
    old_data, old_leaves = resource_leaves(base)
    new_data, new_leaves = resource_leaves(addon)
    assert old_leaves.keys() == new_leaves.keys()
    for key, offset in old_leaves.items():
        if key[0] != 16:
            new_offset = new_leaves[key]
            assert old_data[offset + 4:offset + 16] == new_data[new_offset + 4:new_offset + 16]
            old_rva, size = struct.unpack_from("<II", old_data, offset)
            new_rva = struct.unpack_from("<I", new_data, new_offset)[0]
            old_start, new_start = base.rva_to_offset(old_rva), addon.rva_to_offset(new_rva)
            assert base.data[old_start:old_start + size] == addon.data[new_start:new_start + size]
    api = ctypes.WinDLL("version", use_last_error=True)
    api.GetFileVersionInfoSizeW.argtypes = [ctypes.c_wchar_p, ctypes.c_void_p]
    api.GetFileVersionInfoSizeW.restype = ctypes.c_uint32
    size = api.GetFileVersionInfoSizeW(str(path.resolve()), None)
    assert size, ctypes.get_last_error()
    buffer = ctypes.create_string_buffer(size)
    api.GetFileVersionInfoW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
    assert api.GetFileVersionInfoW(str(path.resolve()), 0, size, buffer)
    api.VerQueryValueW.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_uint32)]
    def query(key):
        address, length = ctypes.c_void_p(), ctypes.c_uint32()
        assert api.VerQueryValueW(buffer, key, ctypes.byref(address), ctypes.byref(length)), key
        return address, length.value
    fixed, length = query("\\")
    assert length == 52
    values = ctypes.cast(fixed, ctypes.POINTER(ctypes.c_uint32 * 13)).contents
    major, minor, patch = map(int, version.split("."))
    assert values[0] == 0xFEEF04BD and tuple(values[2:6]) == (
        (major << 16) | minor, (patch << 16) | build) * 2
    assert values[7] == (0 if release else 2) # VS_FF_PRERELEASE
    for key, expected in {"FileVersion": f"{version}.{build}", "ProductVersion": f"{version if release else version + '-preview'}.{build}",
                          "Comments": f"Build {build} | " + about.split(" | ", 1)[1]}.items():
        value, _ = query("\\StringFileInfo\\040904B0\\" + key)
        assert ctypes.wstring_at(value) == expected
    print(f"Windows version details verified: {version}.{build}; About: Build: {about}; manifest preserved.")
