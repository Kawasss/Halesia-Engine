#include "core/Profiler.h"

#include "system/SystemMetrics.h"

Profiler* Profiler::Get()
{
	static Profiler profiler;
	return &profiler;
}

void Profiler::SetFlags(ProfilerOptions options)
{
	this->options = options;
}

void Profiler::Update(float delta)
{
	static float timeSinceUpdate = 0;
	timeSinceUpdate += delta;

	if (options & PROFILE_FLAG_FRAME_TIME)
		frameTime.Add(delta);

	if (timeSinceUpdate < 1000)
		return;
	timeSinceUpdate = 0;

	if (options & PROFILE_FLAG_RAM_USAGE) ramUsed.Add(GetPhysicalMemoryUsedByApp() / (1024ULL * 1024));
	if (options & PROFILE_FLAG_CPU_USAGE) CPUUsage.Add(GetCPUPercentageUsedByApp());
	if (options & PROFILE_FLAG_GPU_USAGE) GPUUsage.Add((float)GetGPUUsage() * 100.0f);
}