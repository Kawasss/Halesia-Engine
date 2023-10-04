#include <Windows.h>

#include "HalesiaEngine.h"
#include "SceneLoader.h"

class TestCamera : public Camera
{
public:
	TestCamera()
	{
		SetScript(this);
	}

	float sumX = 0, sumY = 0;
	void Update(Win32Window* window, float delta) override
	{
		DefaultUpdate(window, delta);
	}
};

class TestObject : public Object
{
public:
	int health = 10;

	TestObject(const ObjectCreationData& creationData, const MeshCreationObject& creationObjects)
	{
		CreateObject(this, creationData, creationObjects);
	}

	float progress = -1;
	void Update(float delta) override
	{
		transform.scale = glm::vec3(0.3f);
		transform.position.z = -1;
	}

	~TestObject()
	{
		Console::WriteLine("a");
	}
};

class RotatingCube : public Object
{
public:
	int index = 0;
	RotatingCube(const ObjectCreationData& creationData, const MeshCreationObject& creationObjects)
	{
		CreateObject(this, creationData, creationObjects);
	}

	float progress = -1;
	void Update(float delta) override
	{
		/*transform.scale = glm::vec3(0.1f);
		progress += delta * 0.001f* index;
		
		transform.position.x = cos(progress);
		transform.position.y = sin(progress);
		transform.position.z = cos(progress) * sin(progress);*/
	}
};

class TestScene : public Scene
{
	Object* objectToPause = nullptr;
	void Start() override
	{
		Object* objPtr = AddCustomObject<RotatingCube>("stdObj/monkey.obj", OBJECT_IMPORT_EXTERNAL);
		objPtr->GetScript<RotatingCube*>()->index = 0;

		this->camera = new TestCamera();
	}
};

int main(int argsCount, char** args)
{
	HalesiaInstance instance{};
	HalesiaInstanceCreateInfo createInfo{};
	createInfo.argsCount = argsCount;
	createInfo.args = args;
	createInfo.startingScene = new TestScene();
	createInfo.sceneFile = "../CORERenderer/halesia.crs";
	createInfo.windowCreateInfo.windowName = L"Halesia Engine";
	createInfo.windowCreateInfo.windowMode = WINDOW_MODE_BORDERLESS_WINDOWED;
	createInfo.windowCreateInfo.height = 600;
	createInfo.windowCreateInfo.width = 800;
	createInfo.windowCreateInfo.setTimer = true;
	createInfo.windowCreateInfo.icon = (HICON)LoadImageW(NULL, L"logo4.ico", IMAGE_ICON, 128, 128, LR_LOADFROMFILE);
	createInfo.windowCreateInfo.style = WindowStyle::OverlappedWindow;
	createInfo.windowCreateInfo.extendedWindowStyle = ExtendedWindowStyle::DragAndDropFiles;
	createInfo.windowCreateInfo.startMaximized = false;

	HalesiaInstance::GenerateHalesiaInstance(instance, createInfo);

	instance.Run();

	return EXIT_SUCCESS;
}