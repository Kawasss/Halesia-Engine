#define NOMINMAX
#include "HalesiaEngine.h"
#include "physics/Physics.h" // only for test

class TestCamera : public OrbitCamera
{
public:
	glm::vec3 pivot = glm::vec3(0);
	float radius = 5;
	float sumX = 0, sumY = 0;

	TestCamera()
	{
		SetScript(this);
		position = glm::vec3(0, 0, 5);
	}

	void Update(Win32Window* window, float delta) override
	{
		if (Console::isOpen)
			return;

		radius += window->GetWheelRotation() * delta * 0.001f;
		if (radius < 0.1f) radius = 0.1f;

		float phi = sumX * 2 * glm::pi<float>() / window->GetWidth();
		float theta = std::clamp(sumY * glm::pi<float>() / window->GetHeight(), -0.49f * glm::pi<float>(), 0.49f * glm::pi<float>());

		position.x = radius * (cos(phi) * cos(theta));
		position.y = radius * sin(theta);
		position.z = radius * (cos(theta) * sin(phi));
		position += pivot;

		front = glm::normalize(pivot - position);

		right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));
		up = glm::normalize(glm::cross(right, front));

		if (Input::IsKeyPressed(VirtualKey::W))
			pivot += front * (cameraSpeed * delta * 0.001f);
		if (Input::IsKeyPressed(VirtualKey::S))
			pivot -= front * (cameraSpeed * delta * 0.001f);
		if (Input::IsKeyPressed(VirtualKey::A))
			pivot -= right * (cameraSpeed * delta * 0.001f);
		if (Input::IsKeyPressed(VirtualKey::D))
			pivot += right * (cameraSpeed * delta * 0.001f);
		if (Input::IsKeyPressed(VirtualKey::Space))
			pivot += up * (cameraSpeed * delta * 0.001f);
		if (Input::IsKeyPressed(VirtualKey::LeftShift))
			pivot -= up * (cameraSpeed * delta * 0.001f);

		if (!Input::IsKeyPressed(VirtualKey::MiddleMouseButton))
			return;

		int x, y;
		window->GetRelativeCursorPosition(x, y);

		sumX = sumX + x * delta;
		sumY = sumY + y * delta;
	}
};

class ColoringTile : public Object
{
public:
	Material* colorMaterial;
	std::chrono::steady_clock::time_point timeOfClick = std::chrono::high_resolution_clock::now();
	int indexX, indexY;

	void Start() override
	{
		transform.position = glm::vec3(-indexX, 0, indexY);
		transform.scale = glm::vec3(0.5f);
	}

	void Update(float delta) override
	{
		if (Renderer::selectedHandle != handle)
		{
			transform.position.y = 0;
			return;
		}
		else
			transform.position.y = 0.5f;
		if (Renderer::selectedHandle == handle && Input::IsKeyPressed(VirtualKey::LeftMouseButton) && std::chrono::duration<float, std::chrono::milliseconds::period>(std::chrono::high_resolution_clock::now() - timeOfClick).count() > 100)
		{
			timeOfClick = std::chrono::high_resolution_clock::now();
			if (meshes[0].materialIndex == 0)
				meshes[0].SetMaterial(*colorMaterial);
			else
				meshes[0].ResetMaterial();
		}
	}
};

class TestScene : public Scene
{
	Material colorMaterial;

	std::vector<Object*> spheres;
	void Start() override
	{
		/*colorMaterial = { new Texture(GetVulkanCreationObjects(), "textures/red.png") };
		colorMaterial.roughness = new Texture(GetVulkanCreationObjects(), "textures/white.png");
		Object* baseObject = AddCustomObject<ColoringTile>("stdObj/cube.obj", OBJECT_IMPORT_EXTERNAL);
		baseObject->AwaitGeneration();
		baseObject->GetScript<ColoringTile*>()->colorMaterial = &colorMaterial;

		for (int i = 0; i < 5; i++)
		{
			for (int j = 0; j < 5; j++)
			{
				Object* ptr = DuplicateCustomObject<ColoringTile>(baseObject, "tile" + std::to_string(i * 5 + j));
				ptr->GetScript<ColoringTile*>()->colorMaterial = &colorMaterial;
				ptr->GetScript<ColoringTile*>()->indexX = i - 2;
				ptr->GetScript<ColoringTile*>()->indexY = j - 2;
				ptr->Start();
			}
		}
		baseObject->state = OBJECT_STATE_DISABLED;

		Material lightMaterial{};
		lightMaterial.isLight = true;
		AddStaticObject(GenericLoader::LoadObjectFile("stdObj/light.obj"))->meshes[0].SetMaterial(lightMaterial);
		
		this->camera = new TestCamera();*/


		Object* objPtr = AddStaticObject(GenericLoader::LoadObjectFile("stdObj/sphere.obj"));
		Material knifeMaterial = { new Texture(GetVulkanCreationObjects(), "textures/glockAlbedo.png"), new Texture(GetVulkanCreationObjects(), "textures/glockNormal.png") };
		knifeMaterial.AwaitGeneration();
		objPtr->meshes[0].SetMaterial(knifeMaterial);
		objPtr->transform.position.y += 20;
		objPtr->AwaitGeneration();
		Sphere sphere = Sphere(1);
		objPtr->AddRigidBody(RIGID_BODY_DYNAMIC, sphere);
		spheres.push_back(objPtr);
		for (int i = 0; i < 32; i++)
		{
			Object* ptr = DuplicateStaticObject(objPtr, "sphere" + std::to_string(i));
			ptr->transform.position.y += 10 * (i + 1);
			ptr->rigid.ForcePosition(ptr->transform);
			spheres.push_back(ptr);
		}
		
		Object* floorPtr = AddStaticObject(GenericLoader::LoadObjectFile("stdObj/plane.obj"));
		floorPtr->transform.position = glm::vec3(0, -1.1, 0);
		floorPtr->AwaitGeneration();
		Box box = Box(floorPtr->meshes[0].extents);
		floorPtr->AddRigidBody(RIGID_BODY_STATIC, box);

		Object* ramp = AddStaticObject(GenericLoader::LoadObjectFile("stdObj/ramp.obj"));
		ramp->AwaitGeneration();
		ramp->transform.rotation.x = 45;
		box = Box(ramp->meshes[0].extents);
		ramp->AddRigidBody(RIGID_BODY_STATIC, box);
		ramp->rigid.ForcePosition(ramp->transform);

		ramp = DuplicateStaticObject(ramp, "ramp2");
		ramp->transform.rotation.y += 45;
		ramp->rigid.ForcePosition(ramp->transform);
	}

	void Update(float delta) override
	{
		if (!Input::IsKeyPressed(VirtualKey::R) || Input::IsKeyPressed(VirtualKey::P))
			return;

		for (int i = 0; i < spheres.size(); i++)
		{
			spheres[i]->transform.position = glm::vec3(0, 20 + 5 * (i + 1), 0);
			spheres[i]->transform.rotation = glm::vec3(0);
			spheres[i]->rigid.ForcePosition(spheres[i]->transform);
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
	createInfo.useEditor = true;
	createInfo.playIntro = false;
#endif

	HalesiaInstance::GenerateHalesiaInstance(instance, createInfo);

	instance.Run();

	return EXIT_SUCCESS;
}