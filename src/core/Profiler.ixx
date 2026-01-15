export module Core.Profiler;

import std;

export using ProfilerOptions = std::uint32_t;

enum ProfilerFlags : ProfilerOptions
{
	PROFILE_FLAG_OBJECT_COUNT = 1 << 0,
	PROFILE_FLAG_FRAMETIME = 1 << 1,
	PROFILE_FLAG_GPU_USAGE = 1 << 2,
	PROFILE_FLAG_CPU_USAGE = 1 << 3,
	PROFILE_FLAG_RAM_USAGE = 1 << 4,
	PROFILE_FLAG_GPU_BUFFERS = 1 << 5,
	PROFILE_FLAG_1P_LOW_FRAMETIME = 1 << 6,
};
export std::string ProfilerFlagsToString(ProfilerOptions options);

export class Profiler
{
public:
	static Profiler* Get();
	void SetFlags(ProfilerOptions options);
	void Update(float delta);

	const std::vector<float>&         GetCPU()        { return CPUUsage.buffer;          }
	const std::vector<float>&         GetGPU()        { return GPUUsage.buffer;          }
	const std::vector<float>&         GetFrameTime()  { return frameTime.buffer;         }
	const std::vector<std::uint64_t>& GetRAM()        { return ramUsed.buffer;           }
	const std::vector<std::size_t>    GetVertexSize() { return vertexBufferUsage.buffer; }
	const std::vector<std::size_t>    GetIndexSize()  { return indexBufferUsage.buffer;  }

	float Get1PercentLowFrameTime() { return frameTime1PLow; }

	static constexpr ProfilerOptions ALL_OPTIONS = std::numeric_limits<std::uint32_t>::max();

private:
	template<typename T> struct ScrollingBuffer
	{
		ScrollingBuffer(int size, int offset = 0) : size(size), offset(offset)
		{
			buffer.resize(size);
		}

		std::vector<T> buffer;
		int size = 0;
		int offset = 0;

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

	void OneSecondUpdate();
	void Calculate1PLow();

	ProfilerOptions options = 0;

	float frameTime1PLow = 0;
	ScrollingBuffer<float> CPUUsage{ 100 };
	ScrollingBuffer<float> GPUUsage{ 100 };
	ScrollingBuffer<float> frameTime{ 100 };
	ScrollingBuffer<std::uint64_t> ramUsed{ 100 };
	ScrollingBuffer<std::size_t> vertexBufferUsage{ 100 };
	ScrollingBuffer<std::size_t> indexBufferUsage{ 100 };
};