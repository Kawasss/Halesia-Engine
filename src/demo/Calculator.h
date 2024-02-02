#pragma once
#include <string>
#include <set>

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
//  - getting the script attached to an object
//  - Creating a material
//  - Adding a mesh to an object
//  - Loading an FBX scene
//  - mouse picking with the renderer

enum KeyType
{
	KEY_0 = 0,
	KEY_DEL = 1,
	KEY_ANS = 2,
	KEY_ENT = 3,
	KEY_1 = 4,
	KEY_2 = 5,
	KEY_3 = 6,
	KEY_ADD = 7,
	KEY_4 = 8,
	KEY_5 = 9,
	KEY_6 = 10,
	KEY_SUB = 11,
	KEY_7 = 12,
	KEY_8 = 13,
	KEY_9 = 14,
	KEY_LPAREN = 15, // (
	KEY_LEFT = 16,
	KEY_RIGHT = 17,
	KEY_POW = 18, // ^
	KEY_RPAREN = 19, // )
};

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

class OutputChar : public Object
{
	static Material mat;
	void Start() override
	{
		if (mat.albedo != Texture::placeholderAlbedo && mat.albedo != nullptr)
			return;

		MaterialCreateInfo matInfo{};
		matInfo.albedo = "textures/blue.png";
		mat = Material::Create(matInfo);
		mat.AwaitGeneration();
	}

public:
	void Reset()
	{
		for (Object* child : children)
			child->meshes[0].ResetMaterial();
	}

	void SetChildrenStateToKeyType(KeyType type)
	{
		switch (type)
		{
		case KEY_0: ColorPanels({ 5 });
			break;
		case KEY_1: ColorPanels({ 0, 2, 4, 5, 6 });
			break;
		case KEY_2: ColorPanels({ 3, 6 });
			break;
		case KEY_3: ColorPanels({ 2, 6 });
			break;
		case KEY_4: ColorPanels({ 0, 2, 4 });
			break;
		case KEY_5: ColorPanels({ 1, 2 });
			break;
		case KEY_6: ColorPanels({ 1 });
			break;
		case KEY_7: ColorPanels({ 2, 4, 5, 6 });
			break;
		case KEY_8: ColorPanels({ });
			break;
		case KEY_9: ColorPanels({ 2 });
			break;
		case KEY_ANS: ColorPanels({ 4 });
			break;
		case KEY_ADD: ColorPanels({ 0, 4 }); // cant actually make a + char with the calculator, so this'll just create a shape
			break;
		case KEY_SUB: ColorPanels({ 0, 1, 2, 3, 4, 6 });
			break;
		case KEY_POW: ColorPanels({ 2, 3, 4, 5 });
			break;
		case KEY_LPAREN: ColorPanels({ 1, 3, 5 });
			break;
		case KEY_RPAREN: ColorPanels({ 2, 5, 6 });
			break;
		case KEY_LEFT:
			break;
		case KEY_RIGHT:
			break;
		case KEY_ENT:
			break;
		}
	}

	void ColorPanels(const std::set<int>& panelsToAvoid)
	{
		for (int i = 0; i < children.size(); i++)
			if (panelsToAvoid.find(i) == panelsToAvoid.end())
				children[i]->meshes[0].SetMaterial(mat);
	}
};
Material OutputChar::mat{};

class Key : public Object
{
public:
	static std::vector<KeyType> inputs;
	KeyType type;

private:
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
			if (!LMBIsPressed && LMBWasPressed)
			{
				if (type == KEY_DEL && !inputs.empty())
					inputs.pop_back();
				else
					inputs.push_back(type);
			}
		}
		else
			transform.position.y = 0.6f;

		LMBWasPressed = LMBIsPressed;
	}
};
std::vector<KeyType> Key::inputs;

class CalculatorScene : public Scene
{
	std::vector<Object*> outputChars;
	void Start() override
	{
		camera = AddCustomCamera<TestCam>();

		Object* rotator = AddCustomObject<Rotator>(ObjectCreationData{ "rotator" });

		for (int i = 0; i < 4; i++)
			outputChars.push_back(AddCustomObject<OutputChar>(ObjectCreationData{ "output" + std::to_string(i) }));

		SceneLoader loader{ "stdObj/calculator.fbx" };
		loader.LoadFBXScene();

		MaterialCreateInfo matInfo{};
		matInfo.albedo = "textures/red.png";
		Material mat = Material::Create(matInfo);
		mat.AwaitGeneration();
		
		int keyCount = 0;
		std::vector<Key*> objs;
		for (auto& info : loader.objects)
		{
			Object* obj = nullptr;
			if (info.name.substr(0, 3) != "key")
				obj = AddStaticObject(info);
			else
			{
				std::string nameWOExtension = info.name.substr(3, info.name.size() - 7);

				obj = AddCustomObject<Key>(info);
				obj->GetScript<Key>()->type = (KeyType)std::stoi(nameWOExtension);
				objs.push_back(obj->GetScript<Key>());
				keyCount++;
			}

			if (info.name.substr(0, 3) == "led")
				outputChars[info.name.back() - '0']->AddChild(obj);

			if (info.name == "key3.001")
				obj->meshes[0].SetMaterial(mat);
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
		for (int i = 0; i < outputChars.size(); i++)
		{
			outputChars[i]->GetScript<OutputChar>()->Reset();

			int inputIndex = Key::inputs.size() - 4 + i;
			if (inputIndex < 0 || inputIndex >= Key::inputs.size()) continue;
			outputChars[i]->GetScript<OutputChar>()->SetChildrenStateToKeyType(Key::inputs[inputIndex]);
		}
	}
};
