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
	windowInfo.width = 800;
	windowInfo.height = 600;
	windowInfo.windowMode = Window::Mode::Windowed;
	windowInfo.icon = "logo4.ico";
	windowInfo.extendedStyle = Window::ExtendedStyle::DragAndDropFiles;
	windowInfo.startMaximized = false;

	HalesiaEngine::CreateInfo createInfo{};
	createInfo.argsCount = argc;
	createInfo.args = argv;
	createInfo.startingScene = new Editor();
	createInfo.renderFlags = Renderer::Flags::NO_FILTERING_ON_RESULT;
	createInfo.windowCreateInfo = windowInfo;
#ifdef _DEBUG
	createInfo.playIntro = false;
#endif // _DEBUG

	HalesiaEngine* instance = HalesiaEngine::CreateInstance(createInfo);
	Renderer*      renderer = instance->GetEngineCore().renderer;

	renderer->AddRenderPipeline<DeferredPipeline>("deferred");
	
	DeferredPipeline* deferred = dynamic_cast<DeferredPipeline*>(renderer->GetRenderPipeline("deferred"));
	deferred->LoadSkybox("textures/skybox/park.hdr");

	instance->Run();

	return EXIT_SUCCESS;
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	return main(__argc, __argv);
}
