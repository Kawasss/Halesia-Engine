#pragma once
#include <optional>
#include <future>
#include "system/Window.h"
#include "system/Input.h"
#include "renderer/Renderer.h"

class Renderer;
class Scene;
class Profiler;
class AnimationManager;

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
	Win32WindowCreateInfo windowCreateInfo{};
	VirtualKey devConsoleKey = VirtualKey::Tilde;
	bool enableDevConsole = true;
	bool playIntro = true;
	RendererFlags renderFlags;

	int argsCount = 0;
	char** args;
};

struct EngineCore
{
	Renderer* renderer = nullptr;
	Window* window = nullptr;
	Scene* scene = nullptr;
	Profiler* profiler = nullptr;
	AnimationManager* animationManager = nullptr;
	int maxFPS = -1;
};

class HalesiaEngine
{
public:
	static void Exit();

	static void SetCreateInfo(const HalesiaEngineCreateInfo& createInfo);
	static HalesiaEngine* GetInstance(); //dont know if its better to return a pointer or the whole class or write to a reference
	EngineCore& GetEngineCore();
	HalesiaExitCode Run();
	void LoadScene(Scene* newScene);
	void Destroy();

	bool pauseGame = false; //these bools dont reallyy need to be here
	bool playOneFrame = false;
	bool showFPS = false;
	bool showAsyncTimes = false;
	bool showObjectData = false;
	bool showWindowData = false;

private:
	void CheckInput();
	void UpdateAsyncCompletionTimes(float frameDelta);
	void RegisterConsoleVars();

	void OnLoad(HalesiaEngineCreateInfo& createInfo);
	void LoadVars();
	void OnExit();

	void PlayIntro();

	void UpdateRenderer(float delta);
	void UpdateScene(float delta);

	void InitializeCoreComponents();
	void InitializeSubSystems();

	static HalesiaEngineCreateInfo createInfo;
	EngineCore core;

	bool playIntro = true;
	bool devKeyIsPressedLastFrame = false;
	bool renderDevConsole = false;
	VirtualKey devConsoleKey;

	float asyncRendererCompletionTime = 0;
	float asyncScriptsCompletionTime = 0;
	std::vector<float> asyncTimes;

	std::future<void> asyncScripts;
	std::future<void> asyncRenderer;
};