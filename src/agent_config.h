#pragma once

#include <string>

struct AgentConfig {
  std::string logstash_url;
  std::string static_host;
  std::string static_env;
  std::string raw_dump_path;
  std::string procmon_path;
  std::string hook_dll_path;
  std::string ioctl_dump_path;
  int batch_max;
  int batch_interval_ms;
};

bool LoadConfig(const char* path, AgentConfig& out);
