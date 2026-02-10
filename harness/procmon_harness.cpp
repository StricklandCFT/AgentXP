#include <windows.h>
#include <string>
#include <vector>

static void PrintUsage() {
  const char* msg =
      "Usage: procmon_harness.exe --device \\\\.\\ProcmonDebugLogger --dump procmon-raw.bin "
      "[--ioctl 0x222004 --inhex deadbeef --outlen 4096]\n";
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

int main(int argc, char** argv) {
  std::string device = "\\\\.\\ProcmonDebugLogger";
  std::string dump_path = "procmon-raw.bin";
  DWORD ioctl_code = 0;
  std::string inhex;
  DWORD outlen = 0;

  for (int i = 1; i < argc; ++i) {
    if (lstrcmpiA(argv[i], "--device") == 0 && i + 1 < argc) {
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

  HANDLE h = CreateFileA(device.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE) {
    DWORD err = GetLastError();
    char buf[256];
    wsprintfA(buf, "CreateFile failed: %lu\n", err);
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
    wsprintfA(buf, "DeviceIoControl 0x%08lX ok=%d bytes=%lu\n", ioctl_code, ok ? 1 : 0, bytes_ret);
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
      wsprintfA(buf, "ReadFile failed: %lu\n", err);
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
