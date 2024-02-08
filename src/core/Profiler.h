#pragma once
#include <cstdint>
#include <vector>

typedef uint32_t ProfilerOptions;

enum ProfilerFlags : ProfilerOptions
{
	PROFILE_FLAG_OBJECT_COUNT = 1 << 0,
	PROFILE_FLAG_FRAME_TIME = 1 << 1,
	PROFILE_FLAG_GPU_USAGE = 1 << 2,
	PROFILE_FLAG_CPU_USAGE = 1 << 3,
	PROFILE_FLAG_RAM_USAGE = 1 << 4,
};


class Profiler
{
public:
	static Profiler* Get();
	void SetFlags(ProfilerOptions options);
	void Update(float delta);

	const std::vector<float>& GetCPU() { return CPUUsage.buffer; }
	const std::vector<float>& GetGPU() { return GPUUsage.buffer; }
	const std::vector<float>& GetFrameTime() { return frameTime.buffer; }
	const std::vector<uint64_t>& GetRAM() { return ramUsed.buffer; }

	static constexpr ProfilerOptions ALL_OPTIONS = PROFILE_FLAG_OBJECT_COUNT | PROFILE_FLAG_FRAME_TIME | PROFILE_FLAG_GPU_USAGE | PROFILE_FLAG_CPU_USAGE | PROFILE_FLAG_RAM_USAGE;

private:
	template<typename T> struct ScrollingBuffer
	{
		ScrollingBuffer(int size, int offset = 0)
		{
			this->size = size;
			this->offset = offset;
		}

		std::vector<T> buffer;
		int size = 0;
		int offset = 1;

		void Add(T value)
		{
			if (buffer.size() < size)
				buffer.push_back(value);
			else
			{
				buffer[offset] = value;
				offset = (offset + 1) % size;
			}
		}
	};

	ProfilerOptions options = 0;

	ScrollingBuffer<float> CPUUsage{ 100 };
	ScrollingBuffer<float> GPUUsage{ 100 };
	ScrollingBuffer<float> frameTime{ 100 };
	ScrollingBuffer<uint64_t> ramUsed{ 500 };
};