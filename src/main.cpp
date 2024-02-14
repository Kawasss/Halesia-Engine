#define NOMINMAX
#include "demo/Calculator.h"
#include "io/SceneLoader.h"

#include "system/Input.h"

class AnimationTest : public Scene
{
	AnimationManager* manager;
	std::vector<Animation> animations;
	Object* obj = nullptr;
	Object* light = nullptr;

	void Start() override
	{
		SceneLoader loader{ "stdObj/untitled.fbx" };
		loader.LoadFBXScene();
		obj = AddStaticObject(loader.objects.back());
		
		obj->name = "glock";
		obj->transform.scale = glm::vec3(1);
		obj->transform.rotation.z = 180;
		animations = loader.animations;

		manager = AnimationManager::Get();
		manager->AddAnimation(&animations[0]);

		light = AddStaticObject(GenericLoader::LoadObjectFile("stdObj/cube.obj"));
		light->AwaitGeneration();

		MaterialCreateInfo createInfo{};
		createInfo.isLight = true;
		Material lightMat = Material::Create(createInfo);
		lightMat.AwaitGeneration();

		light->meshes[0].SetMaterial(lightMat);
		light->transform.scale = glm::vec3(100, 1, 100);

		MaterialCreateInfo glockInfo{};
		glockInfo.albedo = "textures/glockAlbedo.png";
		Material glockMat = Material::Create(glockInfo);
		glockMat.AwaitGeneration();

		obj->meshes[0].SetMaterial(glockMat);
	}

	void Update(float delta) override
	{
		static bool wasPressed = false;
		static int index = 0;

		bool isPressed = Input::IsKeyPressed(VirtualKey::RightArrow);
		if (!isPressed && wasPressed)
		{
			animations[index].Reset();
			manager->RemoveAnimation(&animations[index]);
			index = ++index % animations.size();
			std::cout << index << '\n';
			manager->AddAnimation(&animations[index]);
		}
		wasPressed = isPressed;

		light->transform.position = camera->position + glm::vec3(0, 30, 0);

		obj->transform.position = camera->position;
		obj->transform.rotation = glm::vec3(-180, glm::degrees(camera->yaw) + 90, 180);
	}
};

int main(int argsCount, char** args)
{
	HalesiaEngine* instance = nullptr;
	HalesiaEngineCreateInfo createInfo{};
	createInfo.argsCount = argsCount;
	createInfo.args = args;
	createInfo.startingScene = new AnimationTest();
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

	Rotator::window = instance->GetEngineCore().window;

	instance->Run();

	return EXIT_SUCCESS;
}