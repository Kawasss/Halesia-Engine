#include <algorithm>

#include "core/Profiler.h"

#include "system/SystemMetrics.h"

#include "renderer/Renderer.h"
#include "renderer/gui.h"

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

	if (options & PROFILE_FLAG_FRAMETIME) frameTime.Add(delta);
	if (options & PROFILE_FLAG_1P_LOW_FRAMETIME) Calculate1PLow();
	
	if (options & PROFILE_FLAG_GPU_BUFFERS)
	{
		vertexBufferUsage.Add(Renderer::globalVertexBuffer.GetSize() / 1024ULL);
		indexBufferUsage.Add(Renderer::globalIndicesBuffer.GetSize() / 1024ULL);
	}
	GUI::ShowFrameTimeGraph(frameTime.buffer, frameTime1PLow);
	if (timeSinceUpdate < 1000)
		return;
	OneSecondUpdate();
	timeSinceUpdate = 0;
}

void Profiler::OneSecondUpdate()
{
	if (options & PROFILE_FLAG_RAM_USAGE) ramUsed.Add(GetPhysicalMemoryUsedByApp() / (1024ULL * 1024));
	if (options & PROFILE_FLAG_CPU_USAGE) CPUUsage.Add(GetCPUPercentageUsedByApp());
	if (options & PROFILE_FLAG_GPU_USAGE) GPUUsage.Add((float)GetGPUUsage() * 100.0f);
}

void Profiler::Calculate1PLow()
{
	const int onePercent = static_cast<int>(frameTime.size * 0.01f);

	std::vector<float> sorted = frameTime.buffer;
	std::sort(sorted.begin(), sorted.end());

	float average = 0;
	for (int i = 0; i < onePercent; i++)
		average += frameTime.buffer[i];
	average /= onePercent;
	frameTime1PLow = average;
}