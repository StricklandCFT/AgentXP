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

void AppendAgentLog2(const char* msg, DWORD value) {
  char buffer[256];
  wsprintfA(buffer, "%s %lu (0x%08lX)", msg, value, value);
  AppendAgentLog(buffer);
}

std::string GetExeDir() {
  char path[MAX_PATH];
  DWORD len = GetModuleFileNameA(NULL, path, sizeof(path));
  if (len == 0 || len >= sizeof(path)) {
    return ".";
  }
  for (int i = (int)len - 1; i >= 0; --i) {
    if (path[i] == '\\' || path[i] == '/') {
      path[i] = '\0';
      break;
    }
  }
  return std::string(path);
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

  if (!SetDllDirectoryA(".")) {
    AppendAgentLog("LaunchProcmonAndInject: SetDllDirectory failed");
  }

  std::string base_dir = GetExeDir();
  std::string ioctls_path = base_dir + "\\ioctls.bin";
  if (!SetEnvironmentVariableA("PROC_MON_IOCTLS_PATH", ioctls_path.c_str())) {
    AppendAgentLog("LaunchProcmonAndInject: SetEnvironmentVariable failed");
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

  std::string dll_full = dll_path;
  if (dll_path.find(':') == std::string::npos && dll_path.find('\\') == std::string::npos) {
    dll_full = base_dir + "\\" + dll_path;
  }
  SIZE_T path_len = dll_full.size() + 1;
  LPVOID remote_mem = VirtualAllocEx(pi.hProcess, NULL, path_len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (!remote_mem) {
    AppendAgentLog("LaunchProcmonAndInject: VirtualAllocEx failed");
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return false;
  }

  SIZE_T written = 0;
  if (!WriteProcessMemory(pi.hProcess, remote_mem, dll_full.c_str(), path_len, &written)) {
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
  DWORD exit_code = 0;
  if (GetExitCodeThread(remote_thread, &exit_code)) {
    if (exit_code == 0) {
      AppendAgentLog("LaunchProcmonAndInject: LoadLibrary returned NULL");
    } else {
      AppendAgentLog("LaunchProcmonAndInject: LoadLibrary returned handle");
    }
  } else {
    AppendAgentLog("LaunchProcmonAndInject: GetExitCodeThread failed");
  }
  CloseHandle(remote_thread);
  VirtualFreeEx(pi.hProcess, remote_mem, 0, MEM_RELEASE);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return true;
}
