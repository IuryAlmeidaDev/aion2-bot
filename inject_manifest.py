import ctypes
from ctypes import wintypes
import os

kernel32 = ctypes.windll.kernel32
kernel32.BeginUpdateResourceW.argtypes = [wintypes.LPCWSTR, wintypes.BOOL]
kernel32.BeginUpdateResourceW.restype = wintypes.HANDLE
kernel32.UpdateResourceW.argtypes = [
    wintypes.HANDLE,
    ctypes.c_void_p,
    ctypes.c_void_p,
    wintypes.WORD,
    ctypes.c_void_p,
    wintypes.DWORD
]
kernel32.UpdateResourceW.restype = wintypes.BOOL
kernel32.EndUpdateResourceW.argtypes = [wintypes.HANDLE, wintypes.BOOL]
kernel32.EndUpdateResourceW.restype = wintypes.BOOL

MANIFEST_XML = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <assemblyIdentity version="1.0.0.0" processorArchitecture="*" name="Aion2Bot" type="win32"/>
  <description>Aion 2 Combat Overlay &amp; Bot</description>
  <trustInfo xmlns="urn:schemas-microsoft-com:asm.v3">
    <security>
      <requestedPrivileges>
        <requestedExecutionLevel level="requireAdministrator" uiAccess="false"/>
      </requestedPrivileges>
    </security>
  </trustInfo>
  <compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1">
    <application>
      <supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"/>
    </application>
  </compatibility>
</assembly>
""".encode('utf-8')

def inject_manifest(exe_path):
    abs_exe = os.path.abspath(exe_path)
    h_update = kernel32.BeginUpdateResourceW(abs_exe, False)
    if not h_update:
        print("BeginUpdateResourceW failed:", kernel32.GetLastError())
        return False

    RT_MANIFEST = ctypes.c_void_p(24)
    MANIFEST_ID = ctypes.c_void_p(1)

    res = kernel32.UpdateResourceW(
        h_update,
        RT_MANIFEST,
        MANIFEST_ID,
        1033, # English / Neutral
        ctypes.cast(ctypes.c_char_p(MANIFEST_XML), ctypes.c_void_p),
        len(MANIFEST_XML)
    )
    if not res:
        print("UpdateResourceW failed:", kernel32.GetLastError())
        kernel32.EndUpdateResourceW(h_update, True)
        return False

    end_res = kernel32.EndUpdateResourceW(h_update, False)
    print("EndUpdateResourceW:", end_res)
    return bool(end_res)

if __name__ == '__main__':
    inject_manifest(r"C:\Users\iuryc\Desktop\Aion2_Bot.exe")
