#pragma once
#include <string>

#include "Scene.h"
#include "Camera.h"

class Window;
class Renderer;
class RigidBody;
class LightObject;
struct Mesh;

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
	enum class GizmoMode
	{
		Translate,
		Rotate,
		Scale,
	};

	struct MeshChangeData
	{
		bool isApplied = true;
		Object* object = nullptr;
		std::string path;
	};

	struct ObjectSelectionData
	{
		Object* parent = nullptr;
		bool show = false;
	};

	void ShowUI();
	void ShowMenuBar();
	void ShowSideBars();
	void ShowLowerBar();
	void ShowObjectComponents();
	void ShowSelectedObject();
	void ShowDefaultRightClick();
	void ShowMaterialWindow();
	void ShowRenderPipelines();

	void StartRightBar();
	void EndRightBar();

	void ShowGizmo();

	void ShowObjectWithChildren(Object* object);
	void ShowObjectRigidBody(RigidBody& rigid);
	void ShowObjectMesh(Mesh& mesh);
	void ShowObjectLight(LightObject* light);

	void ShowAddObjectWindow();

	void DestroyCurrentScene();

	static std::string GetFile(const char* desc, const char* type);
	void LoadFile();
	void SaveToFile();

	void QueueMeshChange(Object* object);

	void UIFree(Object* pObject);

	bool inSelectPopup = false;
	bool addObject = false;
	bool loadFile = false;
	bool save = false;
	bool showUI = true;
	
	int mouseX = 0;
	int mouseY = 0;

	std::string src;

	MeshChangeData queuedMeshChange{};
	ObjectSelectionData selectionData{};
	GizmoMode gizmoMode;
	Object* selectedObj = nullptr;
	Object* rightClickedObj = nullptr;

	Renderer* renderer;

	int width = 0, height = 0;
};