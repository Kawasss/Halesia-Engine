#include <iostream>
#include <future>
#include "HalesiaEngine.h"

#include "system/Input.h"
#include "system/SystemMetrics.h"

#include "renderer/Renderer.h"
#include "renderer/Intro.h"
#include "renderer/RayTracing.h"
#include "renderer/gui.h"

#include "tools/CameraInjector.h"
#include "physics/Physics.h"

#include "core/Console.h"

#include "vvm/VVM.hpp"

#include "Audio.h"

inline void ProcessError(const std::exception& e)
{
	std::string fullError = e.what();
	MessageBoxA(nullptr, fullError.c_str(), ((std::string)"Engine error (" + (std::string)typeid(e).name() + ')').c_str(), MB_OK | MB_ICONERROR);
	std::cerr << e.what() << std::endl;
}

inline float CalculateFrameTime(int fps)
{
	if (fps <= 0) return 0;
	return 1000.0f / fps;
}

void HalesiaInstance::GenerateHalesiaInstance(HalesiaInstance& instance, HalesiaInstanceCreateInfo& createInfo)
{
	std::cout << "Generating Halesia instance:" 
		<< "\n  createInfo.startingScene = " << ToHexadecimalString((int)createInfo.startingScene) 
		<< "\n  createInfo.devConsoleKey = " << ToHexadecimalString((int)createInfo.devConsoleKey) 
		<< "\n  createInfo.playIntro     = " << createInfo.playIntro << "\n\n";
	try
	{
		instance.OnLoad(createInfo);

		const Vulkan::Context& context = Vulkan::GetContext();
		SystemInformation systemInfo = GetCpuInfo();
		VkPhysicalDeviceProperties properties = context.physicalDevice.Properties();
		uint64_t vram = context.physicalDevice.VRAM();

		std::cout << "\nDetected hardware:" 
			<< "\n  CPU: " << systemInfo.CPUName 
			<< "\n  logical processor count: " << systemInfo.processorCount 
			<< "\n  physical RAM: " << systemInfo.installedRAM / 1024 << " MB\n" 
			<< "\n  GPU: " << properties.deviceName 
			<< "\n  type: " << string_VkPhysicalDeviceType(properties.deviceType) 
			<< "\n  vulkan driver version: " << properties.driverVersion 
			<< "\n  API version: " << properties.apiVersion 
			<< "\n  heap 0 total memory (VRAM): " << vram / (1024.0f * 1024.0f) << " MB\n\n";
	}
	catch (const std::exception& e) //catch any normal exception and return
	{
		ProcessError(e);
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
	delete window;
}

void HalesiaInstance::LoadScene(Scene* newScene)
{
	scene->Destroy();
	scene = newScene;
}

inline void ManageCameraInjector(Scene* scene, bool pauseGame)
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

Win32Window* HalesiaInstance::GetWindow()
{
	return window;
}

void HalesiaInstance::UpdateScene(float delta)
{
	SetThreadDescription(GetCurrentThread(), L"SceneUpdatingThread");

	std::chrono::steady_clock::time_point begin = std::chrono::high_resolution_clock::now();

	ManageCameraInjector(scene, pauseGame);

	scene->UpdateCamera(window, delta);
	if (!pauseGame || playOneFrame)
	{
		scene->UpdateScripts(delta);
		scene->Update(delta);
		playOneFrame = false;
	}
	asyncScriptsCompletionTime = std::chrono::duration<float, std::chrono::milliseconds::period>(std::chrono::high_resolution_clock::now() - begin).count();
}

void HalesiaInstance::UpdateRenderer(float delta)
{
	SetThreadDescription(GetCurrentThread(), L"VulkanRenderingThread");
	
	std::chrono::steady_clock::time_point begin = std::chrono::high_resolution_clock::now();
	GUI::ShowDevConsole();
	if (showFPS)
		GUI::ShowFPS((int)(1 / delta * 1000));

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
		renderer->SetViewportOffsets({ 0.125f, 0 });
		renderer->SetViewportModifiers({ 0.75f, 1 }); // doesnt have to be set every frame
		GUI::ShowSceneGraph(scene->allObjects, window);
		GUI::ShowMainMenuBar(showWindowData, showObjectData, showRAM, showCPU, showGPU);
	}
	else
	{
		renderer->SetViewportOffsets({ 0, 0 });
		renderer->SetViewportModifiers({ 1, 1 }); // doesnt have to be set every frame
	}

	renderer->DrawFrame(scene->allObjects, scene->camera, delta);

	asyncRendererCompletionTime = std::chrono::duration<float, std::chrono::milliseconds::period>(std::chrono::high_resolution_clock::now() - begin).count();
}

void HalesiaInstance::CheckInput()
{
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

	if (renderer == nullptr || window == nullptr/* || physics == nullptr*/)
		return HALESIA_EXIT_CODE_UNKNOWN_EXCEPTION;

	RegisterConsoleVars();

	try
	{
		float frameDelta = 0;
		std::chrono::steady_clock::time_point timeSinceLastFrame = std::chrono::high_resolution_clock::now();

		window->maximized = true;
		if (playIntro)
		{
			Intro intro{};
			intro.Create(renderer->swapchain, "textures/floor.png");

			renderer->RenderIntro(&intro);
			intro.Destroy();
		}
		
		scene->Start();
		while (!window->ShouldClose())
		{
			CheckInput();
			devKeyIsPressedLastFrame = Input::IsKeyPressed(devConsoleKey);

			Physics::Simulate(frameDelta);

			asyncScripts = std::async(&HalesiaInstance::UpdateScene, this, frameDelta);
			asyncRenderer = std::async(&HalesiaInstance::UpdateRenderer, this, frameDelta);

			asyncRenderer.get();

			if (showWindowData)
				GUI::ShowWindowData(window); // only works on main thread, because it calls windows functions for changing the window

			Win32Window::PollMessages();

			asyncScripts.get();

			Physics::FetchAndUpdateObjects();

			while (std::chrono::duration<float, std::chrono::milliseconds::period>(std::chrono::high_resolution_clock::now() - timeSinceLastFrame).count() < CalculateFrameTime(maxFPS)); // wait untill the fps limit is reached

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
		OnExit();
		return HALESIA_EXIT_CODE_SUCESS;
	}
	catch (const std::exception& e) //catch any normal exception and return
	{
		std::string fullError = e.what();
		ProcessError(e);
		return HALESIA_EXIT_CODE_EXCEPTION;
	}
	catch (...) //catch any unknown exceptions and return, doesnt catch any read or write errors etc.
	{
		MessageBoxA(nullptr, "Caught an unknown error, this build is most likely corrupt and can't be used.", "Unknown engine error", MB_OK | MB_ICONERROR);
		return HALESIA_EXIT_CODE_UNKNOWN_EXCEPTION;
	}
}

void HalesiaInstance::OnLoad(HalesiaInstanceCreateInfo& createInfo)
{
	Audio::Init();
	Console::Init();
	Physics::Init();
	
	useEditor = createInfo.useEditor;
	devConsoleKey = createInfo.devConsoleKey;
	playIntro = createInfo.playIntro;

	window = new Win32Window(createInfo.windowCreateInfo);
	renderer = new Renderer(window);

	if (createInfo.startingScene == nullptr)
	{
		Console::WriteLine("The given HalesiaInstanceCreateInfo doesn't contain a valid starting scene", MESSAGE_SEVERITY_WARNING);
		scene = new Scene();
	}
	else
		scene = createInfo.startingScene;
	if (createInfo.sceneFile != "")
		scene->LoadScene(createInfo.sceneFile);

	LoadVars();
}

void HalesiaInstance::LoadVars()
{
	std::vector<VVM::Group> groups;
	if (VVM::ReadFromFile("cfg/vars.vvm", groups) != VVM_SUCCESS)
		return;

	useEditor = VVM::FindVariable("engineCore.useEditorUI", groups).As<bool>();
	showFPS =   VVM::FindVariable("engineCore.showFPS", groups).As<bool>();
	maxFPS =    VVM::FindVariable("engineCore.maxFPS", groups).As<int>();
	devConsoleKey = (VirtualKey)VVM::FindVariable("engineCore.consoleKey", groups).As<int>();

	renderer->internalScale =              VVM::FindVariable("renderer.internalRes", groups).As<float>();
	Renderer::shouldRenderCollisionBoxes = VVM::FindVariable("renderer.rendercollision", groups).As<bool>();
	Renderer::denoiseOutput =              VVM::FindVariable("renderer.denoiseOutput", groups).As<bool>();
	RayTracing::raySampleCount =           VVM::FindVariable("renderer.ray-tracing.raySamples", groups).As<int>();
	RayTracing::rayDepth =                 VVM::FindVariable("renderer.ray-tracing.rayDepth", groups).As<int>();
	RayTracing::showNormals =              VVM::FindVariable("renderer.ray-tracing.showNormals", groups).As<bool>();
	RayTracing::showUniquePrimitives =     VVM::FindVariable("renderer.ray-tracing.showUnique", groups).As<bool>();
	RayTracing::showAlbedo =               VVM::FindVariable("renderer.ray-tracing.showAlbedo", groups).As<bool>();
	RayTracing::renderProgressive =        VVM::FindVariable("renderer.ray-tracing.renderProgressive", groups).As<bool>();

	std::cout << "Finished loading from cfg/vars.vvm\n";
}

void HalesiaInstance::OnExit()
{
	Audio::Destroy();

	VVM::PushGroup("engineCore");
	VVM::AddVariable("useEditorUI", useEditor);
	VVM::AddVariable("showFPS", showFPS);
	VVM::AddVariable("maxFPS", maxFPS);
	VVM::AddVariable("consoleKey", (int)devConsoleKey);
	VVM::PopGroup();

	VVM::PushGroup("renderer");
	VVM::AddVariable("interalRes", Renderer::internalScale);
	VVM::AddVariable("renderCollision", Renderer::shouldRenderCollisionBoxes);
	VVM::AddVariable("denoiseOutput", Renderer::denoiseOutput);

	VVM::PushGroup("ray-tracing");
	VVM::AddVariable("raySamples", RayTracing::raySampleCount);
	VVM::AddVariable("rayDepth", RayTracing::rayDepth);
	VVM::AddVariable("showNormals", RayTracing::showNormals);
	VVM::AddVariable("showUnique", RayTracing::showUniquePrimitives);
	VVM::AddVariable("showAlbedo", RayTracing::showAlbedo);
	VVM::AddVariable("renderProgressive", RayTracing::renderProgressive);
	VVM::PopGroup();
	VVM::PopGroup();

	VVM::WriteToFile("cfg/vars.vvm");
	std::cout << "Finished writing to cfg/vars.vvm\n";
}

void HalesiaInstance::RegisterConsoleVars()
{
	Console::AddConsoleVariables<bool>(
		{ "pauseGame", "showFPS", "playOneFrame", "showRAM", "showCPU", "showGPU", "showAsyncTimes", "showMetaData", "showNormals", "showAlbedo", "showUnique", "renderProgressive", "rasterize", "useEditorUI", "denoiseOutput" },
		{ &pauseGame, &showFPS, &playOneFrame, &showRAM, &showCPU, &showGPU, &showAsyncTimes, &showObjectData, &RayTracing::showNormals, &RayTracing::showAlbedo, &RayTracing::showUniquePrimitives, &RayTracing::renderProgressive, &renderer->shouldRasterize, &useEditor, &Renderer::denoiseOutput }
	);
	Console::AddConsoleVariable("raySamples", &RayTracing::raySampleCount);
	Console::AddConsoleVariable("rayDepth", &RayTracing::rayDepth);
	Console::AddConsoleVariable("internalResScale", &Renderer::internalScale);
}