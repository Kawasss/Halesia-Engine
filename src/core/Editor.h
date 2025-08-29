#pragma once
#include <string>
#include <map>

#include "Scene.h"
#include "Camera.h"

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

	void StartRightBar();
	void EndRightBar();

	void ShowGizmo();

	void ShowObjectWithChildren(Object* object);
	void ShowObjectRigidBody(RigidBody& rigid);
	void ShowObjectMesh(Mesh& mesh);
	void ShowObjectLight(LightObject* light);
	void ShowObjectScript(ScriptObject* scriptObject);

	void ShowAddObjectWindow();

	void DestroyCurrentScene();

	static std::string GetFile(const char* desc, const char* type);
	void LoadFile();
	void SaveToFile();

	void QueueMeshChange(Object* object);

	void UIFree(Object* pObject);

	bool showMaterialCreateWindow = false;
	bool inSelectPopup = false;
	bool addObject = false;
	bool loadFile = false;
	bool save = false;
	bool showUI = true;
	
	int mouseX = 0;
	int mouseY = 0;

	std::string src;

	std::map<Handle, AdditionalMaterialData> materialToData;

	MeshChangeData queuedMeshChange{};
	ObjectSelectionData selectionData{};
	GizmoMode gizmoMode;
	Object* selectedObj = nullptr;
	Object* rightClickedObj = nullptr;
	Object* pObjectToCopy = nullptr;

	Renderer* renderer;

	BoundingVolumePipeline* boundingVolumePipeline;
	GridPipeline* gridPipeline;

	int width = 0, height = 0;
};