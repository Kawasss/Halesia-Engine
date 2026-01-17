module;

#include <Windows.h>

#include "../io/CreationData.h"

#include "../glm.h"

#include "Object.h"

#include "../renderer/Mesh.h"

export module Core.Editor;

import std;

import System.Window;

import Physics.RigidBody;

import Renderer.BoundingVolumePipeline;
import Renderer.Animation;
import Renderer.Grid;
import Renderer;

import Core.ScriptObject;
import Core.CameraObject;
import Core.LightObject;
import Core.EditorProject;
import Core.Scene;

namespace fs = std::filesystem;

using Handle = uint64_t;

export class EditorCamera : public CameraObject
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

export class Editor : public Scene
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

	struct ProgressBar
	{
		void Progress(float add);

		void Start();
		void Stop();

		float GetProgress();
		bool IsRunning();

	private:
		std::atomic<bool> isRunning;
		std::atomic<float> progress; // 0.0 as 0%, 1.0 as 100%
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

	void ShowFileSubmenu();
	void ShowViewSubmenu();
	void ShowRendererSubmenu();
	void ShowAddSubmenu();
	void ShowMiscSubmenu();

	void StartRightBar();
	void EndRightBar();

	void ShowGizmo();

	void ShowProgressBar();

	void ShowObjectWithChildren(Object* object);
	void ShowObjectRigidBody(RigidBody& rigid);
	void ShowObjectMesh(Mesh& mesh);
	void ShowObjectLight(LightObject* light);
	void ShowObjectScript(ScriptObject* scriptObject);
	void ShowObjectData(Object* pObject);

	void ShowRigidBodyShape(RigidBody& rigid);

	void LoadObjectsParallel(const std::span<const ObjectCreationData>& datas, float progressStep);
	void LoadMaterialsParallel(const std::span<const std::variant<MaterialCreationData, MaterialCreateInfo>>& datas, float progressStep);
	void LoadAnimationsParallel(const std::span<Animation>& animations, float progressStep);

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

	ProgressBar progressBar{};
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