#include <iostream>
#include <future>
#include "HalesiaEngine.h"

#include "system/Input.h"
#include "system/SystemMetrics.h"

#include "renderer/Renderer.h"
#include "renderer/Intro.h"
#include "renderer/RayTracing.h"
#include "renderer/gui.h"
#include "renderer/AnimationManager.h"

#include "tools/CameraInjector.h"
#include "physics/Physics.h"

#include "core/Console.h"
#include "core/Profiler.h"
#include "core/Behavior.h"

#include "vvm/VVM.hpp"

#include "Audio.h"

HalesiaEngineCreateInfo HalesiaEngine::createInfo{};

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

HalesiaEngine* HalesiaEngine::GetInstance()
{
	static HalesiaEngine instance;
	static  bool init = false;
	if (init)
		return &instance;

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

		init = true;
	}
	catch (const std::exception& e) //catch any normal exception and return
	{
		ProcessError(e);
	}
	catch (...) //catch any unknown exceptions and return, doesnt catch any read or write errors etc.
	{
		MessageBoxA(nullptr, "Caught an unknown error, this build is most likely corrupt and can't be used.", "Unknown engine error", MB_OK | MB_ICONERROR);
	}
	return &instance;
}

void HalesiaEngine::SetCreateInfo(const HalesiaEngineCreateInfo& createInfo)
{
	HalesiaEngine::createInfo = createInfo;
}

void HalesiaEngine::Destroy()
{
	core.renderer->Destroy();
	core.scene->Destroy();
	delete core.window;
}

void HalesiaEngine::LoadScene(Scene* newScene)
{
	core.scene->Destroy();
	core.scene = newScene;
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

EngineCore& HalesiaEngine::GetEngineCore()
{
	return core;
}

void HalesiaEngine::UpdateScene(float delta)
{
	SetThreadDescription(GetCurrentThread(), L"SceneUpdatingThread");

	std::chrono::steady_clock::time_point begin = std::chrono::high_resolution_clock::now();

	ManageCameraInjector(core.scene, pauseGame);

	core.scene->UpdateCamera(core.window, delta);
	if (!pauseGame || playOneFrame)
	{
		core.scene->UpdateScripts(delta);
		core.scene->Update(delta);
		playOneFrame = false;
	}
	asyncScriptsCompletionTime = std::chrono::duration<float, std::chrono::milliseconds::period>(std::chrono::high_resolution_clock::now() - begin).count();
}

void HalesiaEngine::UpdateRenderer(float delta)
{
	SetThreadDescription(GetCurrentThread(), L"VulkanRenderingThread");
	
	std::chrono::steady_clock::time_point begin = std::chrono::high_resolution_clock::now();
	GUI::ShowDevConsole();

	if (!pauseGame)
		core.animationManager->ComputeAnimations(delta);

	if (showFPS)
		GUI::ShowFPS((int)(1 / delta * 1000));

	if (showRAM)
		GUI::ShowGraph(core.profiler->GetRAM(), "RAM in MB");
	if (showCPU)
		GUI::ShowGraph(core.profiler->GetCPU(), "CPU %");
	if (showGPU)
		GUI::ShowGraph(core.profiler->GetGPU(), "GPU %");
	if (showAsyncTimes)
		GUI::ShowPieGraph(asyncTimes, "Async Times (µs)");
	if (showObjectData)
		GUI::ShowObjectTable(core.scene->allObjects);
	
	if (useEditor)
	{
		core.renderer->SetViewportOffsets({ 0.125f, 0 });
		core.renderer->SetViewportModifiers({ 0.75f, 1 }); // doesnt have to be set every frame
		GUI::ShowSceneGraph(core.scene->allObjects, core.window);
		GUI::ShowMainMenuBar(showWindowData, showObjectData, showRAM, showCPU, showGPU);
	}
	else
	{
		core.renderer->SetViewportOffsets({ 0, 0 });
		core.renderer->SetViewportModifiers({ 1, 1 }); // doesnt have to be set every frame
	}

	core.renderer->DrawFrame(core.scene->allObjects, core.scene->camera, delta);

	asyncRendererCompletionTime = std::chrono::duration<float, std::chrono::milliseconds::period>(std::chrono::high_resolution_clock::now() - begin).count();
}

void HalesiaEngine::CheckInput()
{
	playOneFrame = Input::IsKeyPressed(VirtualKey::RightArrow);

	if (Input::IsKeyPressed(VirtualKey::Q))
		core.window->LockCursor();
	if (Input::IsKeyPressed(VirtualKey::E))
		core.window->UnlockCursor();

	if (!Input::IsKeyPressed(devConsoleKey) && devKeyIsPressedLastFrame)
		Console::isOpen = !Console::isOpen;
}

void HalesiaEngine::UpdateAsyncCompletionTimes(float frameDelta)
{
	float timeSpentInMainThread = frameDelta - asyncScriptsCompletionTime - asyncRendererCompletionTime;

	asyncTimes.push_back(timeSpentInMainThread * 1000);
	asyncTimes.push_back(asyncScriptsCompletionTime * 1000);
	asyncTimes.push_back(asyncRendererCompletionTime * 1000);
}

HalesiaExitCode HalesiaEngine::Run()
{
	std::string lastCommand;
	float timeSinceLastDataUpdate = 0;

	if (core.renderer == nullptr || core.window == nullptr/* || physics == nullptr*/)
		return HALESIA_EXIT_CODE_UNKNOWN_EXCEPTION;

	RegisterConsoleVars();

	try
	{
		float frameDelta = 0;
		std::chrono::steady_clock::time_point timeSinceLastFrame = std::chrono::high_resolution_clock::now();

		core.window->maximized = true;
		if (playIntro)
		{
			Intro intro{};
			intro.Create(core.renderer->swapchain, "textures/floor.png");

			core.renderer->RenderIntro(&intro);
			intro.Destroy();
		}
		
		core.scene->Start();
		while (!core.window->ShouldClose())
		{
			CheckInput();
			devKeyIsPressedLastFrame = Input::IsKeyPressed(devConsoleKey);

			Physics::Simulate(frameDelta);

			asyncScripts = std::async(&HalesiaEngine::UpdateScene, this, frameDelta);
			asyncRenderer = std::async(&HalesiaEngine::UpdateRenderer, this, frameDelta);

			asyncRenderer.get();

			if (showWindowData)
				GUI::ShowWindowData(core.window); // only works on main thread, because it calls windows functions for changing the window

			Win32Window::PollMessages();

			asyncScripts.get();

			Physics::FetchAndUpdateObjects();

			while (std::chrono::duration<float, std::chrono::milliseconds::period>(std::chrono::high_resolution_clock::now() - timeSinceLastFrame).count() < CalculateFrameTime(core.maxFPS)); // wait untill the fps limit is reached

			frameDelta = std::chrono::duration<float, std::chrono::milliseconds::period>(std::chrono::high_resolution_clock::now() - timeSinceLastFrame).count();
			timeSinceLastFrame = std::chrono::high_resolution_clock::now();

			core.profiler->Update(frameDelta);

			timeSinceLastDataUpdate += frameDelta;
			if (timeSinceLastDataUpdate > 500)
			{
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

void HalesiaEngine::OnLoad(HalesiaEngineCreateInfo& createInfo)
{
	Audio::Init();
	Console::Init();
	Physics::Init();
	
	Behavior::ProcessArguments(createInfo.argsCount, createInfo.args);

	useEditor = createInfo.useEditor;
	devConsoleKey = createInfo.devConsoleKey;
	playIntro = createInfo.playIntro;

	core.window = new Win32Window(createInfo.windowCreateInfo);
	core.renderer = new Renderer(core.window);
	core.profiler = Profiler::Get();
	core.profiler->SetFlags(Profiler::ALL_OPTIONS);
	core.animationManager = AnimationManager::Get();

	if (createInfo.startingScene == nullptr)
	{
		Console::WriteLine("The given HalesiaInstanceCreateInfo doesn't contain a valid starting scene", MESSAGE_SEVERITY_WARNING);
		core.scene = new Scene();
	}
	else
		core.scene = createInfo.startingScene;
	if (createInfo.sceneFile != "")
		core.scene->LoadScene(createInfo.sceneFile);

	LoadVars();
}

void HalesiaEngine::LoadVars()
{
	std::vector<VVM::Group> groups;
	if (VVM::ReadFromFile("cfg/vars.vvm", groups) != VVM_SUCCESS)
		return;

	useEditor =   VVM::FindVariable("engineCore.useEditorUI", groups).As<bool>();
	showFPS =     VVM::FindVariable("engineCore.showFPS", groups).As<bool>();
	core.maxFPS = VVM::FindVariable("engineCore.maxFPS", groups).As<int>();
	devConsoleKey = (VirtualKey)VVM::FindVariable("engineCore.consoleKey", groups).As<int>();

	core.renderer->internalScale =         VVM::FindVariable("renderer.internalRes", groups).As<float>();
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

void HalesiaEngine::OnExit()
{
	Audio::Destroy();

	VVM::PushGroup("engineCore");
	VVM::AddVariable("useEditorUI", useEditor);
	VVM::AddVariable("showFPS", showFPS);
	VVM::AddVariable("maxFPS", core.maxFPS);
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

void HalesiaEngine::RegisterConsoleVars()
{
	Console::AddConsoleVariables<bool>(
		{ "pauseGame", "showFPS", "playOneFrame", "showRAM", "showCPU", "showGPU", "showAsyncTimes", "showMetaData", "showNormals", "showAlbedo", "showUnique", "renderProgressive", "rasterize", "useEditorUI", "denoiseOutput" },
		{ &pauseGame, &showFPS, &playOneFrame, &showRAM, &showCPU, &showGPU, &showAsyncTimes, &showObjectData, &RayTracing::showNormals, &RayTracing::showAlbedo, &RayTracing::showUniquePrimitives, &RayTracing::renderProgressive, &core.renderer->shouldRasterize, &useEditor, &Renderer::denoiseOutput }
	);
	Console::AddConsoleVariable("raySamples", &RayTracing::raySampleCount);
	Console::AddConsoleVariable("rayDepth", &RayTracing::rayDepth);
	Console::AddConsoleVariable("internalResScale", &Renderer::internalScale);
}