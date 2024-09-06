#include "demo/Topdown.h"
#include "io/SceneWriter.h"

#include "renderer/Renderer.h"
#include "renderer/ForwardPlus.h"
#include "renderer/Deferred.h"
#include "renderer/Light.h"

#include "core/UniquePointer.h"
#include "core/ObjectStreamer.h"

#include "core/Editor.h"

class Room : public Scene
{
	void Start() override
	{
		LoadScene();
	}

	void LoadScene()
	{
		SceneLoader loader("dinner.hsf");
		loader.LoadScene();

		for (const auto& data : loader.materials)
			Mesh::materials.push_back(Material::Create(data));

		for (const auto& data : loader.objects)
		{
			AddStaticObject(data);
		}
	}
};

class StreamerTest : public Scene
{
	UniquePointer<ObjectStreamer> streamer;

	void Start() override
	{
		streamer = new ObjectStreamer(this, "stdObj/streamTest.obj");

		MaterialCreateInfo createInfo{};
		createInfo.isLight = true;
		Material lightMat = Material::Create(createInfo);

		Object* light = AddStaticObject(GenericLoader::LoadObjectFile("stdObj/cube.obj"));
		light->AwaitGeneration();
		light->mesh.SetMaterial(lightMat);
		light->transform.position.y += 10;
		light->transform.scale += glm::vec3(5, 0, 5);
	}

	void Update(float delta) override
	{
		streamer->Poll();
	}
};

class Shooter : public Scene
{
	std::vector<Animation> animations;
	AnimationManager* animManager = nullptr;
	void Start() override
	{
		animManager = AnimationManager::Get();

		SceneLoader loader("stdObj/pistol.gltf");
		loader.LoadScene();

		for (auto& objData : loader.objects)
		{
			if (!objData.hasMesh)
				continue;

			objData.scale = glm::vec3(2);
			AddStaticObject(objData);
		}
			
		animations = std::move(loader.animations);
		for (Animation& anim : animations)
			animManager->AddAnimation(&anim);
	}

	void Update(float delta) override
	{
		animManager->ComputeAnimations(delta);
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
	createInfo.windowCreateInfo.windowName = L"Halesia Test Scene";
	createInfo.windowCreateInfo.width = 800;
	createInfo.windowCreateInfo.height = 600;
	createInfo.windowCreateInfo.windowMode = WINDOW_MODE_WINDOWED;
	createInfo.windowCreateInfo.icon = (HICON)LoadImageW(NULL, L"logo4.ico", IMAGE_ICON, 128, 128, LR_LOADFROMFILE);
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
	light.color = glm::vec3(1, 0, 0);
	light.type  = Light::Type::Point;

	Light light2{};
	light2.pos   = glm::vec4(0, 1, 3, glm::radians(17.5f));
	light2.color = glm::vec3(0, 1, 0);
	light2.direction = glm::vec4(0, -1, 0, glm::radians(12.5f));
	light2.type  = Light::Type::Spot;

	renderer->AddRenderPipeline<DeferredPipeline>();
	renderer->AddLight(light);
	renderer->AddLight(light2);

	instance->Run();

	return EXIT_SUCCESS;
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	return main(__argc, __argv);
}
