#include <iostream>
#include <future>
#include "HalesiaEngine.h"
#include "system/Input.h"
#include "Console.h"
#include "system/SystemMetrics.h"
#include "system/GPUUsage.h"
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
	return localRenderer->GetMeshCreationObjects();
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
			Console::WriteLine("!! The given HalesiaInstanceCreateInfo doesn't contain a valid starting scene");
			instance.scene = new Scene();
		}
		else
			instance.scene = createInfo.startingScene;
		//instance.scene->SubmitStaticModel("./blahaj.obj", instance.renderer->GetMeshCreationObjects());
		//instance.scene->AddCustomObject<Test>("./blahaj.obj", instance.renderer->GetMeshCreationObjects());
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

void UpdateScene(Scene* scene, Win32Window* window, float delta, bool pauseGame, bool* playOneFrame)
{
	if (!pauseGame || *playOneFrame)
	{
		scene->Update(window, delta);
		*playOneFrame = false;
	}
		
}

void WriteAllCommandsToConsole()
{
	Console::WriteLine("-----------------------------------------------");
	Console::WriteLine("\"pauseGame 1 or 0\" pauses or unpauses the game");      // or ctrl + F1
	Console::WriteLine("\"showFPS 1 or 0\" shows or hides the FPS counter");     // or ctrl + F2
	Console::WriteLine("\"showRAM 1 or 0\" shows or hides the RAM usage graph"); // or ctrl + f3
	Console::WriteLine("\"showCPU 1 or 0\" shows or hides the CPU usage graph"); // or ctrl + f4
	Console::WriteLine("\"showGPU 1 or 0\" shows or hides the CPU usage graph"); // or ctrl + F5
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

std::optional<std::string> UpdateRenderer(Renderer* renderer, Camera* camera, std::vector<Object*> objects, float delta, bool renderDevConsole, bool showFPS, bool showRAM, bool showCPU, bool showGPU)
{
	std::optional<std::string> command = renderer->RenderDevConsole(renderDevConsole);
	if (showFPS)
		renderer->RenderFPS(1 / delta * 1000);

	ramUsed.Add(GetPhysicalMemoryUsedByApp() / (1024 * 1024));
	if (showRAM)
		renderer->RenderGraph(ramUsed.buffer, "RAM in MB");
	if (showCPU)
		renderer->RenderGraph(CPUUsage.buffer, "CPU %");
	if (showGPU)
		renderer->RenderGraph(GPUUsage.buffer, "GPU %");
	renderer->DrawFrame(objects, camera, delta);
	return command;
}

HalesiaExitCode HalesiaInstance::Run()
{
	bool devKeyIsPressedLastFrame = false;
	std::string lastCommand;
	float timeSinceLastCPUUpdate = 0;

	Console::commandVariables["pauseGame"] = &pauseGame;
	Console::commandVariables["showFPS"] = &showFPS;
	Console::commandVariables["playOneFrame"] = &playOneFrame;
	Console::commandVariables["showRAM"] = &showRAM;
	Console::commandVariables["showCPU"] = &showCPU;
	Console::commandVariables["showGPU"] = &showGPU;

	if (renderer == nullptr || window == nullptr || physics == nullptr)
		return EXIT_CODE_UNKNOWN_EXCEPTION;

	try
	{
		float frameDelta = 0;
		std::chrono::steady_clock::time_point timeSinceLastFrame = std::chrono::high_resolution_clock::now();

		scene->Start();

		while (!window->ShouldClose())
		{
			showFPS = !showFPS ? Input::IsKeyPressed(VirtualKey::LeftControl) && Input::IsKeyPressed(VirtualKey::F1) : true;
			pauseGame = !pauseGame ? Input::IsKeyPressed(VirtualKey::LeftControl) && Input::IsKeyPressed(VirtualKey::F2) : true;
			showRAM = !showRAM ? Input::IsKeyPressed(VirtualKey::LeftControl) && Input::IsKeyPressed(VirtualKey::F3) : true;
			showCPU = !showCPU ? Input::IsKeyPressed(VirtualKey::LeftControl) && Input::IsKeyPressed(VirtualKey::F4) : true;
			showGPU = !showGPU ? Input::IsKeyPressed(VirtualKey::LeftControl) && Input::IsKeyPressed(VirtualKey::F5) : true;

			std::future<void> asyncScripts = std::async(UpdateScene, scene, window, frameDelta, pauseGame, &playOneFrame);
			std::future<std::optional<std::string>> asyncRenderer = std::async(UpdateRenderer, renderer, scene->camera, scene->allObjects, frameDelta, renderDevConsole, showFPS, showRAM, showCPU, showGPU);

			if (window->ContainsDroppedFile())
				scene->SubmitStaticModel(window->GetDroppedFile(), renderer->GetMeshCreationObjects());

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

			//asyncScripts.get();
			std::optional<std::string> command = asyncRenderer.get();
			if (command.has_value() && lastCommand != command.value())
			{
				HandleConsoleCommand(command.value());
				lastCommand = command.value();
			}

			frameDelta = std::chrono::duration<float, std::chrono::milliseconds::period>(std::chrono::high_resolution_clock::now() - timeSinceLastFrame).count();
			timeSinceLastFrame = std::chrono::high_resolution_clock::now();
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