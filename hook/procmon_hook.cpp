#include <windows.h>
#include <winnt.h>
#include <winternl.h>
#include <imagehlp.h>

#pragma comment(lib, "imagehlp.lib")

typedef BOOL (WINAPI *DeviceIoControlFn)(HANDLE, DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);

static DeviceIoControlFn g_real = NULL;
static HANDLE g_log = INVALID_HANDLE_VALUE;

static void OpenLog() {
  if (g_log != INVALID_HANDLE_VALUE) {
    return;
  }
  g_log = CreateFileA("ioctls.bin", GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (g_log != INVALID_HANDLE_VALUE) {
    SetFilePointer(g_log, 0, NULL, FILE_END);
  }
}

static void WriteIoctl(DWORD code, const void* in_buf, DWORD in_len, DWORD out_len) {
  if (g_log == INVALID_HANDLE_VALUE) {
    return;
  }
  DWORD written = 0;
  WriteFile(g_log, &code, sizeof(code), &written, NULL);
  WriteFile(g_log, &in_len, sizeof(in_len), &written, NULL);
  WriteFile(g_log, &out_len, sizeof(out_len), &written, NULL);
  if (in_len && in_buf) {
    WriteFile(g_log, in_buf, in_len, &written, NULL);
  }
}

static void PatchIAT() {
  HMODULE exe = GetModuleHandleA(NULL);
  if (!exe) {
    return;
  }
  ULONG size = 0;
  PIMAGE_IMPORT_DESCRIPTOR imp = (PIMAGE_IMPORT_DESCRIPTOR)ImageDirectoryEntryToData(exe, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &size);
  if (!imp) {
    return;
  }
  for (; imp->Name; ++imp) {
    const char* dll = (const char*)((BYTE*)exe + imp->Name);
    if (_stricmp(dll, "kernel32.dll") != 0) {
      continue;
    }
    PIMAGE_THUNK_DATA thunk = (PIMAGE_THUNK_DATA)((BYTE*)exe + imp->FirstThunk);
    PIMAGE_THUNK_DATA orig = (PIMAGE_THUNK_DATA)((BYTE*)exe + imp->OriginalFirstThunk);
    for (; orig->u1.AddressOfData; ++orig, ++thunk) {
      if (orig->u1.Ordinal & IMAGE_ORDINAL_FLAG) {
        continue;
      }
      PIMAGE_IMPORT_BY_NAME by_name = (PIMAGE_IMPORT_BY_NAME)((BYTE*)exe + orig->u1.AddressOfData);
      if (lstrcmpiA((char*)by_name->Name, "DeviceIoControl") == 0) {
        DWORD old = 0;
        VirtualProtect(&thunk->u1.Function, sizeof(void*), PAGE_READWRITE, &old);
        g_real = (DeviceIoControlFn)(ULONG_PTR)thunk->u1.Function;
        thunk->u1.Function = (ULONG_PTR)&DeviceIoControl;
        VirtualProtect(&thunk->u1.Function, sizeof(void*), old, &old);
        return;
      }
    }
  }
}

extern "C" __declspec(dllexport) BOOL WINAPI DeviceIoControl(
    HANDLE hDevice,
    DWORD dwIoControlCode,
    LPVOID lpInBuffer,
    DWORD nInBufferSize,
    LPVOID lpOutBuffer,
    DWORD nOutBufferSize,
    LPDWORD lpBytesReturned,
    LPOVERLAPPED lpOverlapped) {
  OpenLog();
  WriteIoctl(dwIoControlCode, lpInBuffer, nInBufferSize, nOutBufferSize);
  if (!g_real) {
    g_real = (DeviceIoControlFn)GetProcAddress(GetModuleHandleA("kernel32.dll"), "DeviceIoControl");
  }
  return g_real(hDevice, dwIoControlCode, lpInBuffer, nInBufferSize, lpOutBuffer, nOutBufferSize, lpBytesReturned, lpOverlapped);
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(hinst);
    PatchIAT();
    OpenLog();
  }
  if (reason == DLL_PROCESS_DETACH) {
    if (g_log != INVALID_HANDLE_VALUE) {
      CloseHandle(g_log);
      g_log = INVALID_HANDLE_VALUE;
    }
  }
  return TRUE;
}
