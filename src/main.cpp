#include "agent_config.h"
#include "http_sender.h"
#include "injector.h"

#include <windows.h>
#include <string>
#include <vector>

static std::string BuildTestEvent(const AgentConfig& config) {
  std::string payload = "{";
  payload += "\"@timestamp\":\"1970-01-01T00:00:00Z\",";
  payload += "\"event\":{\"action\":\"procmon_test\"},";
  payload += "\"host\":{\"name\":\"" + config.static_host + "\"},";
  payload += "\"labels\":{\"env\":\"" + config.static_env + "\"}";
  payload += "}";
  return payload;
}

static void AppendHex(std::string& out, const unsigned char* data, size_t len) {
  static const char* kHex = "0123456789ABCDEF";
  out.reserve(out.size() + len * 2);
  for (size_t i = 0; i < len; ++i) {
    unsigned char c = data[i];
    out.push_back(kHex[(c >> 4) & 0xF]);
    out.push_back(kHex[c & 0xF]);
  }
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

static std::string BuildRawEventJson(const AgentConfig& config, DWORD tick, const unsigned char* data, DWORD len) {
  std::string hex;
  AppendHex(hex, data, len);
  std::string payload = "{";
  payload += "\"event\":{\"action\":\"procmon_raw\"},";
  payload += "\"host\":{\"name\":\"" + config.static_host + "\"},";
  payload += "\"labels\":{\"env\":\"" + config.static_env + "\"},";
  payload += "\"procmon\":{\"tick\":" + std::to_string(tick) + ",";
  payload += "\"len\":" + std::to_string(len) + ",";
  payload += "\"raw_hex\":\"" + hex + "\"}";
  payload += "}";
  return payload;
}

static bool LoadFileBytes(const char* path, std::vector<unsigned char>& out) {
  out.clear();
  HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE) {
    return false;
  }
  DWORD size = GetFileSize(h, NULL);
  if (size == INVALID_FILE_SIZE || size == 0) {
    CloseHandle(h);
    return false;
  }
  out.resize(size);
  DWORD read = 0;
  BOOL ok = ReadFile(h, &out[0], size, &read, NULL);
  CloseHandle(h);
  return ok == TRUE && read == size;
}

static bool ReplayIoctlsFromFile(HANDLE h, const char* path) {
  std::vector<unsigned char> data;
  if (!LoadFileBytes(path, data)) {
    return false;
  }

  const unsigned char* p = &data[0];
  const unsigned char* end = p + data.size();
  while (p + 12 <= end) {
    DWORD code = *(const DWORD*)p; p += 4;
    DWORD in_len = *(const DWORD*)p; p += 4;
    DWORD out_len = *(const DWORD*)p; p += 4;
    if (p + in_len > end) {
      break;
    }
    const unsigned char* in_buf = in_len ? p : NULL;
    p += in_len;

    std::vector<unsigned char> out_buf;
    if (out_len) {
      out_buf.resize(out_len);
    }
    DWORD bytes_ret = 0;
    DeviceIoControl(h, code,
                    (LPVOID)in_buf, in_len,
                    out_buf.empty() ? NULL : &out_buf[0], out_len,
                    &bytes_ret, NULL);
  }
  return true;
}

static bool WaitForIoctlsFile(const std::string& path, int retries, int delay_ms) {
  for (int i = 0; i < retries; ++i) {
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
      DWORD size = GetFileSize(h, NULL);
      CloseHandle(h);
      if (size != INVALID_FILE_SIZE && size > 0) {
        return true;
      }
    }
    Sleep(delay_ms);
  }
  return false;
}

static bool ReadDeviceLoop(const AgentConfig& config, const char* device_name) {
  HANDLE h = CreateFileA(device_name, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE) {
    return false;
  }

  if (!config.ioctl_dump_path.empty()) {
    ReplayIoctlsFromFile(h, config.ioctl_dump_path.c_str());
  }

  HANDLE dump = INVALID_HANDLE_VALUE;
  if (!config.raw_dump_path.empty()) {
    dump = CreateFileA(config.raw_dump_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  }

  std::vector<unsigned char> buffer(64 * 1024);
  while (true) {
    DWORD bytes_read = 0;
    BOOL ok = ReadFile(h, &buffer[0], (DWORD)buffer.size(), &bytes_read, NULL);
    if (!ok) {
      break;
    }
    if (bytes_read == 0) {
      Sleep(10);
      continue;
    }

    DWORD tick = GetTickCount();
    WriteRawDump(dump, tick, &buffer[0], bytes_read);

    std::string payload = BuildRawEventJson(config, tick, &buffer[0], bytes_read);
    HttpPostJson(config.logstash_url, payload);
  }

  if (dump != INVALID_HANDLE_VALUE) {
    CloseHandle(dump);
  }
  CloseHandle(h);
  return true;
}

int main(int argc, char** argv) {
  AppendAgentLog("ProcmonAgent: startup");
  AgentConfig config;
  if (!LoadConfig("agent.ini", config)) {
    AppendAgentLog("ProcmonAgent: LoadConfig failed");
    return 1;
  }

  unsigned long pid = 0;
  if (!LaunchProcmonAndInject(config.procmon_path, config.hook_dll_path, pid)) {
    AppendAgentLog("ProcmonAgent: injection failed");
  } else {
    AppendAgentLog("ProcmonAgent: injection ok");
  }

  if (!config.ioctl_dump_path.empty()) {
    WaitForIoctlsFile(config.ioctl_dump_path, 50, 200);
  }

  const char* device_name = "\\\\.\\ProcmonDebugLogger";
  if (!ReadDeviceLoop(config, device_name)) {
    std::string payload = BuildTestEvent(config);
    HttpPostJson(config.logstash_url, payload);
    return 2;
  }

  return 0;
}
