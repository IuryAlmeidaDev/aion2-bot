import struct
import ctypes
from ctypes import wintypes
import os

kernel32 = ctypes.windll.kernel32

kernel32.BeginUpdateResourceW.argtypes = [wintypes.LPCWSTR, wintypes.BOOL]
kernel32.BeginUpdateResourceW.restype = wintypes.HANDLE

kernel32.UpdateResourceW.argtypes = [
    wintypes.HANDLE,
    ctypes.c_void_p,  # lpType
    ctypes.c_void_p,  # lpName
    wintypes.WORD,    # wLanguage
    ctypes.c_void_p,  # lpData
    wintypes.DWORD    # cbData
]
kernel32.UpdateResourceW.restype = wintypes.BOOL

kernel32.EndUpdateResourceW.argtypes = [wintypes.HANDLE, wintypes.BOOL]
kernel32.EndUpdateResourceW.restype = wintypes.BOOL

RT_ICON = 3
RT_GROUP_ICON = 14

def inject_ico(exe_path, ico_path):
    abs_exe = os.path.abspath(exe_path)
    abs_ico = os.path.abspath(ico_path)

    with open(abs_ico, 'rb') as f:
        ico_data = f.read()

    # Parse ICONDIR
    reserved, ico_type, count = struct.unpack('<HHH', ico_data[:6])
    assert ico_type == 1, "Not an icon file"

    entries = []
    offset = 6
    for i in range(count):
        width, height, colors, res, planes, bpp, size, img_offset = struct.unpack(
            '<BBBBHHII', ico_data[offset:offset+16]
        )
        entries.append({
            'id': i + 1,
            'width': width,
            'height': height,
            'colors': colors,
            'planes': planes,
            'bpp': bpp,
            'size': size,
            'offset': img_offset
        })
        offset += 16

    # Open PE for update
    h_update = kernel32.BeginUpdateResourceW(abs_exe, False)
    if not h_update:
        err = kernel32.GetLastError()
        raise RuntimeError(f"BeginUpdateResourceW failed: {err}")

    # Write each RT_ICON
    for entry in entries:
        img_bytes = ico_data[entry['offset']:entry['offset'] + entry['size']]
        buf = ctypes.create_string_buffer(img_bytes)
        res = kernel32.UpdateResourceW(
            h_update,
            ctypes.c_void_p(RT_ICON),
            ctypes.c_void_p(entry['id']),
            0,  # MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL) = 0
            ctypes.cast(buf, ctypes.c_void_p),
            len(img_bytes)
        )
        if not res:
            err = kernel32.GetLastError()
            raise RuntimeError(f"UpdateResourceW RT_ICON {entry['id']} failed: {err}")

    # Build GRPICONDIR
    grp_header = struct.pack('<HHH', 0, 1, count)
    grp_entries = b""
    for entry in entries:
        grp_entries += struct.pack(
            '<BBBBHHIH',
            entry['width'],
            entry['height'],
            entry['colors'],
            0,
            entry['planes'],
            entry['bpp'],
            entry['size'],
            entry['id']
        )
    grp_data = grp_header + grp_entries
    grp_buf = ctypes.create_string_buffer(grp_data)

    res = kernel32.UpdateResourceW(
        h_update,
        ctypes.c_void_p(RT_GROUP_ICON),
        ctypes.c_void_p(1),  # MAIN ICON ID = 1
        0,
        ctypes.cast(grp_buf, ctypes.c_void_p),
        len(grp_data)
    )
    if not res:
        err = kernel32.GetLastError()
        raise RuntimeError(f"UpdateResourceW RT_GROUP_ICON failed: {err}")

    if not kernel32.EndUpdateResourceW(h_update, False):
        err = kernel32.GetLastError()
        raise RuntimeError(f"EndUpdateResourceW failed: {err}")

    print(f"Successfully embedded {len(entries)} icon sizes into {exe_path}")

if __name__ == '__main__':
    inject_ico('GhostHUD.exe', 'ghosthud.ico')
