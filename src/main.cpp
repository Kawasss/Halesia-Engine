#include "renderer/Renderer.h"
#include "renderer/Deferred.h"
#include "renderer/Skybox.h"
#include "renderer/Light.h"
#include "renderer/Mesh.h"

#include "core/Object.h"
#include "core/UniquePointer.h"
#include "core/ObjectStreamer.h"
#include "core/Editor.h"

#include "io/SceneLoader.h"

#include "HalesiaEngine.h"

#include "io/CreationData.h"

class Room : public Scene
{
	void Start() override
	{
		LoadScene();
	}

	void LoadScene()
	{
		ObjectCreationData data = GenericLoader::LoadObjectFile("stdObj/tree.obj");
		Object* obj = AddObject(data);

		MaterialCreateInfo matInfo{};
		matInfo.albedo = "textures/tree.png";

		obj->mesh.SetMaterial(Material::Create(matInfo));
	}
};

int main(int argc, char** argv)
{
	HalesiaEngine* instance = nullptr;
	HalesiaEngineCreateInfo createInfo{};
	createInfo.argsCount = argc;
	createInfo.args = argv;
	createInfo.startingScene = new Editor();
	createInfo.renderFlags = Renderer::Flags::NO_FILTERING_ON_RESULT;
	createInfo.windowCreateInfo.windowName = "Halesia Test Scene";
	createInfo.windowCreateInfo.width = 800;
	createInfo.windowCreateInfo.height = 600;
	createInfo.windowCreateInfo.windowMode = WINDOW_MODE_WINDOWED;
	createInfo.windowCreateInfo.icon = (HICON)LoadImageA(NULL, "logo4.ico", IMAGE_ICON, 128, 128, LR_LOADFROMFILE);
	createInfo.windowCreateInfo.extendedWindowStyle = ExtendedWindowStyle::DragAndDropFiles;
	createInfo.windowCreateInfo.startMaximized = false;
#ifdef _DEBUG
	createInfo.playIntro = false;
#endif

	HalesiaEngine::SetCreateInfo(createInfo);
	instance = HalesiaEngine::GetInstance();

	Renderer* renderer = instance->GetEngineCore().renderer;

	Light light{};
	light.pos   = glm::vec4(0, 1, 0, 0);
	light.color = glm::vec3(1, 0.2f, 0.2f);
	light.type  = Light::Type::Point;

	Light light2{};
	light2.pos   = glm::vec4(0, 1, 3, glm::radians(17.5f));
	light2.color = glm::vec3(0.2f, 1, 0.2f);
	light2.direction = glm::vec4(0, -1, 0, glm::radians(12.5f));
	light2.type  = Light::Type::Point;
	
	renderer->AddRenderPipeline<DeferredPipeline>("deferred");
	renderer->AddLight(light);
	renderer->AddLight(light2);
	
	DeferredPipeline* deferred = dynamic_cast<DeferredPipeline*>(renderer->GetRenderPipeline("deferred"));
	deferred->LoadSkybox("textures/skybox/park.hdr");

	instance->Run();

	return EXIT_SUCCESS;
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	return main(__argc, __argv);
}
