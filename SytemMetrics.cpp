#include "SystemMetrics.h"
#include "psapi.h"

uint64_t GetPhysicalMemoryUsedByApp()
{
    PROCESS_MEMORY_COUNTERS_EX memoryCounter;
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&memoryCounter, sizeof(memoryCounter));
    return memoryCounter.WorkingSetSize;
}

bool init = false;
FILETIME lastUser, lastIdle, lastKernel;
float GetCPUPercentageUsedByApp()
{
    if (!init)
    {
        GetSystemTimes(&lastIdle, &lastKernel, &lastUser);
        init = true;
        return 0;
    }

    FILETIME user, idle, kernel;
    GetSystemTimes(&idle, &kernel, &user);
    
    //no clue if this can be done better, dont really care;
    uint64_t relativeUser = ULARGE_INTEGER{ user.dwLowDateTime, user.dwHighDateTime }.QuadPart - ULARGE_INTEGER{ lastUser.dwLowDateTime, lastUser.dwHighDateTime }.QuadPart;
    uint64_t relativeIdle = ULARGE_INTEGER{ idle.dwLowDateTime, idle.dwHighDateTime }.QuadPart - ULARGE_INTEGER{ lastIdle.dwLowDateTime, lastIdle.dwHighDateTime }.QuadPart;
    uint64_t relativeKernel = ULARGE_INTEGER{ kernel.dwLowDateTime, kernel.dwHighDateTime }.QuadPart - ULARGE_INTEGER{ lastKernel.dwLowDateTime, lastKernel.dwHighDateTime }.QuadPart;
    
    uint64_t relativeSystem = relativeUser + relativeKernel;

    lastUser = user;
    lastIdle = idle;
    lastKernel = kernel;

    if (relativeSystem == 0) return -1; //cant divide by 0, so return prematurely

    return float((relativeSystem - relativeIdle) / (float)relativeSystem * 100);
}

//Returns the last Win32 error, in string format. Returns an empty string if there is no error.
std::string GetLastErrorAsString()
{
    //Get the error message ID, if any.
    DWORD errorMessageID = ::GetLastError();
    if (errorMessageID == 0) {
        return std::string(); //No error message has been recorded
    }

    LPSTR messageBuffer = nullptr;

    //Ask Win32 to give us the string version of that message ID.
    //The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

    //Copy the error message into a std::string.
    std::string message(messageBuffer, size);

    //Free the Win32's string's buffer.
    LocalFree(messageBuffer);

    return message;
}

SystemInformation GetCpuInfo()
{
    SystemInformation info{};

    // 4 is essentially hardcoded due to the __cpuid function requirements.
    // NOTE: Results are limited to whatever the sizeof(int) * 4 is...
    std::array<int, 4> integerBuffer = {};
    constexpr size_t sizeofIntegerBuffer = sizeof(int) * integerBuffer.size();

    std::array<char, 64> charBuffer = {};

    // The information you wanna query __cpuid for.
    // https://learn.microsoft.com/en-us/cpp/intrinsics/cpuid-cpuidex?view=vs-2019
    constexpr std::array<int, 3> functionIds = {
        // Manufacturer
        //  EX: "Intel(R) Core(TM"
        0x8000'0002,
        // Model
        //  EX: ") i7-8700K CPU @"
        0x8000'0003,
        // Clockspeed
        //  EX: " 3.70GHz"
        0x8000'0004
    };

    for (int id : functionIds)
    {
        // Get the data for the current ID.
        __cpuid(integerBuffer.data(), id);

        // Copy the raw data from the integer buffer into the character buffer
        std::memcpy(charBuffer.data(), integerBuffer.data(), sizeofIntegerBuffer);

        // Copy that data into a std::string
        info.CPUName += std::string(charBuffer.data());
    }

    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);

    MEMORYSTATUSEX buffer = { };
    memset(&buffer, 0, sizeof(MEMORYSTATUSEX));
    buffer.dwLength = sizeof(MEMORYSTATUSEX);
    if (!GlobalMemoryStatusEx(&buffer))
        throw std::runtime_error("Failed to acquire the physical memory from the Windows API, " + GetLastErrorAsString());
    ULONGLONG memory{};
    //memset(&memory, 0, sizeof(ULONGLONG));
    GetPhysicallyInstalledSystemMemory(&memory);
    info.processorCount = systemInfo.dwNumberOfProcessors;
    info.RAMAmount = (uint64_t)buffer.ullTotalPhys;
    info.installedRAM = (long)memory;

    return info;
}