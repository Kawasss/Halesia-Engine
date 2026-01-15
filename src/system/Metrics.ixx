export module System.Metrics;

import std;

export std::string GetLastErrorAsString();
export std::uint64_t GetPhysicalMemoryUsedByApp();
export float GetCPUPercentageUsedByApp();
export double GetGPUUsage();