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
};