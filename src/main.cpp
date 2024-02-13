#define NOMINMAX
#include "demo/Calculator.h"
#include "io/SceneLoader.h"

class AnimationTest : public Scene
{
	Animation animation;
	AnimationManager* manager;
	void Start() override
	{
		SceneLoader loader{ "stdObj/animation.fbx" };
		loader.LoadFBXScene();
		for (auto& t : loader.objects)
			AddStaticObject(t);
		animation = loader.objects[1].meshes[0].animations[0];
		manager = AnimationManager::Get();
		manager->AddAnimation(&animation);
	}

	void Update(float delta) override
	{
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