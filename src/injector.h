#pragma once

#include <string>

void AppendAgentLog(const char* msg);
void AppendAgentLog2(const char* msg, DWORD value);
std::string GetExeDir();
bool LaunchProcmonAndInject(const std::string& procmon_path, const std::string& dll_path, unsigned long& out_pid);
