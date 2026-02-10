#pragma once

#include <string>

bool LaunchProcmonAndInject(const std::string& procmon_path, const std::string& dll_path, unsigned long& out_pid);
