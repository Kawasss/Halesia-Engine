#pragma once
#include <utility>

namespace win32
{
	#include <Windows.h>

	class CriticalSection
	{
	public:
		CriticalSection()
		{
			InitializeCriticalSection(&section);
			valid = true;
		}

		~CriticalSection()
		{
			Destroy();
		}

		CriticalSection(CriticalSection&& c) noexcept
		{
			Destroy();

			std::swap(this->section, c.section);
			std::swap(this->valid, c.valid);
		}

		CriticalSection(const CriticalSection&) = delete;
		CriticalSection& operator=(CriticalSection&&) = delete;

		void Lock()
		{
			if (valid)
			 EnterCriticalSection(&section);
		}

		void Unlock()
		{
			if (valid)
			 LeaveCriticalSection(&section);
		}

	private:
		void Destroy()
		{
			if (!valid)
				return;

			DeleteCriticalSection(&section);
			valid = false;
		}

		bool valid = false;
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