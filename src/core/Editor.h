#pragma once
#include <string>

#include "Scene.h"
#include "Camera.h"

class Window;

class EditorCamera : public Camera
{
public:
	void Update(Window* window, float delta) override;

private:
	bool active = false;
};

class Editor : public Scene
{
public:
	void Start() override;
	void Update(float delta) override;
	void UpdateGUI(float delta) override;
	void MainThreadUpdate(float delta) override;

private:
	struct MeshChangeData
	{
		bool isApplied = true;
		Object* object = nullptr;
		std::string path;
	};

	void ShowMenuBar();
	void ShowSideBars();
	void ShowObjectComponents(int index, Window* window);

	void DestroyCurrentScene();

	static std::string GetFile(const char* filter);
	void LoadFile();
	void SaveToFile();

	void QueueMeshChange(Object* object);

	bool addObject = false;
	bool loadFile = false;
	bool save = false;

	std::string src;

	std::vector<Object*> UIObjects; // the objects in the UI are seperate from the actual objects, because UpdateGUI and Update run at the same time and can clash

	MeshChangeData queuedMeshChange;
};