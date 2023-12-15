#pragma once
#include "renderer/Renderer.h"
#include "system/Window.h"
#include "Scene.h"
#include "SceneLoader.h"
#include "system/Input.h"

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

inline std::string HalesiaExitCodeToString(HalesiaExitCode exitCode)
{
	switch (exitCode)
	{
	case HALESIA_EXIT_CODE_SUCESS:
		return "HALESIA_EXIT_CODE_SUCESS";
	case HALESIA_EXIT_CODE_EXCEPTION:
		return "HALESIA_EXIT_CODE_EXCEPTION";
	case HALESIA_EXIT_CODE_UNKNOWN_EXCEPTION:
		return "HALESIA_EXIT_CODE_UNKNOWN_EXCEPTION";
	}
}

struct HalesiaInstanceCreateInfo
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

class HalesiaInstance
{
public:
	static void GenerateHalesiaInstance(HalesiaInstance& instance, HalesiaInstanceCreateInfo& createInfo); //dont know if its better to return a pointer or the whole class or write to a reference
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
	bool useEditor = false;
	int minimumFPS = 60;
	Scene* scene;

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

	std::optional<std::string> UpdateRenderer(const UpdateRendererData& rendererData);
	void UpdateScene(const UpdateSceneData& sceneData);

	bool playIntro = true;
	bool devKeyIsPressedLastFrame = false;
	bool renderDevConsole = false;
	VirtualKey devConsoleKey;

	Renderer* renderer;
	Win32Window* window;

	float asyncRendererCompletionTime = 0;
	float asyncScriptsCompletionTime = 0;
	std::vector<float> asyncTimes;

	std::future<void> asyncScripts;
	std::future<std::optional<std::string>> asyncRenderer;

	ScrollingBuffer<float> CPUUsage{ 100 };
	ScrollingBuffer<float> GPUUsage{ 100 };
	ScrollingBuffer<uint64_t> ramUsed{ 500 };
};