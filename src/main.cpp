#include <Windows.h>

#include "HalesiaEngine.h"

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
		if (window->CursorIsLocked())
		{
			front = objectToView->transform.GetForward();
			position.y = 10;
			right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));
			up = glm::normalize(glm::cross(right, front));
		}
	}
};

class TestObject : public Object
{
public:
	int health = 10;

	TestObject(std::string path, const MeshCreationObjects& creationObjects)
	{
		CreateObjectAsync(this, path, creationObjects);
	}

	void Update(float delta) override
	{
		transform.scale = glm::vec3(0.1);
		if (Input::IsKeyPressed(VirtualKey::R))
			transform.rotation.y += delta * 0.1f;
		if (Input::IsKeyPressed(VirtualKey::T))
			transform.position += delta * 0.1f * transform.GetForward();
		if (Input::IsKeyPressed(VirtualKey::Space))
			transform.position.y += delta * 0.1f;
	}

	~TestObject()
	{
		Console::WriteLine("a");
	}
};

class TestScene : public Scene
{
	void Start() override
	{
		AddCustomObject<TestObject>("./blahaj.obj")->AwaitGeneration();
		this->camera = new TestCamera();
		camera->GetScript<TestCamera*>()->objectToView = FindObjectByName("blahaj");
	}
};

int main(int argsCount, char** args)
{
	HalesiaInstance instance{};
	HalesiaInstanceCreateInfo createInfo{};
	createInfo.argsCount = argsCount;
	createInfo.args = args;
	createInfo.startingScene = new TestScene();
	createInfo.windowCreateInfo.windowName = L"Halesia Engine";
	createInfo.windowCreateInfo.windowMode = WINDOW_MODE_BORDERLESS_WINDOWED;
	createInfo.windowCreateInfo.height = 600;
	createInfo.windowCreateInfo.width = 800;
	createInfo.windowCreateInfo.setTimer = true;
	createInfo.windowCreateInfo.icon = (HICON)LoadImageW(NULL, L"logo4.ico", IMAGE_ICON, 128, 128, LR_LOADFROMFILE);
	createInfo.windowCreateInfo.extendedWindowStyle = ExtendedWindowStyle::DragAndDropFiles;
	createInfo.windowCreateInfo.startMaximized = false;

	HalesiaInstance::GenerateHalesiaInstance(instance, createInfo);

	instance.Run();

	return EXIT_SUCCESS;
}