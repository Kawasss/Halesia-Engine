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
		transform.rotation.y += delta * 0.1f;
	}
};

class TestScene : public Scene
{
	Object* objPtr = nullptr;
	void Start() override
	{
		SubmitStaticObject(GenericLoader::LoadObjectFile("stdObj/monkey3.obj", 0));
		AddCustomObject<RotatingObject>("stdObj/cube.obj", OBJECT_IMPORT_EXTERNAL);

		this->camera = new TestCamera();
		camera->GetScript<TestCamera*>()->objectToLookAt = nullptr;//objPtr;
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

	HalesiaInstance::GenerateHalesiaInstance(instance, createInfo);

	instance.Run();

	return EXIT_SUCCESS;
}