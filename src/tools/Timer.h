#pragma once
#include <chrono>

enum TimerMetric
{
	TIMER_ELAPSED_NANOSECONDS,
	TIMER_ELAPSED_MICROSECONDS,
	TIMER_ELAPSED_MILLISECONDS,
	TIMER_ELAPSED_MINUTES
};

class Timer
{
public:
	Timer() {};
	
	void Start() { startTime = std::chrono::high_resolution_clock::now(); }
	void End()	 { endTime =   std::chrono::high_resolution_clock::now(); }

	float GetElapsedTime(TimerMetric metric = TIMER_ELAPSED_MILLISECONDS)
	{
		switch (metric)
		{
		case TIMER_ELAPSED_NANOSECONDS:
			return std::chrono::duration<float, std::chrono::nanoseconds::period>(endTime - startTime).count();
		case TIMER_ELAPSED_MICROSECONDS:
			return std::chrono::duration<float, std::chrono::microseconds::period>(endTime - startTime).count();
		case TIMER_ELAPSED_MILLISECONDS:
			return std::chrono::duration<float, std::chrono::milliseconds::period>(endTime - startTime).count();
		case TIMER_ELAPSED_MINUTES:
			return std::chrono::duration<float, std::chrono::minutes::period>(endTime - startTime).count();
		default:
			return std::chrono::duration<float, std::chrono::milliseconds::period>(endTime - startTime).count();
		}
	}

private:
	std::chrono::steady_clock::time_point startTime;
	std::chrono::steady_clock::time_point endTime;
};