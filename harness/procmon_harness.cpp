#include <windows.h>
#include <stdio.h>
#include <string>
#include <vector>

static void PrintUsage() {
  const char* msg =
      "Usage: procmon_harness.exe [--auto] [--procmon-path <path>] --dump procmon-raw.bin "
      "[--device \\\\.\\ProcmonDebugLogger] [--ioctl 0x222004 --inhex deadbeef --outlen 4096]\n";
  OutputDebugStringA(msg);
  DWORD written = 0;
  WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, (DWORD)lstrlenA(msg), &written, NULL);
}

static bool ParseHex(const std::string& hex, std::vector<unsigned char>& out) {
  if (hex.size() % 2 != 0) {
    return false;
  }
  out.clear();
  out.reserve(hex.size() / 2);
  for (size_t i = 0; i < hex.size(); i += 2) {
    char hi = hex[i];
    char lo = hex[i + 1];
    unsigned char v = 0;
    if (hi >= '0' && hi <= '9') v = (hi - '0') << 4;
    else if (hi >= 'a' && hi <= 'f') v = (hi - 'a' + 10) << 4;
    else if (hi >= 'A' && hi <= 'F') v = (hi - 'A' + 10) << 4;
    else return false;
    if (lo >= '0' && lo <= '9') v |= (lo - '0');
    else if (lo >= 'a' && lo <= 'f') v |= (lo - 'a' + 10);
    else if (lo >= 'A' && lo <= 'F') v |= (lo - 'A' + 10);
    else return false;
    out.push_back(v);
  }
  return true;
}

static void WriteRawDump(HANDLE file, DWORD tick, const unsigned char* data, DWORD len) {
  if (file == INVALID_HANDLE_VALUE) {
    return;
  }
  DWORD written = 0;
  WriteFile(file, &tick, sizeof(tick), &written, NULL);
  WriteFile(file, &len, sizeof(len), &written, NULL);
  WriteFile(file, data, len, &written, NULL);
}

static bool FileExists(const char* path) {
  DWORD attrs = GetFileAttributesA(path);
  return (attrs != INVALID_FILE_ATTRIBUTES) && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static std::string JoinPath(const std::string& a, const std::string& b) {
  if (a.empty()) {
    return b;
  }
  if (a[a.size() - 1] == '\\') {
    return a + b;
  }
  return a + "\\" + b;
}

static bool TryLaunchProcmon(const char* path) {
  if (!path || !path[0]) {
    return false;
  }
  if (!FileExists(path)) {
    return false;
  }
  STARTUPINFOA si;
  PROCESS_INFORMATION pi;
  ZeroMemory(&si, sizeof(si));
  ZeroMemory(&pi, sizeof(pi));
  si.cb = sizeof(si);

  std::string cmd = std::string("\"") + path + "\" /AcceptEula /Quiet /Minimized";
  BOOL ok = CreateProcessA(NULL, const_cast<char*>(cmd.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
  if (ok) {
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
  }
  return ok == TRUE;
}

static bool LaunchProcmonAuto(const char* explicit_path) {
  if (explicit_path && explicit_path[0]) {
    return TryLaunchProcmon(explicit_path);
  }

  if (TryLaunchProcmon("procmon.exe")) {
    return true;
  }
  if (TryLaunchProcmon("SysinternalsSuite\\procmon.exe")) {
    return true;
  }

  char* profile = NULL;
  size_t len = 0;
  if (_dupenv_s(&profile, &len, "USERPROFILE") == 0 && profile) {
    std::string base(profile);
    free(profile);
    std::string candidate = JoinPath(base, "Desktop\\SysinternalsSuite\\procmon.exe");
    if (TryLaunchProcmon(candidate.c_str())) {
      return true;
    }
  }

  return false;
}

static void SetExternalLoggerEvent() {
  HANDLE h = OpenEventA(EVENT_MODIFY_STATE, FALSE, "ProcmonExternalLoggerEnabled");
  if (!h) {
    h = OpenEventA(EVENT_MODIFY_STATE, FALSE, "Global\\ProcmonExternalLoggerEnabled");
  }
  if (h) {
    SetEvent(h);
    CloseHandle(h);
  }
}

static bool FindProcmonDevice(std::string& out_device) {
  char buffer[8192];
  DWORD len = QueryDosDeviceA(NULL, buffer, sizeof(buffer));
  if (len == 0) {
    return false;
  }
  const char* p = buffer;
  while (*p) {
    std::string name(p);
    if (name.find("Procmon") != std::string::npos || name.find("PROCMON") != std::string::npos) {
      out_device = "\\\\.\\" + name;
      return true;
    }
    p += name.size() + 1;
  }
  return false;
}

static bool WaitForDevice(std::string& device, int retries, int delay_ms) {
  for (int i = 0; i < retries; ++i) {
    if (device.empty()) {
      if (!FindProcmonDevice(device)) {
        Sleep(delay_ms);
        continue;
      }
    }
    HANDLE h = CreateFileA(device.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
      CloseHandle(h);
      return true;
    }
    Sleep(delay_ms);
  }
  return false;
}

int main(int argc, char** argv) {
  std::string device;
  std::string dump_path = "procmon-raw.bin";
  DWORD ioctl_code = 0;
  std::string inhex;
  DWORD outlen = 0;
  bool auto_mode = false;
  std::string procmon_path;

  for (int i = 1; i < argc; ++i) {
    if (lstrcmpiA(argv[i], "--auto") == 0) {
      auto_mode = true;
    } else if (lstrcmpiA(argv[i], "--procmon-path") == 0 && i + 1 < argc) {
      procmon_path = argv[++i];
    } else if (lstrcmpiA(argv[i], "--device") == 0 && i + 1 < argc) {
      device = argv[++i];
    } else if (lstrcmpiA(argv[i], "--dump") == 0 && i + 1 < argc) {
      dump_path = argv[++i];
    } else if (lstrcmpiA(argv[i], "--ioctl") == 0 && i + 1 < argc) {
      ioctl_code = strtoul(argv[++i], NULL, 0);
    } else if (lstrcmpiA(argv[i], "--inhex") == 0 && i + 1 < argc) {
      inhex = argv[++i];
    } else if (lstrcmpiA(argv[i], "--outlen") == 0 && i + 1 < argc) {
      outlen = strtoul(argv[++i], NULL, 0);
    } else if (lstrcmpiA(argv[i], "--help") == 0) {
      PrintUsage();
      return 0;
    } else {
      PrintUsage();
      return 2;
    }
  }

  if (auto_mode) {
    LaunchProcmonAuto(procmon_path.empty() ? NULL : procmon_path.c_str());
    SetExternalLoggerEvent();
  }
  if (device.empty()) {
    device = "\\\\.\\ProcmonDebugLogger";
  }
  if (!WaitForDevice(device, 50, 200)) {
    char buf[256];
    sprintf(buf, "Failed to open device: %s\n", device.c_str());
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)lstrlenA(buf), &written, NULL);
    return 3;
  }

  HANDLE h = CreateFileA(device.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE) {
    DWORD err = GetLastError();
    char buf[256];
    sprintf(buf, "CreateFile failed: %lu\n", (unsigned long)err);
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)lstrlenA(buf), &written, NULL);
    return 3;
  }

  if (ioctl_code != 0) {
    std::vector<unsigned char> inbuf;
    std::vector<unsigned char> outbuf;
    if (!inhex.empty()) {
      if (!ParseHex(inhex, inbuf)) {
        PrintUsage();
        CloseHandle(h);
        return 4;
      }
    }
    if (outlen > 0) {
      outbuf.resize(outlen);
    }
    DWORD bytes_ret = 0;
    BOOL ok = DeviceIoControl(h, ioctl_code,
                              inbuf.empty() ? NULL : &inbuf[0], (DWORD)inbuf.size(),
                              outbuf.empty() ? NULL : &outbuf[0], (DWORD)outbuf.size(),
                              &bytes_ret, NULL);
    char buf[256];
    sprintf(buf, "DeviceIoControl 0x%08lX ok=%d bytes=%lu\n", (unsigned long)ioctl_code, ok ? 1 : 0, (unsigned long)bytes_ret);
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)lstrlenA(buf), &written, NULL);
  }

  HANDLE dump = CreateFileA(dump_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (dump == INVALID_HANDLE_VALUE) {
    CloseHandle(h);
    return 5;
  }

  std::vector<unsigned char> buffer(64 * 1024);
  while (true) {
    DWORD bytes_read = 0;
    BOOL ok = ReadFile(h, &buffer[0], (DWORD)buffer.size(), &bytes_read, NULL);
    if (!ok) {
      DWORD err = GetLastError();
      char buf[256];
      sprintf(buf, "ReadFile failed: %lu\n", (unsigned long)err);
      DWORD written = 0;
      WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, (DWORD)lstrlenA(buf), &written, NULL);
      break;
    }
    if (bytes_read == 0) {
      Sleep(10);
      continue;
    }
    DWORD tick = GetTickCount();
    WriteRawDump(dump, tick, &buffer[0], bytes_read);
  }

  CloseHandle(dump);
  CloseHandle(h);
  return 0;
}
