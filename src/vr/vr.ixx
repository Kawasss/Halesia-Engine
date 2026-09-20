export module vr;

import std;

export namespace vr
{
	enum class Error
	{
		EarlierFailure, // init must likely failed
		Unsupported,
		NotAvailable,
	};

	enum class Status
	{
		NoRuntime,
		NoDevice,
		Good,
	};

	struct EyeDimension
	{
		std::uint32_t width;
		std::uint32_t height;
		std::uint32_t sampleCount;
	};

	void Init();
	void Destroy();

	Status GetStatus();

	namespace graphics
	{
		std::expected<std::array<EyeDimension, 2>, Error> GetEyeDimensions();

		std::vector<std::string> GetInstanceExtensions();
	}
}