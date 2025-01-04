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

#include "core/Camera.h"
#include "core/Console.h"
#include "core/Profiler.h"
#include "core/Behavior.h"
#include "core/Scene.h"
#include "core/UniquePointer.h"

#include "vvm/VVM.hpp"

#include "Audio.h"

class ExitRequest : public std::exception {}; // not that great to request an exit via errors

HalesiaEngineCreateInfo HalesiaEngine::createInfo{};

inline void ProcessError(const std::exception& e)
{
	std::string fullError = e.what();
	MessageBoxA(nullptr, fullError.c_str(), ((std::string)"Engine error (" + (std::string)typeid(e).name() + ')').c_str(), MB_OK | MB_ICONERROR);
	std::cerr << e.what() << std::endl;

#ifdef _DEBUG
	__debugbreak();
#endif
}

inline float CalculateFrameTime(int fps)
{
	if (fps <= 0) return 0;
	return 1000.0f / fps;
}

inline RendererFlags GetRendererFlagsFromBehavior()
{
	RendererFlags ret = Renderer::Flags::NONE;
	for (int i = 0; i < Behavior::arguments.size(); i++)
	{
		const std::string& str = Behavior::arguments[i];
		if (str == "-no_shader_recompilation")
			ret |= Renderer::Flags::NO_SHADER_RECOMPILATION;
		else if (str == "-force_no_ray_tracing")
			ret |= Renderer::Flags::NO_RAY_TRACING;
		else if (str == "-vulkan_no_validation")
			Vulkan::DisableValidationLayers();
		else if (str == "-force_gpu" && Behavior::arguments.size() > i + 1)
		{
			std::string name = Behavior::arguments[++i];
			while (i < Behavior::arguments.size() - 1 && Behavior::arguments[i][0] != '-') // assemble the full name since the command args are split by spaces
				name += ' ' + Behavior::arguments[++i];

			Vulkan::ForcePhysicalDevice(name);
		}
	}

	return ret;
}

HalesiaEngine* HalesiaEngine::GetInstance()
{
	static HalesiaEngine* instance = nullptr;
	static bool init = false;
	if (init)
		return instance;

	std::cout << "Generating Halesia instance:" 
		<< "\n  createInfo.startingScene = " << reinterpret_cast<uint64_t>(createInfo.startingScene) 
		<< "\n  createInfo.devConsoleKey = " << static_cast<int>(createInfo.devConsoleKey)
		<< "\n  createInfo.playIntro     = " << createInfo.playIntro << "\n\n";
	try
	{
		instance = new HalesiaEngine;
		instance->OnLoad(createInfo);

		const Vulkan::Context& context = Vulkan::GetContext();
		SystemInformation systemInfo = GetCpuInfo();
		VkPhysicalDeviceProperties properties = context.physicalDevice.Properties();
		uint64_t vram = context.physicalDevice.VRAM();

		std::cout
			<< "\n----------------------------------------"
			<< "\nSystem info:"
			<< "\n  CPU:           " << systemInfo.CPUName
			<< "\n  thread count:  " << systemInfo.processorCount
			<< "\n  physical RAM:  " << systemInfo.installedRAM / 1024 << " MB\n"
			<< "\n  GPU:           " << properties.deviceName
			<< "\n  type:          " << (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "discrete" : "integrated")
			<< "\n  vulkan driver: " << properties.driverVersion
			<< "\n  API version:   " << properties.apiVersion
			<< "\n  heap 0 (VRAM): " << vram / (1024ull * 1024ull) << " MB"
			<< "\n----------------------------------------\n\n";
		
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
	return instance;
}

void HalesiaEngine::SetCreateInfo(const HalesiaEngineCreateInfo& createInfo)
{
	HalesiaEngine::createInfo = createInfo;
}

void HalesiaEngine::LoadScene(Scene* newScene)
{
	core.scene->Destroy();
	delete core.scene;
	core.scene = newScene;
}

inline void ManageCameraInjector(Scene* scene, bool pauseGame)
{
	static CameraInjector cameraInjector;
	static UniquePointer orbitCamera = new OrbitCamera();

	if (pauseGame && !cameraInjector.IsInjected())
	{
		cameraInjector = CameraInjector{ scene };
		cameraInjector.Inject(orbitCamera.Get());
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

	if (!pauseGame || playOneFrame)
		core.animationManager->ComputeAnimations(delta);

	if (showFPS)
		GUI::ShowFPS((int)(1 / delta * 1000));

	if (showAsyncTimes)
		GUI::ShowPieGraph(asyncTimes, "Async Times (µs)");
	if (showObjectData)
		GUI::ShowObjectTable(core.scene->allObjects);
	
	core.scene->UpdateGUI(delta);

	//if (core.renderer->frameCount > 0) // the first frame is automatically recorded
		core.renderer->StartRecording();

	core.renderer->RenderObjects(core.scene->allObjects, core.scene->camera);

	core.renderer->SubmitRecording();

	asyncRendererCompletionTime = std::chrono::duration<float, std::chrono::milliseconds::period>(std::chrono::high_resolution_clock::now() - begin).count();
}

void HalesiaEngine::CheckInput()
{
	playOneFrame = Input::IsKeyJustPressed(VirtualKey::RightArrow) && Input::IsKeyJustPressed(VirtualKey::LeftControl);

	if (Input::IsKeyJustReleased(devConsoleKey))
		Console::isOpen = !Console::isOpen;
}

void HalesiaEngine::UpdateAsyncCompletionTimes(float frameDelta)
{
	float timeSpentInMainThread = frameDelta - asyncScriptsCompletionTime - asyncRendererCompletionTime;

	asyncTimes.push_back(timeSpentInMainThread * 1000);
	asyncTimes.push_back(asyncScriptsCompletionTime * 1000);
	asyncTimes.push_back(asyncRendererCompletionTime * 1000);
}

void HalesiaEngine::PlayIntro()
{
	if (!playIntro)
		return;

    UniquePointer intro = new Intro();
	intro->Create(core.renderer->swapchain, "textures/floor.png");

	core.renderer->RenderIntro(intro.Get());
	intro->Destroy();
}

HalesiaEngine::ExitCode HalesiaEngine::Run()
{
	std::string lastCommand;
	float timeSinceLastDataUpdate = 0;

	if (core.renderer == nullptr || core.window == nullptr/* || physics == nullptr*/)
		return ExitCode::UnknownException;

	RegisterConsoleVars();

	try
	{
		float frameDelta = 0;
		std::chrono::steady_clock::time_point timeSinceLastFrame = std::chrono::high_resolution_clock::now();

		core.window->SetMaximized(true);

		PlayIntro();
		
		core.scene->Start();
		while (!core.window->ShouldClose())
		{
			Input::FetchState();
			CheckInput();

			Physics::Simulate(frameDelta);
			Physics::FetchAndUpdateObjects();

			asyncScripts = std::async(&HalesiaEngine::UpdateScene, this, frameDelta);
			asyncRenderer = std::async(&HalesiaEngine::UpdateRenderer, this, frameDelta);

			asyncRenderer.get();
			
			if (showWindowData)
				GUI::ShowWindowData(core.window); // only works on main thread, because it calls windows functions for changing the window

			Window::PollMessages();
			
			asyncScripts.get();

			if (core.renderer->CompletedFIFCyle())
				core.scene->MainThreadUpdate(frameDelta);

			core.scene->CollectGarbage();

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
		return ExitCode::Success;
	}
	catch (const ExitRequest& exit)
	{
		OnExit();
		return ExitCode::Success;
	}
	catch (const std::exception& e) //catch any normal exception and return
	{
		std::string fullError = e.what();
		ProcessError(e);
		OnExit();
		return ExitCode::Exception;
	}
	catch (...) //catch any unknown exceptions and return, doesnt catch any read or write errors etc.
	{
		MessageBoxA(nullptr, "Caught an unknown error, this build is most likely corrupt and can't be used.", "Unknown engine error", MB_OK | MB_ICONERROR);
		return ExitCode::UnknownException;
	}
}

void HalesiaEngine::InitializeCoreComponents()
{
	std::cout 
		<< "----------------------------------------\n"
		<< "Initializing core components...\n\nWindow:\n";

	core.window = new Window(createInfo.windowCreateInfo);
	std::cout << "\nRenderer:\n";
	core.renderer = new Renderer(core.window, createInfo.renderFlags | GetRendererFlagsFromBehavior());
	std::cout << "\nProfiler:\n";
	core.profiler = Profiler::Get();
	core.profiler->SetFlags(Profiler::ALL_OPTIONS);
	std::cout << "\nAnimation manager:\n";
	core.animationManager = AnimationManager::Get();

	std::cout << "----------------------------------------\n\n";
}

void HalesiaEngine::InitializeSubSystems()
{
	std::cout
		<< "----------------------------------------\n"
		<< "Initializing sub systems...\n\nAudio engine:\n";

	Audio::Init();
	std::cout << "\nConsole:\n";
	Console::Init();
	std::cout << "\nPhysics engine:\n";
	Physics::Init();

	std::cout << "----------------------------------------\n\n";
}

void HalesiaEngine::OnLoad(HalesiaEngineCreateInfo& createInfo)
{
	Behavior::ProcessArguments(createInfo.argsCount, createInfo.args);

	InitializeCoreComponents();
	InitializeSubSystems();
	
	devConsoleKey = createInfo.devConsoleKey;
	playIntro = createInfo.playIntro;

	if (createInfo.startingScene == nullptr)
	{
		Console::WriteLine("The given HalesiaInstanceCreateInfo doesn't contain a valid starting scene", Console::Severity::Error);
		core.scene = new Scene();
	}
	else
		core.scene = createInfo.startingScene;

	LoadVars();
}

void HalesiaEngine::LoadVars()
{
	VVM::Reader reader;
	std::vector<VVM::Group> groups;
	if (reader.ReadFromFile("cfg/vars.vvm", groups) != VVM_SUCCESS)
		return;

	showFPS = reader.FindVariable("engineCore.showFPS", groups).As<bool>();
	core.maxFPS = reader.FindVariable("engineCore.maxFPS", groups).As<int>();
	devConsoleKey = (VirtualKey)reader.FindVariable("engineCore.consoleKey", groups).As<int>();

	core.renderer->internalScale = reader.FindVariable("renderer.internalRes", groups).As<float>();
	Renderer::shouldRenderCollisionBoxes = reader.FindVariable("renderer.renderCollision", groups).As<bool>();
	Renderer::denoiseOutput = reader.FindVariable("renderer.denoiseOutput", groups).As<bool>();
	RayTracingPipeline::raySampleCount = reader.FindVariable("renderer.ray-tracing.raySamples", groups).As<int>();
	RayTracingPipeline::rayDepth = reader.FindVariable("renderer.ray-tracing.rayDepth", groups).As<int>();
	RayTracingPipeline::showNormals = reader.FindVariable("renderer.ray-tracing.showNormals", groups).As<bool>();
	RayTracingPipeline::showUniquePrimitives = reader.FindVariable("renderer.ray-tracing.showUnique", groups).As<bool>();
	RayTracingPipeline::showAlbedo = reader.FindVariable("renderer.ray-tracing.showAlbedo", groups).As<bool>();
	RayTracingPipeline::renderProgressive = reader.FindVariable("renderer.ray-tracing.renderProgressive", groups).As<bool>();

	std::cout << "Finished loading from cfg/vars.vvm\n";
}

void HalesiaEngine::Destroy()
{
	delete core.scene;
	delete core.renderer;
	delete core.window;
}

void HalesiaEngine::OnExit()
{
	Audio::Destroy();

	VVM::Writer writer;

	writer.PushGroup("engineCore");
	writer.AddVariable("showFPS", showFPS);
	writer.AddVariable("maxFPS", core.maxFPS);
	writer.AddVariable("consoleKey", (int)devConsoleKey);
	writer.PopGroup();

	writer.PushGroup("renderer");
	writer.AddVariable("interalRes", Renderer::internalScale);
	writer.AddVariable("renderCollision", Renderer::shouldRenderCollisionBoxes);
	writer.AddVariable("denoiseOutput", Renderer::denoiseOutput);

	writer.PushGroup("ray-tracing");
	writer.AddVariable("raySamples", RayTracingPipeline::raySampleCount);
	writer.AddVariable("rayDepth", RayTracingPipeline::rayDepth);
	writer.AddVariable("showNormals", RayTracingPipeline::showNormals);
	writer.AddVariable("showUnique", RayTracingPipeline::showUniquePrimitives);
	writer.AddVariable("showAlbedo", RayTracingPipeline::showAlbedo);
	writer.AddVariable("renderProgressive", RayTracingPipeline::renderProgressive);
	writer.PopGroup();
	writer.PopGroup();

	writer.WriteToFile("cfg/vars.vvm");
	std::cout << "Finished writing to cfg/vars.vvm\n";

	Destroy();

	delete this;
}

void HalesiaEngine::RegisterConsoleVars()
{
	Console::AddConsoleVariables<bool>(
		{ "pauseGame", "showFPS", "playOneFrame", "showAsyncTimes", "showMetaData", "showNormals", "showAlbedo", "showUnique", "renderProgressive", "rasterize", "denoiseOutput", "disableAnimations" },
		{ &pauseGame, &showFPS, &playOneFrame, &showAsyncTimes, &showObjectData, &RayTracingPipeline::showNormals, &RayTracingPipeline::showAlbedo, &RayTracingPipeline::showUniquePrimitives, &RayTracingPipeline::renderProgressive, &core.renderer->shouldRasterize, &Renderer::denoiseOutput, &core.animationManager->disable }
	);
	Console::AddConsoleVariable("raySamples", &RayTracingPipeline::raySampleCount);
	Console::AddConsoleVariable("rayDepth", &RayTracingPipeline::rayDepth);
	Console::AddConsoleVariable("internalResScale", &Renderer::internalScale);
}

void HalesiaEngine::Exit()
{
	throw ExitRequest();
}