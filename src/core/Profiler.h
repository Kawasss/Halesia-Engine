#pragma once
#include <cstdint>

typedef uint32_t ProfilerOptions;

enum ProfilerFlags
{
	PROFILE_FLAG_OBJECT_COUNT,
	PROFILE_FLAG_FRAME_TIME,
};

class Profiler
{
public:
	static Profiler* Get();
	void SetFlags(ProfilerOptions options);

private:
	ProfilerOptions options;
};