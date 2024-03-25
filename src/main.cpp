#include "demo/Topdown.h"

class Room : public Scene
{
	void Start() override
	{
		SceneLoader loader("stdObj/room.fbx");
		loader.LoadFBXScene();

		MaterialCreateInfo lightInfo{};
		lightInfo.isLight = true;
		Material lightMat = Material::Create(lightInfo);

		MaterialCreateInfo lampInfo{};
		lampInfo.albedo = "textures/red.png";
		lampInfo.isLight = true;
		Material lampMat  = Material::Create(lampInfo);
		lampMat.AwaitGeneration();

		for (const ObjectCreationData& data : loader.objects)
			data.name == "Backdrop" ? AddStaticObject(data)->meshes[0].SetMaterial(lightMat) : data.name[0] == '#' ? AddStaticObject(data)->meshes[0].SetMaterial(lampMat) : (void)AddStaticObject(data);
	}

	void Update(float delta) override
	{
		static float addage = 0;
		if (Input::IsKeyPressed(VirtualKey::R))
		{
			Color color(0.7f + addage, 1.0f, 1.0f - addage);
			addage = fmod(addage + delta * 0.001f, 1.0f);
			if (const Object* light = GetObjectByName("Backdrop"))
			{
				Mesh::materials[light->meshes[0].materialIndex].albedo->ChangeData((uint8_t*)&color, sizeof(Color), TEXTURE_FORMAT_UNORM);
			}
		}
	}
};

int main(int argsCount, char** args)
{
	HalesiaEngine* instance = nullptr;
	HalesiaEngineCreateInfo createInfo{};
	createInfo.argsCount = argsCount;
	createInfo.args = args;
	createInfo.startingScene = new Room();
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