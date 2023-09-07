#pragma once
#include "renderer/Renderer.h"
#include "Physics.h"
#include "system/Window.h"
#include "Scene.h"
#include "system/Input.h"

enum HalesiaExitCode
{
	EXIT_CODE_SUCESS,            // succes means that the engine was told to exit or reached the end of its loop, i.e. the game window closes
	EXIT_CODE_UNKNOWN_EXCEPTION, // an unknown exception occured, since the cause is unknown the engine has to terminate
	EXIT_CODE_EXCEPTION          // an error was thrown, this can be user defined or engine defined
};

struct HalesiaInstanceCreateInfo
{
	Scene* startingScene = nullptr;
	Win32WindowCreateInfo windowCreateInfo{};
	VirtualKey devConsoleKey = VirtualKey::Tilde;
	bool enableDevConsole = true;
	bool useEditor = false;

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
	int minimumFPS = 60;
	Scene* scene;

private:
	bool renderDevConsole = false;
	VirtualKey devConsoleKey;

	Renderer* renderer;
	Physics* physics;
	Win32Window* window;
};