#include <Windows.h>
#include <intrin.h>
#include <string>
#include <array>
#include <iostream>

struct SystemInformation
{
    std::string CPUName = "";
    int processorCount = 0;
    uint64_t RAMAmount = 0;
    unsigned long installedRAM = 0;
};

//Returns the last Win32 error, in string format. Returns an empty string if there is no error.
std::string GetLastErrorAsString();
uint64_t GetPhysicalMemoryUsedByApp();
float GetCPUPercentageUsedByApp();
SystemInformation GetCpuInfo();
