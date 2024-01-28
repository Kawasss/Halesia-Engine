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
		MaterialCreateInfo mirrorInfo{};
		mirrorInfo.roughness = "textures/black.png";
		Material mirror = Material::Create(mirrorInfo);
		mirror.AwaitGeneration();

		MaterialCreateInfo wall1Info = mirrorInfo;
		wall1Info.albedo = "textures/red.png";
		Material wall1Mat = Material::Create(wall1Info);
		wall1Mat.AwaitGeneration();
		
		MaterialCreateInfo wall2Info = mirrorInfo;
		wall2Info.albedo = "textures/blue.png";
		Material wall2Mat = Material::Create(wall2Info);
		wall2Mat.AwaitGeneration();

		for (int i = 0; i < 6; i++)
		{
			Object* obj = AddStaticObject(GenericLoader::LoadObjectFile("stdObj/RTXCube/panel" + std::to_string(i) + ".obj"));
			Material& mat = i == 0 ? wall1Mat : i == 1 ? wall2Mat : mirror;
			obj->meshes[0].SetMaterial(mat);
		}

		MaterialCreateInfo lightInfo{};
		lightInfo.isLight = true;
		Material lightMat = Material::Create(lightInfo);
		lightMat.AwaitGeneration();

		Object* light = AddStaticObject(GenericLoader::LoadObjectFile("stdObj/RTXCube/light.obj"));
		light->meshes[0].SetMaterial(lightMat);

		MaterialCreateInfo monkeyInfo{};
		monkeyInfo.roughness = "textures/grey.png";
		Material monkeyMat = Material::Create(monkeyInfo);
		monkeyMat.AwaitGeneration();

		Object* monkey = AddStaticObject(GenericLoader::LoadObjectFile("stdObj/RTXCube/monkey.obj"));
		monkey->meshes[0].SetMaterial(monkeyMat);
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