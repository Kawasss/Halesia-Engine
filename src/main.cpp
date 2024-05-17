#include "demo/Topdown.h"
#include "io/SceneWriter.h"

#include "core/UniquePointer.h"
#include "core/ObjectStreamer.h"

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
	createInfo.startingScene = new Shooter();
	createInfo.windowCreateInfo.windowName = L"Halesia Test Scene";
	createInfo.windowCreateInfo.width = 800;
	createInfo.windowCreateInfo.height = 600;
	createInfo.windowCreateInfo.windowMode = WINDOW_MODE_WINDOWED;
	createInfo.windowCreateInfo.icon = (HICON)LoadImageW(NULL, L"logo4.ico", IMAGE_ICON, 128, 128, LR_LOADFROMFILE);
	createInfo.windowCreateInfo.extendedWindowStyle = ExtendedWindowStyle::DragAndDropFiles;
	createInfo.windowCreateInfo.startMaximized = false;
#ifdef _DEBUG
	createInfo.useEditor = true;
	createInfo.playIntro = false;
#endif

	HalesiaEngine::SetCreateInfo(createInfo);
	instance = HalesiaEngine::GetInstance();
	instance->Run();

	return EXIT_SUCCESS;
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	return main(__argc, __argv);
}
