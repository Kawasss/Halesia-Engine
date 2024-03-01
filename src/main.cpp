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

class Bullet : public Object
{
public:
	glm::vec3 forward = glm::vec3(0);
	float timeAlive = 0;

	void Start() override
	{
		
	}

	void Update(float delta) override
	{
		timeAlive += delta;
		if (timeAlive > 1000)
		{
			shouldBeDestroyed = true;
			return;
		}

		glm::vec3 forward2D = glm::normalize(glm::vec3(forward.x, 0, forward.z));
		transform.rotation.x = glm::degrees(-asin(glm::dot(forward, glm::vec3(0, 1, 0))));
		transform.rotation.y = glm::degrees(acos(glm::dot(forward2D, glm::vec3(1, 0, 0)))) + 90;
		if (glm::dot(forward, glm::vec3(0, 0, 1)) > 0)
			transform.rotation.y = 360 - transform.rotation.y;

		transform.position += forward * delta * 0.01f;
		rigid.MovePosition(transform);
	}

	void OnCollisionEnter(Object* object) override
	{
		if (object->name == "box")
		{
			forward = glm::vec3(0);
		}
	}
};

class Ship : public Object
{
	static constexpr float fireRate = 1 / 4;
	bool lastFrame = false;
	
	Win32Window* mouse;
	Object* baseBullet;
	void Start() override
	{
		AwaitGeneration();
		baseBullet = scene->AddCustomObject<Bullet>(GenericLoader::LoadObjectFile("stdObj/bullet.obj"));
		baseBullet->name = "bullet";
		Shape shape = Box(baseBullet->meshes[0].extents);
		baseBullet->AddRigidBody(RIGID_BODY_KINEMATIC, shape);
		baseBullet->state = OBJECT_STATE_DISABLED;
		baseBullet->transform.position.y = -5;
		baseBullet->rigid.ForcePosition(baseBullet->transform);
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

		bool thisFrame = Input::IsKeyPressed(VirtualKey::LeftMouseButton);
		if (!thisFrame && lastFrame)
		{
			Object* newBullet = scene->DuplicateCustomObject<Bullet>(baseBullet, "bullet");
			newBullet->name = std::to_string(newBullet->handle);
			Bullet* script = newBullet->GetScript<Bullet>();
			script->forward = transform.GetForward();
			newBullet->transform.position = transform.position + transform.GetForward() * 2.0f;
			newBullet->rigid.ForcePosition(newBullet->transform);
			newBullet->state = OBJECT_STATE_VISIBLE;
		}

		rigid.MovePosition(transform);

		lastFrame = thisFrame;
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

#include "io/SceneWriter.h"
class CollisionTest : public Scene
{
	void Start() override
	{
		WriteScene();
	}
	void ReadScene()
	{
		camera = AddCustomCamera<FollowCam>();

		MaterialCreateInfo lightInfo{};
		lightInfo.isLight = true;
		Material lightMat = Material::Create(lightInfo);

		SceneLoader loader("scene.hsf");
		loader.LoadScene();
		for (MaterialCreationData& data : loader.materials)
			Mesh::materials.push_back(Material::Create(data));
		for (ObjectCreationData& data : loader.objects)
		{
			if (data.name == "ship")
			{
				Object* ship = AddCustomObject<Ship>(data);
				camera->GetScript<FollowCam>()->objToFollow = ship;
			}
			else if (data.name == "light")
			{
				Object* light = AddStaticObject(data);
				light->AwaitGeneration();
				light->meshes[0].SetMaterial(lightMat);
			}
			else AddStaticObject(data)->AwaitGeneration();
		}
	}

	void WriteScene()
	{
		camera = AddCustomCamera<FollowCam>();

		Object* ship = AddCustomObject<Ship>(GenericLoader::LoadObjectFile("stdObj/ship.obj"));
		ship->name = "ship";
		ship->AwaitGeneration();
		Shape shape = Box(ship->meshes[0].extents);
		ship->AddRigidBody(RIGID_BODY_KINEMATIC, shape);
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

		MaterialCreateInfo boxInfo{};
		boxInfo.albedo = "textures/red.png";
		Material boxMat = Material::Create(boxInfo);
		boxMat.AwaitGeneration();

		Object* box = DuplicateStaticObject(floor, "box");
		box->AwaitGeneration();
		box->transform.scale = glm::vec3(1, 1, 1);
		box->transform.position = glm::vec3(5, 0, 0);
		box->meshes[0].SetMaterial(boxMat);

		box->AddRigidBody(RIGID_BODY_DYNAMIC, Box(box->meshes[0].extents));

		HSFWriter::WriteHSFScene(this, "scene.hsf");
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