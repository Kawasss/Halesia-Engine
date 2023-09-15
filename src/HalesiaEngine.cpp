#include <iostream>
#include <future>
#include "HalesiaEngine.h"
#include "system/Input.h"
#include "Console.h"
#include "system/SystemMetrics.h"
#include "CreationObjects.h"

template<typename T> struct ScrollingBuffer //dont know where to put this struct
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

int ParseAndValidateDimensionArgument(std::string string)
{
	int i = std::stoi(string.substr(string.find(' '), string.size() - 1));
	if (i <= 0)
		throw std::runtime_error("Incorrect argument was given, it must be higher than 0, not " + std::to_string(i));
	return i;
}

void DetermineArgs(int argsCount, char** args, Win32WindowCreateInfo& createInfo)
{
#ifdef _DEBUG
	std::cout << "Received the following argument(s):" << std::endl;
#endif
	for (int i = 0; i < argsCount; i++)
	{
		std::string arg = args[i];
#ifdef _DEBUG
		std::cout << "  " << arg << std::endl;
#endif
		switch (arg[0])
		{
		case 'w':
			createInfo.width = ParseAndValidateDimensionArgument(arg);
			break;
		case 'h':
			createInfo.height = ParseAndValidateDimensionArgument(arg);
			break;
		}
	}
}

Renderer* localRenderer; // really bad behavior
MeshCreationObjects GetMeshCreationObjects()
{
	return localRenderer->GetVulkanCreationObjects();
}

void HalesiaInstance::GenerateHalesiaInstance(HalesiaInstance& instance, HalesiaInstanceCreateInfo& createInfo)
{
	try
	{
		Console::WriteLine("Write \"help\" for all commands");
		instance.physics = new Physics();
		instance.devConsoleKey = createInfo.devConsoleKey;

		if (createInfo.argsCount > 1)
			DetermineArgs(createInfo.argsCount, createInfo.args, createInfo.windowCreateInfo);
		
		instance.window = new Win32Window(createInfo.windowCreateInfo);
		instance.renderer = new Renderer(instance.window);
		localRenderer = instance.renderer;
		Scene::GetMeshCreationObjects = &GetMeshCreationObjects;

		if (createInfo.startingScene == nullptr)
		{
			Console::WriteLine("The given HalesiaInstanceCreateInfo doesn't contain a valid starting scene", MESSAGE_SEVERITY_ERROR);
			instance.scene = new Scene();
		}
		else
			instance.scene = createInfo.startingScene;
		if (createInfo.sceneFile != "")
			instance.scene->LoadScene(createInfo.sceneFile);
	}
	catch (const std::exception& e) //catch any normal exception and return
	{
		std::string fullError = e.what();
		MessageBoxA(nullptr, fullError.c_str(), "Engine error", MB_OK | MB_ICONERROR);
		std::cerr << e.what() << std::endl;
		return;
	}
	catch (...) //catch any unknown exceptions and return, doesnt catch any read or write errors etc.
	{
		MessageBoxA(nullptr, "Caught an unknown error, this build is most likely corrupt and can't be used.", "Unknown engine error", MB_OK | MB_ICONERROR);
		return;
	}
}

void HalesiaInstance::Destroy()
{
	renderer->Destroy();
	scene->Destroy();
	delete physics;
	delete window;
}

void HalesiaInstance::LoadScene(Scene* newScene)
{
	scene->Destroy();
	scene = newScene;
}

struct UpdateSceneData
{
	Scene* scene;
	Win32Window* window;
	float delta;
	float* timeToComplete;
	bool pauseGame;
	bool* playOneFrame;
};

void UpdateScene(UpdateSceneData* sceneData)
{
	std::chrono::steady_clock::time_point begin = std::chrono::high_resolution_clock::now();
	
	if (!sceneData->pauseGame || *sceneData->playOneFrame)
	{
		sceneData->scene->Update(sceneData->window, sceneData->delta);
		*sceneData->playOneFrame = false;
	}
	*sceneData->timeToComplete = std::chrono::duration<float, std::chrono::milliseconds::period>(std::chrono::high_resolution_clock::now() - begin).count();
}

void WriteAllCommandsToConsole()
{
	Console::WriteLine("-----------------------------------------------");
	Console::WriteLine("\"pauseGame 1 or 0\" pauses or unpauses the game");      // or ctrl + F1
	Console::WriteLine("\"showFPS 1 or 0\" shows or hides the FPS counter");     // or ctrl + F2
	Console::WriteLine("\"showRAM 1 or 0\" shows or hides the RAM usage graph"); // or ctrl + F3
	Console::WriteLine("\"showCPU 1 or 0\" shows or hides the CPU usage graph"); // or ctrl + F4
	Console::WriteLine("\"showGPU 1 or 0\" shows or hides the CPU usage graph"); // or ctrl + F5
	Console::WriteLine("\"showAsyncTimes 1 or 0\" shows or hides the the time it took to run the async tasks"); // or ctrl + F6
	Console::WriteLine("\"playOneFrame 1\" plays one frame");
	Console::WriteLine("\"exit\" terminates the program");
	Console::WriteLine("-----------------------------------------------");
}

void HandleConsoleCommand(std::string command)
{
	if (command == "")
		return;

	if (command != "help")
		Console::InterpretCommand(command);
	else
		WriteAllCommandsToConsole();
}

ScrollingBuffer<float> CPUUsage(100);
ScrollingBuffer<float> GPUUsage(100);
ScrollingBuffer<uint64_t> ramUsed(500);

struct UpdateRendererData
{
	Renderer* renderer;
	Camera* camera;
	std::vector<Object*> objects;
	float delta;
	float* timeToComplete;
	float* pieChartValues;
	bool renderDevConsole;
	bool showFPS;
	bool showRAM;
	bool showCPU;
	bool showGPU;
	bool showAsyncTimes;
};

std::optional<std::string> UpdateRenderer(UpdateRendererData* rendererData)
{
	
	std::chrono::steady_clock::time_point begin = std::chrono::high_resolution_clock::now();
	std::optional<std::string> command = rendererData->renderer->RenderDevConsole(rendererData->renderDevConsole);
	if (rendererData->showFPS)
		rendererData->renderer->RenderFPS(1 / rendererData->delta * 1000);

	ramUsed.Add(GetPhysicalMemoryUsedByApp() / (1024 * 1024));
	if (rendererData->showRAM)
		rendererData->renderer->RenderGraph(ramUsed.buffer, "RAM in MB");
	if (rendererData->showCPU)
		rendererData->renderer->RenderGraph(CPUUsage.buffer, "CPU %");
	if (rendererData->showGPU)
		rendererData->renderer->RenderGraph(GPUUsage.buffer, "GPU %");
	if (rendererData->showAsyncTimes)
		rendererData->renderer->RenderPieGraph(rendererData->pieChartValues, "Async Times (µs)");

	rendererData->renderer->DrawFrame(rendererData->objects, rendererData->camera, rendererData->delta);

	*rendererData->timeToComplete = std::chrono::duration<float, std::chrono::milliseconds::period>(std::chrono::high_resolution_clock::now() - begin).count();
	return command;
}

HalesiaExitCode HalesiaInstance::Run()
{
	bool devKeyIsPressedLastFrame = false;
	std::string lastCommand;
	float timeSinceLastCPUUpdate = 0;
	float timeSinceLastGraphUpdate = 0;
	float dummy[] = {0, 0, 0};
	float* pieChartValuesPtr = dummy;

	Console::commandVariables["pauseGame"] = &pauseGame;
	Console::commandVariables["showFPS"] = &showFPS;
	Console::commandVariables["playOneFrame"] = &playOneFrame;
	Console::commandVariables["showRAM"] = &showRAM;
	Console::commandVariables["showCPU"] = &showCPU;
	Console::commandVariables["showGPU"] = &showGPU;
	Console::commandVariables["showAsyncTimes"] = &showAsyncTimes;

	if (renderer == nullptr || window == nullptr || physics == nullptr)
		return EXIT_CODE_UNKNOWN_EXCEPTION;

	try
	{
		float frameDelta = 0;
		float asyncRendererCompletionTime = 0;
		float asyncScriptsCompletionTime = 0;
		std::chrono::steady_clock::time_point timeSinceLastFrame = std::chrono::high_resolution_clock::now();

		//scene->Start();
		window->maximized = true;

		while (!window->ShouldClose())
		{
			//if (!scene->HasFinishedLoading())
			//	continue; // quick patch to prevent read access violation

			showFPS = !showFPS ? Input::IsKeyPressed(VirtualKey::LeftControl) && Input::IsKeyPressed(VirtualKey::F1) : true;
			pauseGame = !pauseGame ? Input::IsKeyPressed(VirtualKey::LeftControl) && Input::IsKeyPressed(VirtualKey::F2) : true;
			showRAM = !showRAM ? Input::IsKeyPressed(VirtualKey::LeftControl) && Input::IsKeyPressed(VirtualKey::F3) : true;
			showCPU = !showCPU ? Input::IsKeyPressed(VirtualKey::LeftControl) && Input::IsKeyPressed(VirtualKey::F4) : true;
			showGPU = !showGPU ? Input::IsKeyPressed(VirtualKey::LeftControl) && Input::IsKeyPressed(VirtualKey::F5) : true;
			showAsyncTimes = !showAsyncTimes ? Input::IsKeyPressed(VirtualKey::LeftControl) && Input::IsKeyPressed(VirtualKey::F6) : true;

			UpdateSceneData sceneData{ scene, window, frameDelta, &asyncScriptsCompletionTime, pauseGame, &playOneFrame };
			std::future<void> asyncScripts = std::async(UpdateScene, &sceneData);

			UpdateRendererData rendererData{ renderer, scene->camera, scene->allObjects, frameDelta, &asyncRendererCompletionTime, pieChartValuesPtr, renderDevConsole, showFPS, showRAM, showCPU, showGPU, showAsyncTimes };
			std::future<std::optional<std::string>> asyncRenderer = std::async(UpdateRenderer, &rendererData);

			//if (window->ContainsDroppedFile())
			//	scene->SubmitStaticModel(window->GetDroppedFile(), renderer->GetMeshCreationObjects());

			if (!Input::IsKeyPressed(devConsoleKey) && devKeyIsPressedLastFrame)
				renderDevConsole = !renderDevConsole;

			if (Input::IsKeyPressed(VirtualKey::Q))
				window->LockCursor();
			if (Input::IsKeyPressed(VirtualKey::E))
				window->UnlockCursor();

			if (timeSinceLastCPUUpdate > 300)
			{
				float cpu = GetCPUPercentageUsedByApp();
				if (cpu != -1 && cpu != 0) //dont know if 0 is junk data
					CPUUsage.Add(cpu);

				float gpu = GetGPUUsage() * 100; //doesnt look incredibly accurate but it works good enough
				GPUUsage.Add(gpu);

				timeSinceLastCPUUpdate = 0;
			}
			timeSinceLastCPUUpdate += frameDelta;

			playOneFrame = Input::IsKeyPressed(VirtualKey::RightArrow);

			devKeyIsPressedLastFrame = Input::IsKeyPressed(devConsoleKey);

			Win32Window::PollEvents();

			std::optional<std::string> command = asyncRenderer.get();
			if (command.has_value() && lastCommand != command.value())
			{
				HandleConsoleCommand(command.value());
				lastCommand = command.value();
			}
			asyncScripts.get();

			frameDelta = std::chrono::duration<float, std::chrono::milliseconds::period>(std::chrono::high_resolution_clock::now() - timeSinceLastFrame).count();
			timeSinceLastFrame = std::chrono::high_resolution_clock::now();

			timeSinceLastGraphUpdate += frameDelta;
			if (timeSinceLastGraphUpdate > 500)
			{
				float timeSpentInMainThread = frameDelta - asyncScriptsCompletionTime - asyncRendererCompletionTime;
				
				float timeValues[] = { timeSpentInMainThread * 1000, asyncScriptsCompletionTime * 1000, asyncRendererCompletionTime * 1000 };
				pieChartValuesPtr = timeValues;
				timeSinceLastGraphUpdate = 0;
			}
		}
		return EXIT_CODE_SUCESS;
	}
	catch (const std::exception& e) //catch any normal exception and return
	{
		std::string fullError = e.what();
		MessageBoxA(nullptr, fullError.c_str(), "Engine error", MB_OK | MB_ICONERROR);
		std::cerr << e.what() << std::endl;
		return EXIT_CODE_EXCEPTION;
	}
	catch (...) //catch any unknown exceptions and return, doesnt catch any read or write errors etc.
	{
		MessageBoxA(nullptr, "Caught an unknown error, this build is most likely corrupt and can't be used.", "Unknown engine error", MB_OK | MB_ICONERROR);
		return EXIT_CODE_UNKNOWN_EXCEPTION;
	}
}