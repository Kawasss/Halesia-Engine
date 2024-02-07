#include "core/Profiler.h"

Profiler* Profiler::Get()
{
	static Profiler profiler;
	return &profiler;
}

void Profiler::SetFlags(ProfilerOptions options)
{
	this->options = options;
}