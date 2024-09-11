#pragma once

namespace win32
{
	#include <Windows.h>

	class CriticalSection
	{
	public:
		CriticalSection()
		{
			InitializeCriticalSection(&section);
		}

		~CriticalSection()
		{
			DeleteCriticalSection(&section);
		}

		CriticalSection(const CriticalSection&) = delete;
		CriticalSection& operator=(CriticalSection&&) = delete;

		void Lock()
		{
			EnterCriticalSection(&section);
		}

		void Unlock()
		{
			LeaveCriticalSection(&section);
		}

	private:
		CRITICAL_SECTION section;
	};

	class CriticalLockGuard
	{
	public:
		CriticalLockGuard() = delete;
		CriticalLockGuard(const CriticalLockGuard&) = delete;
		CriticalLockGuard& operator=(CriticalLockGuard&&) = delete;

		CriticalLockGuard(CriticalSection& section) : lock(section)
		{
			lock.Lock();
		}

		~CriticalLockGuard()
		{
			lock.Unlock();
		}

	private:
		CriticalSection& lock;
	};
}