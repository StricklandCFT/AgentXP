#include "injector.h"

#include <windows.h>

void AppendAgentLog(const char* msg) {
  HANDLE h = CreateFileA("agent.log", GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE) {
    return;
  }
  SetFilePointer(h, 0, NULL, FILE_END);
  DWORD written = 0;
  WriteFile(h, msg, (DWORD)lstrlenA(msg), &written, NULL);
  WriteFile(h, "\r\n", 2, &written, NULL);
  CloseHandle(h);
}

static bool FileExists(const std::string& path) {
  DWORD attrs = GetFileAttributesA(path.c_str());
  return (attrs != INVALID_FILE_ATTRIBUTES) && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

bool LaunchProcmonAndInject(const std::string& procmon_path, const std::string& dll_path, unsigned long& out_pid) {
  out_pid = 0;
  if (!FileExists(procmon_path) || !FileExists(dll_path)) {
    AppendAgentLog("LaunchProcmonAndInject: missing procmon_path or dll_path");
    return false;
  }

  STARTUPINFOA si;
  PROCESS_INFORMATION pi;
  ZeroMemory(&si, sizeof(si));
  ZeroMemory(&pi, sizeof(pi));
  si.cb = sizeof(si);

  std::string cmd = "\"" + procmon_path + "\" /AcceptEula /Quiet /Minimized";
  BOOL ok = CreateProcessA(NULL, const_cast<char*>(cmd.c_str()), NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
  if (!ok) {
    AppendAgentLog("LaunchProcmonAndInject: CreateProcess failed");
    return false;
  }

  out_pid = pi.dwProcessId;

  SIZE_T path_len = dll_path.size() + 1;
  LPVOID remote_mem = VirtualAllocEx(pi.hProcess, NULL, path_len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (!remote_mem) {
    AppendAgentLog("LaunchProcmonAndInject: VirtualAllocEx failed");
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return false;
  }

  SIZE_T written = 0;
  if (!WriteProcessMemory(pi.hProcess, remote_mem, dll_path.c_str(), path_len, &written)) {
    AppendAgentLog("LaunchProcmonAndInject: WriteProcessMemory failed");
    VirtualFreeEx(pi.hProcess, remote_mem, 0, MEM_RELEASE);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return false;
  }

  HMODULE k32 = GetModuleHandleA("kernel32.dll");
  FARPROC load_lib = GetProcAddress(k32, "LoadLibraryA");
  HANDLE remote_thread = CreateRemoteThread(pi.hProcess, NULL, 0,
                                             (LPTHREAD_START_ROUTINE)load_lib, remote_mem, 0, NULL);
  if (!remote_thread) {
    AppendAgentLog("LaunchProcmonAndInject: CreateRemoteThread failed");
    VirtualFreeEx(pi.hProcess, remote_mem, 0, MEM_RELEASE);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return false;
  }

  WaitForSingleObject(remote_thread, 10000);
  AppendAgentLog("LaunchProcmonAndInject: injection attempted");
  CloseHandle(remote_thread);
  VirtualFreeEx(pi.hProcess, remote_mem, 0, MEM_RELEASE);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return true;
}
