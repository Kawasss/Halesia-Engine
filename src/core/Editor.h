#pragma once
#include <string>

#include "Scene.h"

class Window;

class Editor : public Scene
{
public:
	void Start() override;
	void Update(float delta) override;
	void UpdateGUI(float delta) override;

private:
	void ShowMenuBar();
	void ShowSideBars();
	void ShowObjectComponents(int index, Window* window);

	void DestroyCurrentScene();

	std::string GetFile();
	void LoadFile();
	void SaveToFile();

	bool addObject = false;
	bool loadFile = false;
	bool save = false;

	std::string src;

	std::vector<Object*> UIObjects; // the objects in the UI are seperate from the actual objects, because UpdateGUI and Update run at the same time and can clash
};