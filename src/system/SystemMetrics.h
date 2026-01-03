#include <Windows.h>
#include <string>

//Returns the last Win32 error, in string format. Returns an empty string if there is no error.
std::string GetLastErrorAsString();
uint64_t GetPhysicalMemoryUsedByApp();
float GetCPUPercentageUsedByApp();
double GetGPUUsage();