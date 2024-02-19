#define NOMINMAX
#include "demo/Calculator.h"
#include "io/SceneLoader.h"

#include "system/Input.h"

class FollowCam : public Camera
{
public:
	Object* objToFollow = nullptr;

	void Start() override
	{
		position.y = 8;
	}

	void Update(Win32Window* window, float delta) override
	{
		position = objToFollow->transform.position;
		position.y = 8;
		position.z += 5;
		pitch = glm::radians(-45.0f);
		UpdateVectors();
	}
};

class Ship : public Object
{
	Win32Window* mouse;
	void Start() override
	{
		AwaitGeneration();
		Shape shape = Box(meshes[0].extents);
		AddRigidBody(RIGID_BODY_KINEMATIC, shape);
		mouse = HalesiaEngine::GetInstance()->GetEngineCore().window;
	}

	void Update(float delta) override
	{
		int x, y;
		mouse->GetRelativeCursorPosition(x, y);
		transform.rotation += glm::vec3(0, cos(x) * sin(y), 0);

		if (!Input::IsKeyPressed(VirtualKey::LeftControl))
			return;
		
		if (Input::IsKeyPressed(VirtualKey::W))
			transform.position.z -= delta * 0.01f;
		if (Input::IsKeyPressed(VirtualKey::S))
			transform.position.z += delta * 0.01f;
		if (Input::IsKeyPressed(VirtualKey::A))
			transform.position.x -= delta * 0.01f;
		if (Input::IsKeyPressed(VirtualKey::D))
			transform.position.x += delta * 0.01f;

		rigid.MovePosition(transform);
	}

	void OnCollisionEnter(Object* object) override
	{
		std::cout << "collided with " << object->name << '\n';
	}
	void OnCollisionStay(Object* object) override
	{
		std::cout << __FUNCTION__ << '\n';
	}
	void OnCollisionExit(Object* object) override
	{
		std::cout << __FUNCTION__ << '\n';
	}
};

class CollisionTest : public Scene
{
	void Start() override
	{
		camera = AddCustomCamera<FollowCam>();

		Object* ship = AddCustomObject<Ship>(GenericLoader::LoadObjectFile("stdObj/cube.obj"));
		ship->AwaitGeneration();
		camera->GetScript<FollowCam>()->objToFollow = ship;

		Object* floor = AddStaticObject(GenericLoader::LoadObjectFile("stdObj/cube.obj"));
		floor->AwaitGeneration();
		floor->name = "floor";
		floor->transform.scale = glm::vec3(20, 1, 20);
		floor->transform.position.y = -3;

		Shape floorShape = Box(glm::vec3(20, 1, 20));
		floor->AddRigidBody(RIGID_BODY_STATIC, floorShape);
		floor->rigid.ForcePosition(floor->transform);

		Object* light = DuplicateStaticObject(floor, "light");
		light->transform.position.y = 10;

		MaterialCreateInfo lightInfo{};
		lightInfo.isLight = true;
		Material lightMat = Material::Create(lightInfo);
		light->meshes[0].SetMaterial(lightMat);

		Object* box = DuplicateStaticObject(floor, "box");
		box->AwaitGeneration();
		box->transform.scale = glm::vec3(1, 1, 1);
		box->transform.position = glm::vec3(5, 0, 0);

		Shape boxShape = Box(box->meshes[0].extents);
		box->AddRigidBody(RIGID_BODY_DYNAMIC, boxShape);
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
	createInfo.startingScene = new CollisionTest();
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