#define NOMINMAX
#include "HalesiaEngine.h"

class TestCamera : public Camera
{
public:
	Object* objectToLookAt = nullptr;

	TestCamera()
	{
		SetScript(this);
		position = glm::vec3(0, 0, 5);
	}

	void Update(Win32Window* window, float delta) override
	{
		if (objectToLookAt != nullptr)
		{
			front = glm::normalize(objectToLookAt->transform.position - position);
			UpdateUpAndRightVectors();
		}	
		else
			DefaultUpdate(window, delta);
	}
};

class RotatingObject : public Object
{
public:
	void Update(float delta) override
	{
		transform.scale.x = 1;
		transform.scale.y = 0.5f;
		transform.scale.z = 1;
		//transform.rotation.y += delta * 0.05f;
		//transform.rotation.y += delta * 0.1f;
	}
};

class FollowCamera : public Object
{
public:
	Camera* cameraToLookAt = nullptr;

	void Update(float delta) override
	{
		if (cameraToLookAt == nullptr)
			return;

		transform.position = cameraToLookAt->position + glm::vec3(0, 0, 1) * cameraToLookAt->front;
	}
};

class TestScene : public Scene
{
	Object* objPtr = nullptr;
	void Start() override
	{
		Object* ptr2 = AddCustomObject<FollowCamera>("stdObj/sniper.obj", OBJECT_IMPORT_EXTERNAL);
		ptr2->transform.scale = glm::vec3(0.1f, 0.1f, 0.1f);
		ptr2->meshes[0].SetMaterial({ new Texture(GetMeshCreationObjects(), "textures/albedo.png"), Texture::placeholderNormal, Texture::placeholderMetallic, new Texture(GetMeshCreationObjects(), "textures/white.png") });
		ptr2->AwaitGeneration();

		Object* ptr = SubmitStaticObject(GenericLoader::LoadObjectFile("stdObj/panel.obj"));
		ptr->meshes[0].SetMaterial({ new Texture(GetMeshCreationObjects(), "textures/blue.png"), Texture::placeholderNormal, Texture::placeholderMetallic, new Texture(GetMeshCreationObjects(), "textures/white.png") });
		ptr->AwaitGeneration();

		ptr = SubmitStaticObject(GenericLoader::LoadObjectFile("stdObj/panel2.obj"));
		ptr->meshes[0].SetMaterial({ new Texture(GetMeshCreationObjects(), "textures/red.png"), Texture::placeholderNormal, Texture::placeholderMetallic, new Texture(GetMeshCreationObjects(), "textures/white.png") });
		ptr->AwaitGeneration();

		ptr = SubmitStaticObject(GenericLoader::LoadObjectFile("stdObj/panel3.obj"));
		ptr->meshes[0].SetMaterial({ Texture::placeholderAlbedo, Texture::placeholderNormal, Texture::placeholderMetallic, new Texture(GetMeshCreationObjects(), "textures/white.png") });
		ptr->AwaitGeneration();

		ptr = SubmitStaticObject(GenericLoader::LoadObjectFile("stdObj/panel4.obj"));
		ptr->meshes[0].SetMaterial({ Texture::placeholderAlbedo, Texture::placeholderNormal, Texture::placeholderMetallic, new Texture(GetMeshCreationObjects(), "textures/white.png") });
		ptr->AwaitGeneration();

		ptr = SubmitStaticObject(GenericLoader::LoadObjectFile("stdObj/panelBottom.obj"));
		ptr->meshes[0].SetMaterial({ new Texture(GetMeshCreationObjects(), "textures/floor.png"), Texture::placeholderNormal, Texture::placeholderMetallic, new Texture(GetMeshCreationObjects(), "textures/white.png") });
		ptr->AwaitGeneration();

		ptr = SubmitStaticObject(GenericLoader::LoadObjectFile("stdObj/panelTop.obj"));
		ptr->meshes[0].SetMaterial({ new Texture(GetMeshCreationObjects(), "textures/white.png"), Texture::placeholderNormal, Texture::placeholderMetallic, new Texture(GetMeshCreationObjects(), "textures/white.png") });
		ptr->AwaitGeneration();

		AddCustomObject<RotatingObject>("stdObj/light.obj", OBJECT_IMPORT_EXTERNAL);
		
		this->camera = new TestCamera();
		camera->GetScript<TestCamera*>()->objectToLookAt = nullptr;//objPtr;
		ptr2->GetScript<FollowCamera*>()->cameraToLookAt = this->camera;
	}

	void Update(float delta) override
	{
		if (Input::IsKeyPressed(VirtualKey::R) && objPtr != nullptr)
		{
			Free(objPtr);
			objPtr = nullptr;
		}
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
	createInfo.playIntro = false;
#endif

	HalesiaInstance::GenerateHalesiaInstance(instance, createInfo);

	instance.Run();

	return EXIT_SUCCESS;
}