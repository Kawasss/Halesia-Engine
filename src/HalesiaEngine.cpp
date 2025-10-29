#include <iostream>
#include <format>
#include <future>
#include <chrono>

#include "HalesiaEngine.h"

#include "system/Input.h"
#include "system/SystemMetrics.h"

#include "renderer/Renderer.h"
#include "renderer/RayTracing.h"
#include "renderer/gui.h"
#include "renderer/AnimationManager.h"
#include "renderer/Vulkan.h"

#include "physics/Physics.h"

#include "core/CameraObject.h"
#include "core/Console.h"
#include "core/Profiler.h"
#include "core/Behavior.h"
#include "core/Scene.h"
#include "core/UniquePointer.h"

#include "io/IniFile.h"

#include "Audio.h"

class ExitRequest : public std::exception {}; // not that great to request an exit via errors

HalesiaEngine* HalesiaEngine::instance = nullptr;

static void ProcessError(const std::exception& e)
{
	std::string fullError = e.what();

	std::string message = std::format("Engine error ({})", typeid(e).name());

	MessageBoxA(nullptr, fullError.c_str(), message.c_str(), MB_OK | MB_ICONERROR);
	std::cerr << e.what() << std::endl;
}

static float CalculateFrameTime(int fps)
{
	if (fps <= 0) return 0;
	return 1000.0f / fps;
}

static float GetMilliseconds(const std::chrono::steady_clock::time_point& start, const std::chrono::steady_clock::time_point& end)
{
	return std::chrono::duration<float, std::chrono::milliseconds::period>(end - start).count();
}

static void WaitForTimeLimit(const std::chrono::steady_clock::time_point& startTime, float timeLimit)
{
	while (GetMilliseconds(startTime, std::chrono::high_resolution_clock::now()) < timeLimit); // wasting cycles
}

static RendererFlags GetRendererFlagsFromBehavior()
{
	RendererFlags ret = Renderer::Flags::None;
	for (int i = 0; i < Behavior::arguments.size(); i++)
	{
		const std::string_view& str = Behavior::arguments[i];
		if (str == "-no_shader_recompilation")
			ret |= Renderer::Flags::NoShaderRecompilation;
		else if (str == "-force_no_ray_tracing")
			ret |= Renderer::Flags::NoRayTracing;
		else if (str == "-vulkan_no_validation")
			Vulkan::DisableValidationLayers();
		else if (str == "-force_gpu" && Behavior::arguments.size() > i + 1)
		{
			std::string name = std::string(Behavior::arguments[++i]);
			while (i < Behavior::arguments.size() - 1 && Behavior::arguments[i][0] != '-') // assemble the full name since the command args are split by spaces
				name += ' ' + std::string(Behavior::arguments[++i]);

			Vulkan::ForcePhysicalDevice(name);
		}
	}

	return ret;
}

HalesiaEngine* HalesiaEngine::CreateInstance(CreateInfo& createInfo)
{
	assert(instance == nullptr); // cannot create an instance after one instance is already created

	std::cout << "Generating Halesia instance:"
		<< "\n  createInfo.startingScene = 0x" << reinterpret_cast<void*>(createInfo.startingScene)
		<< "\n  createInfo.devConsoleKey = "   << static_cast<int>(createInfo.devConsoleKey)
		<< "\n  createInfo.playIntro     = "   << createInfo.playIntro << "\n\n";
	try
	{
		instance = new HalesiaEngine();
		instance->OnLoad(createInfo);

		LogLoadingInformation();
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

void HalesiaEngine::LogLoadingInformation()
{
	const Vulkan::Context& context = Vulkan::GetContext();
	SystemInformation systemInfo = GetCpuInfo();
	VkPhysicalDeviceProperties properties = context.physicalDevice.Properties();

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
		<< "\n  heap 0 (VRAM): " << context.physicalDevice.VRAM() / (1024ull * 1024ull) << " MB"
		<< "\n----------------------------------------\n\n";
}

HalesiaEngine* HalesiaEngine::GetInstance()
{
	assert(instance != nullptr); // may not return an invalid pointer
	return instance;
}

void HalesiaEngine::LoadScene(Scene* newScene)
{
	core.scene->Destroy();
	delete core.scene;
	core.scene = newScene;
}

EngineCore& HalesiaEngine::GetEngineCore()
{
	return core;
}

void HalesiaEngine::UpdateScene(float delta)
{
	SetThreadDescription(GetCurrentThread(), L"SceneUpdatingThread");

	std::chrono::steady_clock::time_point begin = std::chrono::high_resolution_clock::now();

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
	if (showWindowData)
		GUI::ShowWindowData(core.window);

	core.scene->UpdateGUI(delta);

	if (core.renderer->frameCount > 0) // the first frame is automatically recorded
		core.renderer->StartRecording(delta);

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

HalesiaEngine::ExitCode HalesiaEngine::Run()
{
	if (core.renderer == nullptr || core.window == nullptr)
		return ExitCode::UnknownException;

	RegisterConsoleVars();

	try
	{
		float timeSinceLastDataUpdate = 0;
		float frameDelta = 0;
		std::chrono::steady_clock::time_point timeSinceLastFrame = std::chrono::high_resolution_clock::now();

		core.window->SetMaximized(true);

		core.scene->Start();
		while (!core.window->ShouldClose())
		{
			Input::FetchState();
			CheckInput();

			Physics::Simulate(frameDelta);
			Physics::FetchAndUpdateObjects();

			core.scene->PrepareObjectsForUpdate();

			asyncScripts = std::async(&HalesiaEngine::UpdateScene, this, frameDelta);
			asyncRenderer = std::async(&HalesiaEngine::UpdateRenderer, this, frameDelta);

			asyncRenderer.get();
			asyncScripts.get();

			Window::PollMessages();

			if (core.renderer->CompletedFIFCyle())
				core.scene->MainThreadUpdate(frameDelta);

			core.scene->CollectGarbage();

			WaitForTimeLimit(timeSinceLastFrame, CalculateFrameTime(core.maxFPS)); // wait untill the fps limit is reached

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

void HalesiaEngine::InitializeCoreComponents(const CreateInfo& createInfo)
{
	std::cout 
		<< "----------------------------------------\n"
		<< "Initializing core components...\n\nWindow:\n";

	core.window = new Window(createInfo.windowCreateInfo);
	std::cout << "\nRenderer:\n";

	core.renderer = new Renderer(core.window, createInfo.renderFlags | ::GetRendererFlagsFromBehavior());

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

void HalesiaEngine::OnLoad(const CreateInfo& createInfo)
{
	Behavior::ProcessArguments(createInfo.argsCount, createInfo.args);

	InitializeCoreComponents(createInfo);
	InitializeSubSystems();
	
	devConsoleKey = createInfo.devConsoleKey;
	playIntro = createInfo.playIntro;

	assert(createInfo.startingScene != nullptr);
	core.scene = createInfo.startingScene;

	LoadVars();
}

void HalesiaEngine::LoadVars()
{
	ini::Reader reader("cfg/engine.ini");

	showFPS       = reader.GetBool("showFPS");
	core.maxFPS   = reader.GetInt("maxFPS");
	devConsoleKey = static_cast<VirtualKey>(reader.GetFloat("consoleKey"));

	core.renderer->internalScale         = reader.GetFloat("internalRes");
	Renderer::shouldRenderCollisionBoxes = reader.GetBool("renderCollision");
	Renderer::denoiseOutput              = reader.GetBool("denoiseOutput");

	RayTracingRenderPipeline::raySampleCount       = reader.GetInt("raySamples");
	RayTracingRenderPipeline::rayDepth             = reader.GetInt("rayDepth");
	RayTracingRenderPipeline::showNormals          = reader.GetBool("showNormals");
	RayTracingRenderPipeline::showUniquePrimitives = reader.GetBool("showUnique");
	RayTracingRenderPipeline::showAlbedo           = reader.GetBool("showAlbedo");
	RayTracingRenderPipeline::renderProgressive    = reader.GetBool("renderProgressive");

	std::cout << "Finished loading from cfg/engine.ini\n";
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

	ini::Writer writer("cfg/engine.ini");
	writer.SetGroup("engine");

	writer["showFPS"]    = std::to_string(showFPS);
	writer["maxFPS"]     = std::to_string(core.maxFPS);
	writer["consoleKey"] = std::to_string(static_cast<int>(devConsoleKey));

	writer["internalRes"]     = std::to_string(Renderer::internalScale);
	writer["renderCollision"] = std::to_string(Renderer::shouldRenderCollisionBoxes);
	writer["denoiseOutput"]   = std::to_string(Renderer::denoiseOutput);

	writer["raySamples"]        = std::to_string(RayTracingRenderPipeline::raySampleCount);
	writer["rayDepth"]          = std::to_string(RayTracingRenderPipeline::rayDepth);
	writer["showNormals"]       = std::to_string(RayTracingRenderPipeline::showNormals);
	writer["showUnique"]        = std::to_string(RayTracingRenderPipeline::showUniquePrimitives);
	writer["showAlbedo"]        = std::to_string(RayTracingRenderPipeline::showAlbedo);
	writer["renderProgressive"] = std::to_string(RayTracingRenderPipeline::renderProgressive);

	writer.Write();

	std::cout << "Finished writing to cfg/engine.ini\n";

	Destroy();

	delete this;
}

void HalesiaEngine::RegisterConsoleVars()
{
	Console::AddCVar("pauseGame",    &pauseGame);
	Console::AddCVar("showFPS",      &showFPS);
	Console::AddCVar("playFrame",    &playOneFrame);
	Console::AddCVar("showAsync",    &showAsyncTimes);
	Console::AddCVar("showMetaData", &showObjectData);

	Console::AddCVar("showNormals", &RayTracingRenderPipeline::showNormals);
	Console::AddCVar("showAlbedo",  &RayTracingRenderPipeline::showAlbedo);
	Console::AddCVar("showUnique",  &RayTracingRenderPipeline::showUniquePrimitives);
	Console::AddCVar("renderProg",  &RayTracingRenderPipeline::renderProgressive);
	Console::AddCVar("raySamples",  &RayTracingRenderPipeline::raySampleCount);
	Console::AddCVar("rayDepth",    &RayTracingRenderPipeline::rayDepth);

	Console::AddCVar("denoiseOutput", &Renderer::denoiseOutput);
	Console::AddCVar("internalScale", &Renderer::internalScale);

	Console::AddCVar("rasterize",         &core.renderer->shouldRasterize);
	Console::AddCVar("disableAnimations", &core.animationManager->disable);
}

void HalesiaEngine::Exit()
{
	throw ExitRequest();
}