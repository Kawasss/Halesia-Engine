module;

#include <Windows.h>
#include <Psapi.h>
#include <intrin.h>

#include <pdh.h>
#include <pdhmsg.h>
#include <strsafe.h>
#include <tchar.h>

#pragma comment(lib, "pdh.lib")

module System.Metrics;

import std;

std::uint64_t GetPhysicalMemoryUsedByApp()
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

// https://docs.microsoft.com/en-us/windows/win32/perfctrs/enumerating-process-objects
std::vector<std::pair<int, int>> GetGPURunningTimeProcess() {
    std::vector<std::pair<int, int>> ret;

    DWORD counterListSize = 0;
    DWORD instanceListSize = 0;
    DWORD dwFlags = 0;
    const auto COUNTER_OBJECT = TEXT("GPU Engine");
    PDH_STATUS status = ERROR_SUCCESS;
    status = PdhEnumObjectItems(nullptr, nullptr, COUNTER_OBJECT, nullptr,
        &counterListSize, nullptr, &instanceListSize,
        PERF_DETAIL_WIZARD, dwFlags);
    if (status != PDH_MORE_DATA) {
        throw std::runtime_error("failed PdhEnumObjectItems()");
    }

    std::vector<TCHAR> counterList(counterListSize);
    std::vector<TCHAR> instanceList(instanceListSize);
    status = ::PdhEnumObjectItems(
        nullptr, nullptr, COUNTER_OBJECT, counterList.data(), &counterListSize,
        instanceList.data(), &instanceListSize, PERF_DETAIL_WIZARD, dwFlags);
    if (status != ERROR_SUCCESS) {
        throw std::runtime_error("failed PdhEnumObjectItems()");
    }

    for (TCHAR* pTemp = instanceList.data(); *pTemp != 0;
        pTemp += _tcslen(pTemp) + 1) {
        if (::_tcsstr(pTemp, TEXT("engtype_3D")) == NULL) {
            continue;
        }

        TCHAR buffer[1024];
        ::StringCchCopy(buffer, 1024, TEXT("\\GPU Engine("));
        ::StringCchCat(buffer, 1024, pTemp);
        ::StringCchCat(buffer, 1024, TEXT(")\\Running time"));

        HQUERY hQuery = NULL;
        status = ::PdhOpenQuery(NULL, 0, &hQuery);
        if (status != ERROR_SUCCESS) {
            continue;
        }

        HCOUNTER hCounter = NULL;
        status = ::PdhAddCounter(hQuery, buffer, 0, &hCounter);
        if (status != ERROR_SUCCESS) {
            continue;
        }

        status = ::PdhCollectQueryData(hQuery);
        if (status != ERROR_SUCCESS) {
            continue;
        }

        status = ::PdhCollectQueryData(hQuery);
        if (status != ERROR_SUCCESS) {
            continue;
        }

        const DWORD dwFormat = PDH_FMT_LONG;
        PDH_FMT_COUNTERVALUE ItemBuffer;
        status =
            ::PdhGetFormattedCounterValue(hCounter, dwFormat, nullptr, &ItemBuffer);
        if (ERROR_SUCCESS != status) {
            continue;
        }

        if (ItemBuffer.longValue > 0) {
#ifdef _UNICODE
            std::wregex re(TEXT("pid_(\\d+)"));
            std::wsmatch sm;
            std::wstring str = pTemp;
#else
            std::regex re(TEXT("pid_(\\d+)"));
            std::smatch sm;
            std::string str = pTemp;
#endif
            if (std::regex_search(str, sm, re)) {
                int pid = std::stoi(sm[1]);
                ret.push_back({ pid, ItemBuffer.longValue });
            }
        }

        ::PdhCloseQuery(hQuery);
    }

    return ret;
}

std::int64_t GetGPURunningTimeTotal() {
    std::int64_t total = 0;
    std::vector<std::pair<int, int>> list = GetGPURunningTimeProcess();
    for (const std::pair<int, int>& v : list) {
        if (v.second > 0) {
            total += v.second;
        }
    }
    return total;
}

double GetGPUUsage() {
    static std::chrono::steady_clock::time_point prev_called =
        std::chrono::steady_clock::now();
    static std::int64_t prev_running_time = GetGPURunningTimeTotal();

    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    std::chrono::steady_clock::duration elapsed = now - prev_called;

    std::int64_t elapsed_sec =
        std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
    std::int64_t running_time = GetGPURunningTimeTotal();

    double percentage =
        (double)(running_time - prev_running_time) / elapsed_sec * 100;
    // printf("percent = (%lld - %lld) / %lld * 100 = %f\n", running_time,
    // prev_running_time, elapsed_sec, percentage);

    prev_called = now;
    prev_running_time = running_time;

    if (percentage > 1.0)
        percentage = 1.0;
    else if (percentage < 0.0)
        percentage = 0.0;
    return percentage;
}