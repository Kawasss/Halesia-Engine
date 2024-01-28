#define NOMINMAX
#include "HalesiaEngine.h"
#include "physics/Physics.h" // only for test
#include "io/SceneLoader.h"
#include "Object.h"
#include "Camera.h"
#include "Scene.h"
#include "renderer/Renderer.h"
#include "renderer/RayTracing.h"

class TestScene : public Scene
{
	Material colorMaterial;

	std::vector<Object*> spheres;
	void Start() override
	{
		MaterialCreateInfo createInfo{};
		createInfo.albedo = "textures/rockA.jpg";
		createInfo.normal = "textures/rockN.jpg";

		Object* ptr = AddStaticObject(GenericLoader::LoadObjectFile("stdObj/rock.obj"));
		Material rockMat = Material::Create(createInfo);
		rockMat.AwaitGeneration();
		ptr->meshes[0].SetMaterial(rockMat);
		Shape ptrShape = Box(ptr->meshes[0].extents);
		ptr->AddRigidBody(RIGID_BODY_DYNAMIC, ptrShape);

		MaterialCreateInfo lampInfo{};
		lampInfo.isLight = true;

		Object* lamp = AddStaticObject({ "sphere" });
		lamp->AddMesh(GenericLoader::LoadObjectFile("stdObj/sphere.fbx").meshes);
		Material lampMat = Material::Create(lampInfo);
		lampMat.AwaitGeneration();
		lamp->meshes[0].SetMaterial(lampMat);
		lamp->transform.position = glm::vec3(0, 2, 4);

		Object* cube = AddStaticObject(GenericLoader::LoadObjectFile("stdObj/cube.obj"));
		cube->transform.position.y -= 2;
		Shape cubeShape = Box(cube->meshes[0].extents);
		cube->AddRigidBody(RIGID_BODY_STATIC, cubeShape);
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