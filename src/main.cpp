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

static bool ReadDeviceLoop(const AgentConfig& config, const char* device_name) {
  HANDLE h = CreateFileA(device_name, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE) {
    return false;
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
  AgentConfig config;
  if (!LoadConfig("agent.ini", config)) {
    return 1;
  }

  unsigned long pid = 0;
  LaunchProcmonAndInject(config.procmon_path, config.hook_dll_path, pid);

  const char* device_name = "\\\\.\\ProcmonDebugLogger";
  if (!ReadDeviceLoop(config, device_name)) {
    std::string payload = BuildTestEvent(config);
    HttpPostJson(config.logstash_url, payload);
    return 2;
  }

  return 0;
}
