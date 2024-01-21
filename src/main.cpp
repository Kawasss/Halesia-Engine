#define NOMINMAX
#include "HalesiaEngine.h"
#include "physics/Physics.h" // only for test
#include "io/SceneLoader.h"
#include "Object.h"
#include "Camera.h"
#include "Scene.h"
#include "renderer/Renderer.h"
#include "renderer/RayTracing.h"

class TestScene : public Scene
{
	Material colorMaterial;

	std::vector<Object*> spheres;
	void Start() override
	{
		/*Object* objPtr = AddStaticObject(GenericLoader::LoadObjectFile("stdObj/sphere.fbx"));
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
		
		SceneLoader loader("stdObj/ramp.fbx");
		loader.LoadFBXScene();
		for (ObjectCreationData& data : loader.objects)
		{
			Object* ptr = AddStaticObject(data);
		}*/

		Object* ptr = AddStaticObject(GenericLoader::LoadObjectFile("stdObj/rock.obj"));
		Material rockMat = { new Texture(GetVulkanCreationObjects(), "textures/rockA.jpg"), new Texture(GetVulkanCreationObjects(), "textures/rockN.jpg", true, TEXTURE_FORMAT_UNORM) };
		rockMat.AwaitGeneration();
		ptr->meshes[0].SetMaterial(rockMat);
	}

	void Update(float delta) override
	{
		/*RayHitInfo info;
		
		if (Input::IsKeyPressed(VirtualKey::LeftMouseButton) && Physics::CastRay(camera->position, camera->front, 9999.9f, info))
		{
			if (info.object->rigid.type == RIGID_BODY_DYNAMIC)
				info.object->rigid.AddForce(glm::vec3(0, 1000, 0));
		}

		if (!Input::IsKeyPressed(VirtualKey::R) || Input::IsKeyPressed(VirtualKey::P))
			return;

		for (int i = 0; i < spheres.size(); i++)
		{
			spheres[i]->transform.position = glm::vec3(0, 20 + 5 * (i + 1), 0);
			spheres[i]->transform.rotation = glm::vec3(0);
			spheres[i]->rigid.ForcePosition(spheres[i]->transform);
		}*/
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