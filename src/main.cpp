#include "renderer/Renderer.h"
#include "renderer/Deferred.h"
#include "renderer/Skybox.h"
#include "renderer/GraphicsPipeline.h"
#include "renderer/RayTracingPipeline.h"
#include "renderer/AccelerationStructures.h"
#include "renderer/ComputeShader.h"

#include "core/Editor.h"

#include "HalesiaEngine.h"

#include "io/CreationData.h"

int main(int argc, char** argv)
{
	Window::CreateInfo windowInfo{};
	windowInfo.name = "Halesia Test Scene";
	windowInfo.icon = "logo4.ico";
	windowInfo.width = 800;
	windowInfo.height = 600;
	windowInfo.windowMode = Window::Mode::Windowed;
	windowInfo.extendedStyle = Window::ExtendedStyle::DragAndDropFiles;
	windowInfo.startMaximized = false;

	HalesiaEngine::CreateInfo createInfo{};
	createInfo.argsCount = argc;
	createInfo.args = argv;
	createInfo.startingScene = new Editor(); // launch the engines built-in editor
	createInfo.renderFlags = Renderer::Flags::NoFilteringOnResult; // the renderer must not filter the final frame for crisp resolution scaling
	createInfo.windowCreateInfo = windowInfo;

#ifdef _DEBUG
	createInfo.playIntro = false;
#endif // _DEBUG

	HalesiaEngine* instance = HalesiaEngine::CreateInstance(createInfo);
	Renderer*      renderer = instance->GetEngineCore().renderer;

	DeferredPipeline* deferred = renderer->AddRenderPipeline<DeferredPipeline>("deferred"); // choose the deferred pipeline for rendering
	deferred->LoadSkybox("textures/skybox/park.hdr");

	instance->Run();

	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDOUT);

	return EXIT_SUCCESS;
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	return main(__argc, __argv);
}
