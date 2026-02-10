#include "agent_config.h"

#include <windows.h>

static std::string GetIniString(const char* path, const char* key, const char* def) {
  char buf[512];
  buf[0] = '\0';
  GetPrivateProfileStringA("agent", key, def, buf, sizeof(buf), path);
  return std::string(buf);
}

static int GetIniInt(const char* path, const char* key, int def) {
  return static_cast<int>(GetPrivateProfileIntA("agent", key, def, path));
}

bool LoadConfig(const char* path, AgentConfig& out) {
  out.logstash_url = GetIniString(path, "logstash_url", "http://127.0.0.1:8080/");
  out.static_host = GetIniString(path, "static_host", "XP-AGENT");
  out.static_env = GetIniString(path, "static_env", "lab");
  out.raw_dump_path = GetIniString(path, "raw_dump_path", "procmon-raw.bin");
  out.procmon_path = GetIniString(path, "procmon_path", "C:\\Documents and Settings\\trevor\\Desktop\\SysinternalsSuite\\procmon.exe");
  out.hook_dll_path = GetIniString(path, "hook_dll_path", "procmon_hook.dll");
  out.ioctl_dump_path = GetIniString(path, "ioctl_dump_path", "ioctls.bin");
  out.batch_max = GetIniInt(path, "batch_max", 100);
  out.batch_interval_ms = GetIniInt(path, "batch_interval_ms", 2000);
  return true;
}
