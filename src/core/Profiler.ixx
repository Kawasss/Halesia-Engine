export module Core.Profiler;

import std;

import Templates.CircularBuffer;

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

	const std::vector<float>&         GetCPU()        { return CPUUsage.GetInternalBuffer();          }
	const std::vector<float>&         GetGPU()        { return GPUUsage.GetInternalBuffer();          }
	const std::vector<float>&         GetFrameTime()  { return frameTime.GetInternalBuffer();         }
	const std::vector<std::uint64_t>& GetRAM()        { return ramUsed.GetInternalBuffer();           }
	const std::vector<std::size_t>    GetVertexSize() { return vertexBufferUsage.GetInternalBuffer(); }
	const std::vector<std::size_t>    GetIndexSize()  { return indexBufferUsage.GetInternalBuffer();  }

	float Get1PercentLowFrameTime() { return frameTime1PLow; }

	static constexpr ProfilerOptions ALL_OPTIONS = std::numeric_limits<std::uint32_t>::max();

private:
	template<typename T>
	using RecordingBuffer = CircularBuffer<T, 100>;

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
	RecordingBuffer<float> CPUUsage;
	RecordingBuffer<float> GPUUsage;
	RecordingBuffer<float> frameTime;
	RecordingBuffer<std::uint64_t> ramUsed;
	RecordingBuffer<std::size_t> vertexBufferUsage;
	RecordingBuffer<std::size_t> indexBufferUsage;
};