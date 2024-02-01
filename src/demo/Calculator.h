#pragma once
#include "../HalesiaEngine.h"
#include "../io/SceneLoader.h"

#include "../core/Object.h"
#include "../core/Camera.h"
#include "../core/Scene.h"

#include "../renderer/Renderer.h"

#include "../Audio.h"

// This demo shows:
//  - Creating static and custom objects
//  - Using the Input class
//  - Using the audio engine
//  - Overriding classes (Camera, Object and Scene)
//  - Creating a parent - child relationship between objects
//  - Creating a material
//  - Adding a mesh to an object
//  - Loading an FBX scene
//  - mouse picking with the renderer

class TestCam : public Camera
{
public:
	void Start() override
	{
		position = glm::vec3(0, 9, -1);
		SetPitch(-90);
		UpdateVectors();
	}

	void Update(Win32Window* window, float delta) {}
};

class Rotator : public Object
{
	void Start() override
	{
		transform.position.y = -3;
	}

	void Update(float delta) override
	{
		if (window == nullptr) return;

		int x, y;
		window->GetRelativeCursorPosition(x, y);

		if (Input::IsKeyPressed(VirtualKey::MiddleMouseButton))
		{
			transform.rotation.z += x * delta * -0.05f;
			transform.rotation.x += y * delta * 0.05f;
		}
	}

public:
	static Win32Window* window;
};
Win32Window* Rotator::window = nullptr;

class Key : public Object
{
	enum Type
	{
		KEY_0 = 1,
		KEY_DOT = 2,
		KEY_ENTER = 3,
		KEY_1 = 4,
		KEY_2 = 5,
		KEY_3 = 6,
		KEY_4 = 8,
		KEY_5 = 9,
		KEY_6 = 10,
		KEY_7 = 12,
		KEY_8 = 13,
		KEY_9 = 14,
		KEY_MULTIPLY, // two multiplies (**) is the same as power of (^)
		KEY_DIVIDE,
		KEY_ADD,
		KEY_MINUS,
		KEY_ANS,
		KEY_PAREN_OPEN,
		KEY_PAREN_CLOSE
	};

	WavSound keyPress;
	bool LMBWasPressed = false;

	void Start() override
	{
		keyPress.load("sounds/press.wav");
	}

	void Update(float delta) override
	{
		bool LMBIsPressed = Input::IsKeyPressed(VirtualKey::LeftMouseButton) && !Input::IsKeyPressed(VirtualKey::MiddleMouseButton);

		if (Renderer::selectedHandle == handle)
		{
			if (LMBIsPressed && !LMBWasPressed)
				Audio::PlayWavSound(keyPress);
			transform.position.y = LMBIsPressed ? 0.2f : 0.5f;
		}
		else
			transform.position.y = 0.6f;

		LMBWasPressed = LMBIsPressed;
	}
};

class CalculatorScene : public Scene
{
	void Start() override
	{
		camera = AddCustomCamera<TestCam>();

		Object* rotator = AddCustomObject<Rotator>(ObjectCreationData{ "rotator" });

		SceneLoader loader{ "stdObj/calculator.fbx" };
		loader.LoadFBXScene();

		for (auto& info : loader.objects)
		{
			Object* obj = nullptr;
			if (info.name == "case")
				obj = AddStaticObject(info);
			else
				obj = AddCustomObject<Key>(info);
			rotator->AddChild(obj);
		}

		MaterialCreateInfo lampInfo{};
		lampInfo.isLight = true;

		Object* lamp = AddStaticObject({ "cube" });
		lamp->AddMesh(GenericLoader::LoadObjectFile("stdObj/cube.obj").meshes);
		Material lampMat = Material::Create(lampInfo);
		lampMat.AwaitGeneration();
		lamp->meshes[0].SetMaterial(lampMat);
		lamp->transform.position = glm::vec3(0, 10, 0);
		lamp->transform.scale = glm::vec3(10, 1, 10);
	}

	void Update(float delta) override
	{
		if (allObjects.empty())
			return;
	}
};
