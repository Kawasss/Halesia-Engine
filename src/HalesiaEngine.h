#pragma once
#include <optional>
#include <future>
#include "system/Window.h"
#include "system/Input.h"

class Renderer;
class Scene;

template<typename T> struct ScrollingBuffer //dont know where to put this struct, maybe make it private
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

enum HalesiaExitCode
{
	HALESIA_EXIT_CODE_SUCESS,            // succes means that the engine was told to exit or reached the end of its loop, i.e. the game window closes
	HALESIA_EXIT_CODE_UNKNOWN_EXCEPTION, // an unknown exception occured, since the cause is unknown the engine has to terminate
	HALESIA_EXIT_CODE_EXCEPTION          // an error was thrown, this can be user defined or engine defined
};
inline extern std::string HalesiaExitCodeToString(HalesiaExitCode exitCode);

struct HalesiaEngineCreateInfo
{
	Scene* startingScene = nullptr;
	std::string sceneFile = "";
	Win32WindowCreateInfo windowCreateInfo{};
	VirtualKey devConsoleKey = VirtualKey::Tilde;
	bool enableDevConsole = true;
	bool useEditor = false;
	bool playIntro = true;

	int argsCount = 0;
	char** args;
};

struct EngineCore
{
	Renderer* renderer;
	Win32Window* window;
	Scene* scene;
	int maxFPS = -1;
};

class HalesiaEngine
{
public:
	static void SetCreateInfo(const HalesiaEngineCreateInfo& createInfo);
	static HalesiaEngine* GetInstance(); //dont know if its better to return a pointer or the whole class or write to a reference
	EngineCore& GetEngineCore();
	HalesiaExitCode Run();
	void LoadScene(Scene* newScene);
	void Destroy();

	bool pauseGame = false; //these bools dont reallyy need to be here
	bool playOneFrame = false;
	bool showFPS = false;
	bool showRAM = false;
	bool showCPU = false;
	bool showGPU = false;
	bool showAsyncTimes = false;
	bool showObjectData = false;
	bool showWindowData = false;
	bool useEditor = false;
	Scene* scene = nullptr;

private:
	struct UpdateRendererData
	{
		float delta;
	};

	struct UpdateSceneData
	{
		float delta;
		bool pauseGame;
		bool& playOneFrame;
	};

	void CheckInput();
	void UpdateAsyncCompletionTimes(float frameDelta);
	void UpdateCGPUUsage();
	void RegisterConsoleVars();

	void OnLoad(HalesiaEngineCreateInfo& createInfo);
	void LoadVars();
	void OnExit();

	void UpdateRenderer(float delta);
	void UpdateScene(float delta);

	static HalesiaEngineCreateInfo createInfo;
	EngineCore core;

	bool playIntro = true;
	bool devKeyIsPressedLastFrame = false;
	bool renderDevConsole = false;
	VirtualKey devConsoleKey;

	Renderer* renderer;
	Win32Window* window = nullptr;

	float asyncRendererCompletionTime = 0;
	float asyncScriptsCompletionTime = 0;
	std::vector<float> asyncTimes;

	std::future<void> asyncScripts;
	std::future<void> asyncRenderer;

	ScrollingBuffer<float> CPUUsage{ 100 };
	ScrollingBuffer<float> GPUUsage{ 100 };
	ScrollingBuffer<uint64_t> ramUsed{ 500 };
};