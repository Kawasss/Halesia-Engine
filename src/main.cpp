#define NOMINMAX
#include "HalesiaEngine.h"
#include "physics/Physics.h" // only for test
#include "io/SceneLoader.h"

#include "core/Object.h"
#include "core/Camera.h"
#include "core/Scene.h"

#include "renderer/Renderer.h"
#include "renderer/RayTracing.h"

class TestCam : public Camera
{
public:
	void Start() override
	{
		position = glm::vec3(0, 9, -1);
		SetPitch(-90);
		UpdateVectors();
	}

	//void Update(Win32Window* window, float delta) {}
};

class Rotator : public Object
{
	void Update(float delta) override
	{
		transform.rotation.x += delta * 0.01f;
	}
};

class Key : public Object
{
	void Update(float delta) override
	{
		transform.position.y = Renderer::selectedHandle == handle ? -0.2f : 0;
	}
};

class TestScene : public Scene
{
	Material colorMaterial;

	std::vector<Object*> spheres;
	void Start() override
	{
		camera = new TestCam();
		camera->Start();

		Object* rotator = AddCustomObject<Rotator>(ObjectCreationData{ "rotator" });

		SceneLoader loader{ "stdObj/calculator.fbx" };
		loader.LoadFBXScene();
		
		for (auto& info : loader.objects)
		{
			Object* obj = nullptr;
			if (info.name.back() == '4' && info.name[info.name.size() - 2] == '0')
				obj = AddStaticObject(info);
			else
				obj = AddCustomObject<Key>(info);
			rotator->AddChild(obj);
		}

		MaterialCreateInfo lampInfo{};
		lampInfo.isLight = true;

		Object* lamp = AddStaticObject({ "cube" });
		lamp->AddMesh(GenericLoader::LoadObjectFile("stdObj/cube.obj").meshes);
		Material lampMat = Material::Create(lampInfo);
		lampMat.AwaitGeneration();
		lamp->meshes[0].SetMaterial(lampMat);
		lamp->transform.position = glm::vec3(0, 10, 0);
		lamp->transform.scale = glm::vec3(10, 1, 10);
	}

	void Update(float delta) override
	{
		if (allObjects.empty())
			return;
	}
};

int main(int argsCount, char** args)
{
	HalesiaInstance instance{};
	HalesiaInstanceCreateInfo createInfo{};
	createInfo.argsCount = argsCount;
	createInfo.args = args;
	createInfo.startingScene = new TestScene();
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

	HalesiaInstance::GenerateHalesiaInstance(instance, createInfo);
	
	instance.Run();

	return EXIT_SUCCESS;
}