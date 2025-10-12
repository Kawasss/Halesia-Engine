#pragma once
#include <string>
#include <map>

#include "Scene.h"
#include "CameraObject.h"
#include "EditorProject.h"

class Window;
class Renderer;
class RigidBody;
class LightObject;
class ScriptObject;
class GridPipeline;
class BoundingVolumePipeline;
struct MaterialCreateInfo;
struct Mesh;

using Handle = uint64_t;

class EditorCamera : public CameraObject
{
public:
	EditorCamera(Window* pWindow);

	Window* window = nullptr;

protected:
	void Update(float delta) override;

private:
	void MovementLogic(float delta);

	glm::vec2 rotation;

	int prevX = -1, prevY = -1;
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

	struct AdditionalMaterialData
	{
		AdditionalMaterialData() = default;
		AdditionalMaterialData(const MaterialCreateInfo& createInfo);

		std::string albedoSource = "d_albedo";
		std::string normalSource = "d_normal";
		std::string metallicSource = "d_metallic";
		std::string roughnessSource = "d_roughness";
		std::string ambOcclSource = "d_amb_occl";
	};

	void ShowUI();
	void ShowMenuBar();
	void ShowSideBars();
	void ShowLowerBar();
	void ShowObjectComponents();
	void ShowSelectedObject();
	void ShowDefaultRightClick();
	void ShowRenderPipelines();
	void ShowMaterials();
	void ShowVRAM();

	void StartRightBar();
	void EndRightBar();

	void ShowGizmo();

	void ShowObjectWithChildren(Object* object);
	void ShowObjectRigidBody(RigidBody& rigid);
	void ShowObjectMesh(Mesh& mesh);
	void ShowObjectLight(LightObject* light);
	void ShowObjectScript(ScriptObject* scriptObject);

	void ShowRigidBodyShape(RigidBody& rigid);

	void ShowAddObjectWindow();

	void DestroyCurrentScene();

	void InitializeProject();
	void LoadProject();
	void LoadFile(const fs::path& path);

	static std::string GetFile(const char* desc, const char* type);
	void BuildProject();

	void QueueMeshChange(Object* object);
	void ApplyQueuedMeshChange();

	void UIFree(Object* pObject);

	bool showVram = false;
	bool showMaterialCreateWindow = false;
	bool inSelectPopup = false;
	bool addObject = false;
	bool loadFile = false;
	bool save = false;
	bool showUI = true;

	int mouseX = 0;
	int mouseY = 0;

	std::map<Handle, AdditionalMaterialData> materialToData;

	EditorProject project;

	MeshChangeData queuedMeshChange{};
	ObjectSelectionData selectionData{};
	GizmoMode gizmoMode;
	Object* selectedObj = nullptr;
	Object* rightClickedObj = nullptr;
	Object* pObjectToCopy = nullptr;

	Renderer* renderer;
	Window* window = nullptr;

	BoundingVolumePipeline* boundingVolumePipeline;
	GridPipeline* gridPipeline;

	int width = 0, height = 0;
};