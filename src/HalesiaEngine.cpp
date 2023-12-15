#include <iostream>
#include <future>
#include "HalesiaEngine.h"
#include "system/Input.h"
#include "system/SystemMetrics.h"
#include "renderer/Intro.h"
#include "tools/CameraInjector.h"
#include "CreationObjects.h"
#include "Console.h"
#include "renderer/RayTracing.h"
#include "physics/Physics.h"
#include "gui.h"

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
MeshCreationObject GetVulkanCreationObjects()
{
	return localRenderer->GetVulkanCreationObject();
}

void HalesiaInstance::GenerateHalesiaInstance(HalesiaInstance& instance, HalesiaInstanceCreateInfo& createInfo)
{
	std::cout << "Generating Halesia instance:\n" << "  createInfo.startingScene = " << createInfo.startingScene << "\n  createInfo.devConsoleKey = " << ToHexadecimalString((int)createInfo.devConsoleKey) << "\n  createInfo.playIntro = " << createInfo.playIntro << "\n\n";
	try
	{
		Console::Init();
		Console::WriteLine("Write \"help\" for all commands");
		//instance.physics = new Physics();
		instance.useEditor = createInfo.useEditor;
		instance.devConsoleKey = createInfo.devConsoleKey;
		instance.playIntro = createInfo.playIntro;
		Physics::Init();

		if (createInfo.argsCount > 1)
			DetermineArgs(createInfo.argsCount, createInfo.args, createInfo.windowCreateInfo);
		
		instance.window = new Win32Window(createInfo.windowCreateInfo);
		instance.renderer = new Renderer(instance.window);
		localRenderer = instance.renderer;
		Scene::GetVulkanCreationObjects = &GetVulkanCreationObjects;

		if (createInfo.startingScene == nullptr)
		{
			Console::WriteLine("The given HalesiaInstanceCreateInfo doesn't contain a valid starting scene", MESSAGE_SEVERITY_WARNING);
			instance.scene = new Scene();
		}
		else
			instance.scene = createInfo.startingScene;
		if (createInfo.sceneFile != "")
			instance.scene->LoadScene(createInfo.sceneFile);

		SystemInformation systemInfo = GetCpuInfo();
		VkPhysicalDeviceProperties properties = instance.renderer->GetVulkanCreationObject().physicalDevice.Properties();
		uint64_t vram = instance.renderer->GetVulkanCreationObject().physicalDevice.VRAM();
		std::cout << "\nDetected hardware:\n" << "  CPU: " << systemInfo.CPUName << "\n  logical processor count: " << systemInfo.processorCount << "\n  physical RAM: " << systemInfo.installedRAM / 1024 << " MB\n\n" << "  GPU: " << properties.deviceName << "\n  type: " << string_VkPhysicalDeviceType(properties.deviceType) << "\n  vulkan driver version: " << properties.driverVersion << "\n  API version: " << properties.apiVersion << "\n  heap 0 total memory (VRAM): " << vram / (1024.0f * 1024.0f) << " MB\n" << std::endl;;
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
	//delete physics;
	delete window;
}

void HalesiaInstance::LoadScene(Scene* newScene)
{
	scene->Destroy();
	scene = newScene;
}

void ManageCameraInjector(Scene* scene, bool pauseGame)
{
	static CameraInjector cameraInjector;
	static Camera* orbitCamera = new OrbitCamera();

	if (pauseGame && !cameraInjector.IsInjected())
	{
		cameraInjector = CameraInjector{ scene };
		cameraInjector.Inject(orbitCamera);
	}
	else if (!pauseGame && cameraInjector.IsInjected())
		cameraInjector.Eject();
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

void HalesiaInstance::UpdateScene(const UpdateSceneData& sceneData)
{
	SetThreadDescription(GetCurrentThread(), L"SceneUpdatingThread");

	std::chrono::steady_clock::time_point begin = std::chrono::high_resolution_clock::now();

	ManageCameraInjector(scene, sceneData.pauseGame);

	scene->UpdateCamera(window, sceneData.delta);
	if (!sceneData.pauseGame || sceneData.playOneFrame)
	{
		scene->UpdateScripts(sceneData.delta);
		scene->Update(sceneData.delta);
		sceneData.playOneFrame = false;
	}
	asyncScriptsCompletionTime = std::chrono::duration<float, std::chrono::milliseconds::period>(std::chrono::high_resolution_clock::now() - begin).count();
}

std::optional<std::string> HalesiaInstance::UpdateRenderer(const UpdateRendererData& rendererData)
{
	SetThreadDescription(GetCurrentThread(), L"VulkanRenderingThread");

	std::chrono::steady_clock::time_point begin = std::chrono::high_resolution_clock::now();
	std::optional<std::string> command = GUI::ShowDevConsole();
	if (showFPS)
		GUI::ShowFPS((int)(1 / rendererData.delta * 1000));

	ramUsed.Add(GetPhysicalMemoryUsedByApp() / (1024ULL * 1024));
	if (showRAM)
		GUI::ShowGraph(ramUsed.buffer, "RAM in MB");
	if (showCPU)
		GUI::ShowGraph(CPUUsage.buffer, "CPU %");
	if (showGPU)
		GUI::ShowGraph(GPUUsage.buffer, "GPU %");
	if (showAsyncTimes)
		GUI::ShowPieGraph(asyncTimes, "Async Times (µs)");
	if (showObjectData)
		GUI::ShowObjectTable(scene->allObjects);

	if (useEditor)
	{
		GUI::ShowSceneGraph(scene->allObjects, window);
		GUI::ShowMainMenuBar(showObjectData, showRAM, showCPU, showGPU);
	}

	renderer->DrawFrame(scene->allObjects, scene->camera, rendererData.delta);

	asyncRendererCompletionTime = std::chrono::duration<float, std::chrono::milliseconds::period>(std::chrono::high_resolution_clock::now() - begin).count();
	return command;
}

void HalesiaInstance::CheckInput()
{
	if (Input::IsKeyPressed(VirtualKey::C))
		pauseGame = false;
	showFPS = !showFPS ? Input::IsKeyPressed(VirtualKey::LeftControl) && Input::IsKeyPressed(VirtualKey::F1) : showFPS;
	pauseGame = !pauseGame ? Input::IsKeyPressed(VirtualKey::LeftControl) && Input::IsKeyPressed(VirtualKey::F2) : true;
	showRAM = !showRAM ? Input::IsKeyPressed(VirtualKey::LeftControl) && Input::IsKeyPressed(VirtualKey::F3) : true;
	showCPU = !showCPU ? Input::IsKeyPressed(VirtualKey::LeftControl) && Input::IsKeyPressed(VirtualKey::F4) : true;
	showGPU = !showGPU ? Input::IsKeyPressed(VirtualKey::LeftControl) && Input::IsKeyPressed(VirtualKey::F5) : true;
	showAsyncTimes = !showAsyncTimes ? Input::IsKeyPressed(VirtualKey::LeftControl) && Input::IsKeyPressed(VirtualKey::F6) : true;

	playOneFrame = Input::IsKeyPressed(VirtualKey::RightArrow);

	if (Input::IsKeyPressed(VirtualKey::Q))
		window->LockCursor();
	if (Input::IsKeyPressed(VirtualKey::E))
		window->UnlockCursor();

	if (!Input::IsKeyPressed(devConsoleKey) && devKeyIsPressedLastFrame)
		Console::isOpen = !Console::isOpen;
}

void HalesiaInstance::UpdateAsyncCompletionTimes(float frameDelta)
{
	float timeSpentInMainThread = frameDelta - asyncScriptsCompletionTime - asyncRendererCompletionTime;

	asyncTimes.push_back(timeSpentInMainThread * 1000);
	asyncTimes.push_back(asyncScriptsCompletionTime * 1000);
	asyncTimes.push_back(asyncRendererCompletionTime * 1000);
}

void HalesiaInstance::UpdateCGPUUsage()
{
	float cpu = GetCPUPercentageUsedByApp();
	if (cpu != -1 && cpu != 0) //dont know if 0 is junk data
		CPUUsage.Add(cpu);

	float gpu = (float)GetGPUUsage() * 100.0f; //doesnt look incredibly accurate but it works good enough
	GPUUsage.Add(gpu);
}

HalesiaExitCode HalesiaInstance::Run()
{
	std::string lastCommand;
	float timeSinceLastDataUpdate = 0;
	float dummy[] = {0, 0, 0};

	Console::AddConsoleVariable("pauseGame", &pauseGame);
	Console::AddConsoleVariable("showFPS", &showFPS);
	Console::AddConsoleVariable("playOneFrame", &playOneFrame);
	Console::AddConsoleVariable("showRAM", &showRAM);
	Console::AddConsoleVariable("showCPU", &showCPU);
	Console::AddConsoleVariable("showGPU", &showGPU);
	Console::AddConsoleVariable("showAsyncTimes", &showAsyncTimes);
	Console::AddConsoleVariable("showMetaData", &showObjectData);

	if (renderer == nullptr || window == nullptr/* || physics == nullptr*/)
		return HALESIA_EXIT_CODE_UNKNOWN_EXCEPTION;

	Console::AddConsoleVariable("raySamples", &RayTracing::raySampleCount);
	Console::AddConsoleVariable("rayDepth", &RayTracing::rayDepth);
	Console::AddConsoleVariable("showNormals", &RayTracing::showNormals);
	Console::AddConsoleVariable("renderProgressive", &RayTracing::renderProgressive);
	Console::AddConsoleVariable("showUnique", &RayTracing::showUniquePrimitives);
	Console::AddConsoleVariable("showAlbedo", &RayTracing::showAlbedo);
	Console::AddConsoleVariable("rasterize", &renderer->shouldRasterize);
	Console::AddConsoleVariable("useEditorUI", &useEditor);
	try
	{
		float frameDelta = 0;
		std::chrono::steady_clock::time_point timeSinceLastFrame = std::chrono::high_resolution_clock::now();

		window->maximized = true;
		if (playIntro)
		{
			Intro intro{};
			intro.Create(renderer->GetVulkanCreationObject(), renderer->swapchain, "textures/floor.png");

			renderer->RenderIntro(&intro);
			intro.Destroy();
		}
		
		scene->Start();
		//Sleep(1000);
		while (!window->ShouldClose())
		{
			CheckInput();
			UpdateSceneData sceneData{ frameDelta, pauseGame, playOneFrame };
			asyncScripts = std::async(&HalesiaInstance::UpdateScene, this, std::cref(sceneData));

			UpdateRendererData rendererData{ frameDelta };
			asyncRenderer = std::async(&HalesiaInstance::UpdateRenderer, this, std::cref(rendererData));

			if (Input::IsKeyPressed(VirtualKey::P))
			Physics::physics->Simulate(frameDelta);

			if (window->ContainsDroppedFile())
				scene->AddStaticObject(GenericLoader::LoadObjectFile(window->GetDroppedFile()));

			devKeyIsPressedLastFrame = Input::IsKeyPressed(devConsoleKey);

			std::optional<std::string> command = asyncRenderer.get();
			if (command.has_value() && lastCommand != command.value())
			{
				HandleConsoleCommand(command.value());
				lastCommand = command.value();
			}

			Win32Window::PollMessages(); // moved for swapchain / surface interference

			asyncScripts.get();

			if (frameDelta > 0)
			Physics::physics->FetchAndUpdateObjects();

			frameDelta = std::chrono::duration<float, std::chrono::milliseconds::period>(std::chrono::high_resolution_clock::now() - timeSinceLastFrame).count();
			timeSinceLastFrame = std::chrono::high_resolution_clock::now();
			
			timeSinceLastDataUpdate += frameDelta;
			if (timeSinceLastDataUpdate > 500)
			{
				UpdateCGPUUsage();
				UpdateAsyncCompletionTimes(frameDelta);
				timeSinceLastDataUpdate = 0;
			}
		}
		return HALESIA_EXIT_CODE_SUCESS;
	}
	catch (const std::exception& e) //catch any normal exception and return
	{
		std::string fullError = e.what();
		ShowWindow(window->window, 0);
		
		MessageBoxA(nullptr, fullError.c_str(), ((std::string)"Engine error (" + (std::string)typeid(e).name() + ')').c_str(), MB_OK | MB_ICONERROR);
		std::cerr << e.what() << std::endl;
		return HALESIA_EXIT_CODE_EXCEPTION;
	}
	catch (...) //catch any unknown exceptions and return, doesnt catch any read or write errors etc.
	{
		MessageBoxA(nullptr, "Caught an unknown error, this build is most likely corrupt and can't be used.", "Unknown engine error", MB_OK | MB_ICONERROR);
		return HALESIA_EXIT_CODE_UNKNOWN_EXCEPTION;
	}
}