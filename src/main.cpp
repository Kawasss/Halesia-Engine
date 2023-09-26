#include <Windows.h>

#include "HalesiaEngine.h"
#include "SceneLoader.h"

class TestCamera : public Camera
{
public:
	Object* objectToView;

	TestCamera()
	{
		SetScript(this);
	}

	void Update(Win32Window* window, float delta) override
	{
		if (Input::IsKeyPressed(VirtualKey::R))
		{
			pitch = 0;
			yaw = -(glm::pi<float>() / 2);
		}
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
		
		//if (progress >= 2 * glm::pi<float>())
		//{
			//transform.position.x = cos(progress) * 2;
		//}
		/*if (Input::IsKeyPressed(VirtualKey::R))
			transform.rotation.y += delta * 0.1f;
		if (Input::IsKeyPressed(VirtualKey::T))
			transform.position += delta * 0.1f * transform.GetForward();
		if (Input::IsKeyPressed(VirtualKey::Space))
			transform.position.y += delta * 0.1f;*/
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
		transform.scale = glm::vec3(0.1f);
		progress += delta * 0.001f* index;
		//if (progress >= 2 * glm::pi<float>())
		//{
		transform.position.x = cos(progress);
		transform.position.y = sin(progress);
		transform.position.z = cos(progress) * sin(progress);
		//}
	}
};

class TestScene : public Scene
{
	Object* objectToPause = nullptr;
	void Start() override
	{
		//AddCustomObject<TestObject>("blahaj.obj", OBJECT_IMPORT_EXTERNAL);
		objectToPause = AddCustomObject<TestObject>("monkey");
		for (int i = 0; i < 10; i++)
		{
			Object* objPtr = AddCustomObject<RotatingCube>("stdObj/monkey.obj", OBJECT_IMPORT_EXTERNAL);
			objPtr->GetScript<RotatingCube*>()->index = i + 1;
		}

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