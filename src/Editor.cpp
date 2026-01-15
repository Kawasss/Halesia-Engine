module;

#include <Windows.h>
#include <cstdint>

#include "core/Object.h"
#include "core/Transform.h"
#include "core/MeshObject.h"
#include "core/Rigid3DObject.h"
#include "core/LightObject.h"
#include "core/ScriptObject.h"

#include "renderer/gui.h"
#include "renderer/RenderPipeline.h"
#include "renderer/GraphicsPipeline.h"
#include "renderer/Grid.h"

#include "io/SceneLoader.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui-1.91.7/implot.h>
#include <imgui-1.91.7/imgui.h>
#include <imgui-1.91.7/ImGuizmo.h>
#include <imgui-1.91.7/misc/cpp/imgui_stdlib.h>

#include <glm/gtc/type_ptr.hpp>

#include "glm.h"

module Core.Editor;

import std;

import Core.EditorProject;

import HalesiaEngine;

import System.Input;
import System.FileDialog;
import System.Window;
import System;

namespace fs = std::filesystem;

constexpr float BAR_WIDTH = 0.15f;
constexpr float LOWER_BAR_HEIGHT = 0.2f;

constexpr float VIEWPORT_WIDTH = 1.0f - BAR_WIDTH * 2.0f;
constexpr float VIEWPORT_HEIGHT = 1.0f - LOWER_BAR_HEIGHT;

constexpr std::string_view SUPPORTED_FILES = "*.obj;*.glb;*.gltf;*.fbx;*.stl;*.dat;";

struct MaterialVisitor
{
	MaterialVisitor(int idx) : index(idx) {}

	void operator()(const MaterialCreationData& data)
	{
		Mesh::InsertMaterial(index, Material::Create(data));
	}

	void operator()(const MaterialCreateInfo& data)
	{
		Mesh::InsertMaterial(index, Material::Create(data));
	}

	int index;
};

void Editor::ProgressBar::Progress(float add)
{
	progress.fetch_add(add);
}

void Editor::ProgressBar::Start()
{
	progress.store(0.0f);
	isRunning.store(true);
}

void Editor::ProgressBar::Stop()
{
	isRunning.store(false);
}

float Editor::ProgressBar::GetProgress()
{
	return progress.load();
}

bool Editor::ProgressBar::IsRunning()
{
	return isRunning.load();
}

EditorCamera::EditorCamera(Window* pWindow) : CameraObject()
{
	window = pWindow;
}

void EditorCamera::Update(float delta)
{
	int viewportWidth  = window->GetWidth()  * VIEWPORT_WIDTH;
	int viewportHeight = window->GetHeight() * VIEWPORT_HEIGHT;

	int viewportX = window->GetWidth()  * BAR_WIDTH;
	int viewportY = window->GetHeight() * LOWER_BAR_HEIGHT;

	int mouseX = 0;
	int mouseY = 0;

	window->GetAbsoluteCursorPosition(mouseX, mouseY);

	bool isInViewport    = (mouseX > viewportX && mouseX < (viewportX + viewportWidth)) && (mouseY > viewportY && mouseY < (viewportY + viewportHeight));
	bool buttonIsPressed = Input::IsKeyPressed(VirtualKey::RightMouseButton);

	if (isInViewport && buttonIsPressed)
		active = true;

	else if (!buttonIsPressed)
		active = false;

	if (active)
		MovementLogic(delta);
	else
		prevX = prevY = -1;
}

void EditorCamera::MovementLogic(float delta)
{
	static constexpr float SENSITIVITY = 0.1f;

	static constexpr glm::vec3 VEC3_UP = glm::vec3(0, 1, 0);
	static constexpr glm::vec3 VEC3_LEFT = glm::vec3(1, 0, 0);

	const glm::vec3 front = transform.GetForward();
	const glm::vec3 up = transform.GetUp();
	const glm::vec3 right = transform.GetRight();

	int wheelRotation = window->GetWheelRotation();

	transform.position += front * static_cast<float>(wheelRotation) * delta * 0.00005f;

	if (Input::IsKeyPressed(VirtualKey::W))
		transform.position += front * (delta * 0.001f);
	if (Input::IsKeyPressed(VirtualKey::S))
		transform.position -= front * (delta * 0.001f);
	if (Input::IsKeyPressed(VirtualKey::A))
		transform.position -= right * (delta * 0.001f);
	if (Input::IsKeyPressed(VirtualKey::D))
		transform.position += right * (delta * 0.001f);
	if (Input::IsKeyPressed(VirtualKey::Space))
		transform.position += up * (delta * 0.001f);
	if (Input::IsKeyPressed(VirtualKey::LeftShift))
		transform.position -= up * (delta * 0.001f);

	int x = 0, y = 0;
	window->GetAbsoluteCursorPosition(x, y);

	if (prevX == -1)
		prevX = x;
	if (prevY == -1)
		prevY = y;

	rotation.x -= (x - prevX) * SENSITIVITY;
	rotation.y -= (y - prevY) * SENSITIVITY;

	rotation.y = std::clamp(rotation.y, -89.0f, 89.0f);
	
	glm::quat rotX = glm::angleAxis(glm::radians(rotation.x), VEC3_UP);
	glm::quat rotY = glm::angleAxis(glm::radians(rotation.y), VEC3_LEFT);

	transform.rotation = rotX * rotY;

	prevX = x;
	prevY = y;
}

void Editor::Start()
{
	EngineCore& core = HalesiaEngine::GetInstance()->GetEngineCore();

	renderer = core.renderer;
	window = core.window;

	//boundingVolumePipeline = renderer->AddRenderPipeline<BoundingVolumePipeline>("boundingVolume");
	gridPipeline = renderer->AddRenderPipeline<GridPipeline>("grid");

	SetActiveCamera(new EditorCamera(core.window));

	renderer->SetViewportOffsets({ BAR_WIDTH, 0.0f });
	renderer->SetViewportModifiers({ VIEWPORT_WIDTH, VIEWPORT_HEIGHT });

	
	InitializeProject();
	LoadProject();
}

void Editor::InitializeProject()
{
	do
	{
		FileDialog::Filter filter{};
		filter.description = "project file";
		filter.fileType = "*.hproj;";

		std::string projectLocation = FileDialog::RequestFileSaveLocation(filter);
		std::expected<EditorProject, EditorProject::Result> exProject = fs::exists(projectLocation) ? EditorProject::LoadFromFile(projectLocation) : EditorProject::CreateInFile(projectLocation);

		if (!exProject.has_value())
		{
			Console::WriteLine("Failed to load an editor project, retrying... (error: {})", Console::Severity::Error, static_cast<int>(exProject.error()));
		}
		else
		{
			project = *exProject;
		}

	} while (!project.IsValid());
}

void Editor::Update(float delta)
{
	if (addObject)
	{
		ObjectCreationData data{ "new object" };
		Object* obj = AddObject(data);

		addObject = false;
	}

	if (pObjectToCopy != nullptr)
	{
		pObjectToCopy->CreateShallowCopy();
		pObjectToCopy = nullptr;
	}
}

void Editor::UpdateGUI(float delta)
{
	HalesiaEngine* engine = HalesiaEngine::GetInstance();
	EngineCore& core = engine->GetEngineCore();

	width = core.window->GetWidth();
	height = core.window->GetHeight();

	ShowUI();
}

void Editor::ShowGizmo()
{
	ImGuizmo::OPERATION mode = ImGuizmo::TRANSLATE;
	switch (gizmoMode)
	{
	case GizmoMode::Translate: break;
	case GizmoMode::Rotate: mode = ImGuizmo::ROTATE; break;
	case GizmoMode::Scale:  mode = ImGuizmo::SCALE;  break;
	}

	Object* obj = selectedObj;

	if (obj == nullptr)
		return;

	const Transform& camTrans = camera->transform;
	glm::mat4 view = glm::lookAtRH(camTrans.GetGlobalPosition(), camTrans.GetGlobalPosition() + camTrans.GetForward(), camTrans.GetUp());
	glm::mat4 proj = glm::perspectiveRH(glm::radians(camera->fov), renderer->GetInternalWidth() / (float)renderer->GetInternalHeight(), camera->zNear, camera->zFar);

	glm::vec2 mod = renderer->GetViewportModifier();
	glm::vec2 off = renderer->GetViewportOffset();

	glm::mat4 model = obj->transform.GetModelMatrix();
	glm::mat4 delta = glm::mat4();

	bool useSnap = Input::IsKeyPressed(VirtualKey::LeftControl);
	glm::vec3 snaps = glm::vec3(1);

	ImGuizmo::SetRect(off.x * width, off.y * height, mod.x * width, mod.y * height);
	ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), mode, ImGuizmo::WORLD, glm::value_ptr(model), glm::value_ptr(delta), useSnap ? glm::value_ptr(snaps) : NULL);

	glm::vec3 position = glm::vec3();
	glm::vec3 scale = glm::vec3(1);
	glm::quat rotation = glm::quat();

	glm::vec3 skew;
	glm::vec4 pers;
	glm::decompose(delta, scale, rotation, position, skew, pers);

	Transform& trans = obj->transform;
	trans.position += position;
	trans.scale += scale - glm::vec3(1);
	trans.rotation *= rotation;
}

void Editor::MainThreadUpdate(float delta)
{
	if (save)
	{
		BuildProject();
		save = false;
	}

	if (!queuedMeshChange.isApplied)
		ApplyQueuedMeshChange();

	if (queuedMeshChange.path.empty())
	{
		queuedMeshChange.isApplied = true;
	}

	if (loadFile)
	{
		fs::path src = GetFile("file", SUPPORTED_FILES.data());
		if (!src.empty())
		{
			LoadFile(src);
		}
		loadFile = false;
	}
}

void Editor::ApplyQueuedMeshChange()
{
	queuedMeshChange.path = GetFile("Mesh file", SUPPORTED_FILES.data());
	Object* obj = queuedMeshChange.object;

	if (obj != nullptr)
	{
		MeshCreationData creationData = assetImport::LoadFirstMesh(queuedMeshChange.path);

		if (obj->IsType(Object::InheritType::Mesh))
		{
			MeshObject* ptr = dynamic_cast<MeshObject*>(obj);

			ptr->mesh.Destroy();
			ptr->mesh.Create(creationData);
			ptr->mesh.SetMaterial(ptr->mesh.GetMaterial());
		}
	}

	queuedMeshChange.isApplied = true;
}

void Editor::ShowDefaultRightClick()
{
	static bool gizmoButton = false;
	static ImVec2 rightClickPos{};
	static ImVec2 endPosition{};

	bool isPressed = Input::IsKeyPressed(VirtualKey::RightMouseButton);

	if (!gizmoButton && isPressed)
	{
		gizmoButton = true;
		rightClickPos = ImGui::GetMousePos();
	}

	else if (gizmoButton && !isPressed)
	{
		gizmoButton = false;
		endPosition = ImGui::GetMousePos();
	}

	if (!gizmoButton && rightClickPos != endPosition)
		return;

	if (!ImGui::BeginPopupContextVoid("##default_right_click"))
		return;

	selectionData.show = selectionData.show || ImGui::MenuItem("Add object");

	ImGui::SeparatorText("Gizmo");

	if (ImGui::MenuItem("Translate", nullptr, gizmoMode == GizmoMode::Translate)) 
		gizmoMode = GizmoMode::Translate;
	if (ImGui::MenuItem("Rotate", nullptr, gizmoMode == GizmoMode::Rotate))   
		gizmoMode = GizmoMode::Rotate;
	if (ImGui::MenuItem("Scale", nullptr, gizmoMode == GizmoMode::Scale))    
		gizmoMode = GizmoMode::Scale;

	ImGui::EndPopup();
}

void Editor::ShowUI()
{
	if (showUI)
	{
		ShowLowerBar();

		StartRightBar();

		ShowRenderPipelines();
		ShowMaterials();
		ShowSelectedObject();

		EndRightBar();

		ShowSideBars();

		if (selectionData.show)
			ShowAddObjectWindow();

		if (showVram)
			ShowVRAM();
	}

	ShowMenuBar();
	ShowGizmo();
	ShowDefaultRightClick();

	if (progressBar.IsRunning())
		ShowProgressBar();
}

void Editor::ShowSideBars()
{
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.03f, 0.03f, 0.03f, 1));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.03f, 0.03f, 0.03f, 1));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.02f, 0.02f, 0.02f, 1));

	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 5);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

	ImGuiStyle& style = ImGui::GetStyle();

	ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetFrameHeight() + style.FramePadding.y));
	ImGui::SetNextWindowSize(ImVec2(width * BAR_WIDTH, height - ImGui::GetFrameHeight() - style.FramePadding.y));

	ImGui::Begin("scene graph", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
	ImGui::BeginChild(1);

	for (int i = 0; i < allObjects.size(); i++)
	{
		ShowObjectWithChildren(allObjects[i]);
	}
	ImGui::EndChild();
	ImGui::End();

	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);
}

void Editor::ShowObjectWithChildren(Object* object)
{
	std::string rightClickName = "##r_" + std::to_string((uint64_t)object);

	ImVec2 first = ImGui::GetWindowPos() + ImGui::GetCursorPos();

	std::string nodeName = "##n_" + object->name;
	bool success = false; // default state is that the object has no children
	bool noChildren = !object->GetChildren().empty();

	if (noChildren)
	{
		success = ImGui::TreeNode(nodeName.c_str());
		ImGui::SameLine();
	}

	if (ImGui::Selectable(object->name.c_str()))
		selectedObj = object;
	
	ImVec2 end = ImGui::GetWindowPos() + ImGui::GetCursorPos() + ImVec2(ImGui::GetWindowWidth(), 0.0f);
	ImVec2 mousePos = ImGui::GetMousePos();

	if (ImGui::BeginPopupContextItem(nodeName.c_str(), ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoReopen))
	{
		inSelectPopup = true;
		if (ImGui::MenuItem("add object"))
		{
			selectionData.parent = object;
			selectionData.show = true;
		}

		if (ImGui::MenuItem("copy"))
		{
			pObjectToCopy = object;
		}

		if (ImGui::MenuItem("delete"))
		{
			UIFree(object);
		}

		ImGui::EndPopup();
	}

	if (!success)
		return;

	const std::vector<Object*>& children = object->GetChildren();
	for (Object* child : children)
	{
		ShowObjectWithChildren(child);
	}
	ImGui::TreePop();
}

Editor::AdditionalMaterialData::AdditionalMaterialData(const MaterialCreateInfo& createInfo)
{
	if (!createInfo.albedo.empty())
		albedoSource = createInfo.albedo;
	if (!createInfo.normal.empty())
		normalSource = createInfo.normal;
	if (!createInfo.metallic.empty())
		metallicSource = createInfo.metallic;
	if (!createInfo.roughness.empty())
		roughnessSource = createInfo.roughness;
	if (!createInfo.ambientOcclusion.empty())
		ambOcclSource = createInfo.ambientOcclusion;
}

void Editor::ShowMaterials()
{
	static MaterialCreateInfo createInfo{};

	if (!ImGui::CollapsingHeader("materials"))
		return;

	if (ImGui::Button("Add material"))
		showMaterialCreateWindow = true;

	if (showMaterialCreateWindow)
	{
		ImGui::Text("Albedo:    ");
		ImGui::SameLine();
		if (ImGui::Button(createInfo.albedo.empty() ? "Set path ##albedo_path" : createInfo.albedo.c_str()))
			createInfo.albedo = GetFile("Image file", "*.png;*.jpg;");

		ImGui::Text("Normal:    ");
		ImGui::SameLine();
		if (ImGui::Button(createInfo.normal.empty() ? "Set path ##normal_path" : createInfo.normal.c_str()))
			createInfo.normal = GetFile("Image file", "*.png;*.jpg;");

		ImGui::Text("Roughness: ");
		ImGui::SameLine();
		if (ImGui::Button(createInfo.roughness.empty() ? "Set path ##roughness_path" : createInfo.roughness.c_str()))
			createInfo.roughness = GetFile("Image file", "*.png;*.jpg;");

		ImGui::Text("Metallic:  ");
		ImGui::SameLine();
		if (ImGui::Button(createInfo.metallic.empty() ? "Set path ##metallic_path" : createInfo.metallic.c_str()))
			createInfo.metallic = GetFile("Image file", "*.png;*.jpg;");

		ImGui::Text("Amb. occl.:");
		ImGui::SameLine();
		if (ImGui::Button(createInfo.ambientOcclusion.empty() ? "Set path ##ambient_occlusion_path" : createInfo.ambientOcclusion.c_str()))
			createInfo.ambientOcclusion = GetFile("Image file", "*.png;*.jpg;");
		
		if (ImGui::Button("create material"))
		{
			Handle handle = Mesh::AddMaterial(Material::Create(createInfo));

			materialToData[handle] = AdditionalMaterialData(createInfo);

			createInfo.albedo.clear();
			createInfo.normal.clear();
			createInfo.roughness.clear();
			createInfo.metallic.clear();
			createInfo.ambientOcclusion.clear();

			showMaterialCreateWindow = false;
		}
	}

	for (const Material& mat : Mesh::materials)
	{
		std::string handleString = std::to_string(mat.handle);
		std::string treeLabel = "material " + handleString;
		if (!ImGui::TreeNode(treeLabel.c_str()))
			continue;

		if (materialToData.contains(mat.handle))
		{
			AdditionalMaterialData data = materialToData[mat.handle];

			ImGui::Text("albedo:     %s\n", data.albedoSource.c_str());
			if (data.albedoSource != "d_albedo")
			{
				if (ImGui::Button(("open##" + handleString + "_al").c_str()))
					sys::OpenFile(data.albedoSource);
			}

			ImGui::Text("normal:     %s\n", data.normalSource.c_str());
			if (data.normalSource != "d_normal")
			{
				if (ImGui::Button(("open##" + handleString + "_no").c_str()))
					sys::OpenFile(data.normalSource);
			}

			ImGui::Text("metallic:   %s\n", data.metallicSource.c_str());
			if (data.metallicSource != "d_metallic")
			{
				if (ImGui::Button(("open##" + handleString + "_me").c_str()))
					sys::OpenFile(data.metallicSource);
			}

			ImGui::Text("roughness:  %s\n", data.roughnessSource.c_str());
			if (data.roughnessSource != "d_roughness")
			{
				if (ImGui::Button(("open##" + handleString + "_ro").c_str()))
					sys::OpenFile(data.roughnessSource);
			}

			ImGui::Text("amb. occl.: %s\n\n", data.ambOcclSource.c_str());
			if (data.ambOcclSource != "d_amb_occl")
			{
				if (ImGui::Button(("open##" + handleString + "_am").c_str()))
					sys::OpenFile(data.ambOcclSource);
			}
		}
		else
		{
			ImGui::Text
			(
				"albedo:     d_albedo\n"
				"normal:     d_normal\n"
				"metallic:   d_metallic\n"
				"roughness:  d_rougness\n"
				"amb. occl.: d_amb_occl\n\n"
			);
		}
		ImGui::TreePop();
	}
}

void Editor::ShowMenuBar() // add renderer variables here like taa sample count
{
	ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.02f, 0.02f, 0.02f, 1));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.02f, 0.02f, 0.02f, 1));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 5);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 2);

	ImGui::BeginMainMenuBar();

	if (ImGui::BeginMenu("file"))
	{
		ShowFileSubmenu();
	}
	if (ImGui::BeginMenu("view"))
	{
		ShowViewSubmenu();
	}
	if (ImGui::BeginMenu("renderer"))
	{
		ShowRendererSubmenu();
	}
	if (ImGui::BeginMenu("Add"))
	{
		ShowAddSubmenu();
	}
	if (ImGui::BeginMenu("misc."))
	{
		ShowMiscSubmenu();
	}

	ImGui::EndMainMenuBar();

	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);
}

void Editor::ShowFileSubmenu()
{
	if (ImGui::MenuItem("Load file")) loadFile = true;
	if (ImGui::MenuItem("Save file")) save = true;

	ImGui::Separator();

	if (ImGui::MenuItem("Clear scene"))
		DestroyCurrentScene();

	ImGui::Separator();

	if (ImGui::MenuItem("Exit"))
		HalesiaEngine::Exit();
	ImGui::EndMenu();
}

void Editor::ShowViewSubmenu()
{
	HalesiaEngine* engine = HalesiaEngine::GetInstance();

	if (ImGui::MenuItem("window data"))     
		engine->showWindowData = !engine->showWindowData;

	ImGui::EndMenu();
}

void Editor::ShowRendererSubmenu()
{
	if (ImGui::BeginMenu("render mode"))
	{
		int count = static_cast<int>(RenderMode::ModeCount);
		for (int i = 0; i < count; i++)
		{
			RenderMode mode = static_cast<RenderMode>(i);
			std::string_view text = RenderModeToString(mode);

			if (ImGui::MenuItem(text.data()))
				renderer->SetRenderMode(mode);
		}
		ImGui::EndMenu();
	}
	ImGui::Separator();
	if (ImGui::MenuItem("show collision boxes")) Renderer::shouldRenderCollisionBoxes = !Renderer::shouldRenderCollisionBoxes;
	ImGui::Separator();
	ImGui::MenuItem("view statistics");
	if (ImGui::MenuItem("show VRAM"))
	{
		showVram = !showVram;
	}
	ImGui::EndMenu();
}

void Editor::ShowAddSubmenu()
{
	if (ImGui::MenuItem("Object"))
		selectionData.show = true;
	ImGui::EndMenu();
}

void Editor::ShowMiscSubmenu()
{
	if (ImGui::MenuItem("enable/disable UI"))
	{
		showUI = !showUI;
		if (showUI)
		{
			renderer->SetViewportOffsets({ BAR_WIDTH, 0.0f });
			renderer->SetViewportModifiers({ VIEWPORT_WIDTH, VIEWPORT_HEIGHT });
		}
		else
		{
			renderer->SetViewportOffsets({ 0.0f, 0.0f });
			renderer->SetViewportModifiers({ 1.0f, 1.0f });
		}
	}
	ImGui::EndMenu();
}

void Editor::ShowVRAM()
{
	ImGui::Begin("VRAM");

	std::vector<vvm::DbgMemoryBlock> blocks = vvm::DbgGetMemoryBlocks();

	ImVec2 windowSize = ImGui::GetWindowSize();
	float blockHeight = ImGui::GetTextLineHeight();

	for (int i = 0; i < blocks.size(); i++)
	{
		vvm::DbgMemoryBlock& block = blocks[i];
		bool blockIsAtleast1kb = block.used >= 1024;

		std::string blockName = std::format("mem_block {}%% ({} kb, {} {} used):", block.used / block.size * 100, block.size / 1024, blockIsAtleast1kb ? block.used / 1024 : block.used, blockIsAtleast1kb ? "kb" : "b");
		ImGui::Text(blockName.c_str());
		ImGui::SameLine();

		ImVec2 cursorPos = ImGui::GetCursorPos();

		ImGui::BeginChild(i + 1, { windowSize.x - cursorPos.x * 2, blockHeight }, 1);

		ImDrawList* drawList = ImGui::GetWindowDrawList();

		ImVec2 blockPos = ImGui::GetWindowPos();
		ImVec2 blockSize = ImGui::GetWindowSize();
		
		float xOffset = 0.0f;
		
		for (const vvm::DbgSegment& segment : block.segments)
		{
			float end = (static_cast<float>(segment.end) / static_cast<float>(block.size)) * blockSize.x;

			ImVec2 startPos = blockPos + ImVec2(xOffset, 0.0f);
			ImVec2 endPos = startPos + ImVec2(end, blockHeight);

			drawList->AddRectFilled(startPos, endPos, IM_COL32(255, 255, 255, 255));

			xOffset += end + 1.0f; // 1 for padding
		}

		ImGui::EndChild();
	}

	ImGui::End();
}

void Editor::StartRightBar()
{
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.03f, 0.03f, 0.03f, 1));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.03f, 0.03f, 0.03f, 1));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.02f, 0.02f, 0.02f, 1));

	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 5);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5);

	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.05f, 0.05f, 0.05f, 1);
	colors[ImGuiCol_Header] = ImVec4(0.06f, 0.06f, 0.06f, 1);
	colors[ImGuiCol_FrameBg] = ImVec4(0.06f, 0.06f, 0.06f, 1);
	colors[ImGuiCol_Button] = ImVec4(0.05f, 0.05f, 0.05f, 1);

	ImGui::SetNextWindowPos(ImVec2(width * (1.0f - BAR_WIDTH), ImGui::GetFrameHeight() + style.FramePadding.y));

	ImGui::SetNextWindowPos(ImVec2(width * (1.0f - BAR_WIDTH), ImGui::GetFrameHeight() + style.FramePadding.y));
	ImGui::SetNextWindowSize(ImVec2(width * BAR_WIDTH, height - ImGui::GetFrameHeight() - style.FramePadding.y));

	ImGui::Begin("components", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);

}

void Editor::EndRightBar()
{
	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor(3);
	ImGui::End();
}

void Editor::ShowProgressBar()
{
	static constexpr float PADDING = 5.0f;

	float percentage = progressBar.GetProgress();

	ImGuiStyle& style = ImGui::GetStyle();

	ImVec2 size(200, 60);
	ImVec2 pos(style.FramePadding);

	pos.y = window->GetHeight() - size.y - pos.y;

	ImGui::SetNextWindowPos(pos);
	ImGui::SetNextWindowSize(size);
	ImGui::Begin("loading...", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);

	ImGui::ProgressBar(percentage);

	ImGui::End();
}

void Editor::ShowSelectedObject()
{
	if (!ImGui::CollapsingHeader("Selected object"))
		return;

	ImGui::BeginChild(2);

	if (selectedObj != nullptr)
	{
		ShowObjectComponents();
	}

	ImGui::EndChild();
}

void Editor::ShowObjectComponents()
{
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed;

	if (ImGui::CollapsingHeader("Metadata", flags))
		ShowObjectData(selectedObj);

	if (ImGui::CollapsingHeader("Transform", flags))
		GUI::ShowObjectTransform(selectedObj->transform);

	if (selectedObj->IsType(Object::InheritType::Rigid3D) && ImGui::CollapsingHeader("rigid body", flags))
	{
		ShowObjectRigidBody(dynamic_cast<Rigid3DObject*>(selectedObj)->rigid);
	}

	if (selectedObj->IsType(Object::InheritType::Mesh) && ImGui::CollapsingHeader("mesh", flags))
	{
		ShowObjectMesh(dynamic_cast<MeshObject*>(selectedObj)->mesh);
	}

	if (selectedObj->IsType(Object::InheritType::Light) && ImGui::CollapsingHeader("light", flags))
	{
		ShowObjectLight(dynamic_cast<LightObject*>(selectedObj));
	}
	if (selectedObj->IsType(Object::InheritType::Script) && ImGui::CollapsingHeader("script"))
	{
		ShowObjectScript(dynamic_cast<ScriptObject*>(selectedObj));
	}
}

void Editor::ShowObjectScript(ScriptObject* scriptObject)
{
	if (ImGui::Button("Set script"))
	{
		std::string file = GetFile("lua", "*.lua");
		if (!file.empty())
			scriptObject->SetScriptFile(file);
	}

	if (ImGui::Button("pause"))
		scriptObject->pause = !scriptObject->pause;

	if (ImGui::Button("Reload"))
	{
		scriptObject->Reload();
	}
}

void Editor::ShowObjectData(Object* pObject)
{
	static std::array<std::string_view, 3> allStates = { "OBJECT_STATE_VISIBLE", "OBJECT_STATE_INVISIBLE", "OBJECT_STATE_DISABLED" };

	std::string_view currentState = ObjectStateToString(pObject->state);
	int currentIndex = -1;

	ImGui::Text("name:   ");
	ImGui::SameLine();

	std::string placeholderName = pObject->name;
	ImGui::InputText("##objectname", &placeholderName);

	EnsureValidName(placeholderName, pObject);
	pObject->name = placeholderName;

	if (pObject->name.empty())
		pObject->name = "NO_NAME";

	ImGui::Text("state:  ");
	ImGui::SameLine();
	GUI::ShowDropdownMenu(allStates, currentState, currentIndex, "##objectstate");
	pObject->state = ObjectStateFromString(currentState);

	ImGui::Text
	(
		"Handle:  %I64u\n"
		"Type:    %s\n"
		"\n"
		"loading: %i\n"
		, pObject->handle, Object::InheritTypeToString(pObject->GetType()).data(), !pObject->FinishedLoading());
}

void Editor::ShowObjectLight(LightObject* light)
{
	static std::array<std::string_view, 3> lightTypes = { "Directional", "Point", "Spot" };

	std::string_view curr = Light::TypeToString(light->lType);
	int currIndex = static_cast<int>(light->lType);
	int nextIndex = currIndex;

	ImGui::Text("type:         ");
	ImGui::SameLine();
	GUI::ShowDropdownMenu(lightTypes, curr, nextIndex, "##selected_object_light");

	if (currIndex != nextIndex)
		light->lType = Light::StringToType(curr);

	float cutoff = glm::degrees(light->cutoff);

	ImGui::Text("cutoff:       ");
	ImGui::SameLine();
	ImGui::InputFloat("##light_cutoff", &cutoff);

	light->cutoff = glm::radians(cutoff);
	float outerCutoff = glm::degrees(light->outerCutoff);

	ImGui::Text("outer cutoff: ");
	ImGui::SameLine();
	ImGui::InputFloat("##light_outer_cutoff", &outerCutoff);

	light->outerCutoff = glm::radians(outerCutoff);

	ImGui::Text("color:        ");
	ImGui::SameLine();
	ImGui::InputFloat3("##color", glm::value_ptr(light->color));
}

void Editor::ShowObjectRigidBody(RigidBody& rigid)
{
	static int rigidIndex = -1;
	static std::array<std::string_view, 3> allRigidTypes = { "RIGID_BODY_DYNAMIC", "RIGID_BODY_STATIC", "RIGID_BODY_KINEMATIC" };

	std::string_view currentRigid = RigidBody::TypeToString(rigid.type);
	
	ImGui::Text("type:   ");
	ImGui::SameLine();
	GUI::ShowDropdownMenu(allRigidTypes, currentRigid, rigidIndex, "##RigidType");

	ShowRigidBodyShape(rigid);

	ImGui::Text("Force:   %.1f, %.1f, %.1f", rigid.queuedUpForce.x, rigid.queuedUpForce.y, rigid.queuedUpForce.z);

	RigidBody::Type currRigidAsType = RigidBody::StringToType(currentRigid);

	if (currRigidAsType != rigid.type)
	{
		rigid.Destroy();
		Object* obj = static_cast<Object*>(rigid.GetUserData());
		rigid = RigidBody(rigid.shape, currRigidAsType, obj->transform.position, obj->transform.rotation);
		rigid.SetUserData(obj);
	}
}

void Editor::ShowRigidBodyShape(RigidBody& rigid)
{
	static int shapeIndex = -1;
	static std::array<std::string_view, 3> allShapeTypes = { "SHAPE_TYPE_BOX", "SHAPE_TYPE_SPHERE", "SHAPE_TYPE_CAPSULE" };

	std::string_view currentShape = Shape::TypeToString(rigid.shape.type);
	glm::vec3 holderExtents = rigid.shape.data;

	ImGui::Text("shape:  ");
	ImGui::SameLine();
	GUI::ShowDropdownMenu(allShapeTypes, currentShape, shapeIndex, "##rigidShapeMenu");

	switch (rigid.shape.type)
	{
	case Shape::Type::Box:
		ImGui::Text("Extents:");
		ImGui::SameLine();
		ImGui::InputFloat3("##extents", glm::value_ptr(holderExtents));
		break;
	case Shape::Type::Capsule:
		ImGui::Text("Height: ");
		ImGui::SameLine();
		ImGui::InputFloat("##height", &holderExtents.y);
		ImGui::Text("radius: ");
		ImGui::SameLine();
		if (ImGui::InputFloat("##radius", &holderExtents.x))
			holderExtents.y += holderExtents.x; // add radius on top of the half height
		break;
	case Shape::Type::Sphere:
		ImGui::Text("radius: ");
		ImGui::SameLine();
		ImGui::InputFloat("##radius", &holderExtents.x);
		break;
	case Shape::Type::None:
	case Shape::Type::Plane:
		break;
	}

	Shape::Type currShapeAsType = Shape::StringToType(currentShape);

	if (currShapeAsType != rigid.shape.type || holderExtents != rigid.shape.data)
	{
		Shape newShape = Shape::GetShapeFromType(currShapeAsType, holderExtents);
		rigid.ChangeShape(newShape);
	}
}

void Editor::ShowObjectMesh(Mesh& mesh)
{
	ImGui::Text
	(
		"Memory:\n"
		"  vertex:   %I64u\n"
		"  d_vertex: %I64u\n"
		"  index:    %I64u\n"
		"BLAS:       %I64u\n"
		"face count: %i\n\n"
		"center:     %.2f, %.2f, %.2f\n"
		"extents:    %.2f, %.2f, %.2f\n\n",
		mesh.vertexMemory, mesh.defaultVertexMemory, mesh.indexMemory, (uint64_t)mesh.BLAS.get(), mesh.faceCount, mesh.center.x, mesh.center.y, mesh.center.z, mesh.extents.x, mesh.extents.y, mesh.extents.z);

	ImGui::Checkbox("cull faces", &mesh.cullBackFaces);

	MeshOptionFlags flags = mesh.GetFlags();
	bool useRayTracing = !(flags & MESH_FLAG_NO_RAY_TRACING);

	ImGui::Checkbox("use in ray-tracing", &useRayTracing);

	if (useRayTracing)
		flags &= ~(MESH_FLAG_NO_RAY_TRACING);
	else
		flags |= MESH_FLAG_NO_RAY_TRACING;

	mesh.SetFlags(flags);

	int index = static_cast<int>(mesh.GetMaterialIndex());

	ImGui::Text("Material: ");
	ImGui::SameLine();
	ImGui::InputInt("##inputmeshmaterialindex", &index);

	if (index != static_cast<int>(mesh.GetMaterialIndex()))
		mesh.SetMaterialIndex(index);

	ImGui::Text("UV scale: ");
	ImGui::SameLine();
	ImGui::InputFloat("##uv_input", &mesh.uvScale);

	if (ImGui::Button("Change mesh"))
		QueueMeshChange(selectedObj);
}

void Editor::ShowAddObjectWindow()
{
	ImGui::Begin("Add object");

	int typeCount = static_cast<int>(Object::InheritType::TypeCount);
	for (int i = 0; i < typeCount; i++)
	{
		Object::InheritType type = static_cast<Object::InheritType>(i);
		std::string_view typeAsString = Object::InheritTypeToString(type);

		if (!ImGui::Selectable(typeAsString.data()))
			continue;

		ObjectCreationData data{};
		data.type = static_cast<ObjectCreationData::Type>(type); // should always convert correctly

		AddObject(data, selectionData.parent);

		selectionData.show = false;
	}

	ImGui::End();
}

void Editor::ShowLowerBar()
{
	ImGuiStyle& style = ImGui::GetStyle();

	ImGui::SetNextWindowPos(ImVec2(width * BAR_WIDTH, height * (1.0f - LOWER_BAR_HEIGHT)));
	ImGui::SetNextWindowSize(ImVec2(width * VIEWPORT_WIDTH, height * LOWER_BAR_HEIGHT));

	ImGui::Begin("##LowerBar", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
	ImGui::BeginChild(3);

	GUI::ShowDevConsoleContent();

	ImGui::EndChild();
	ImGui::End();
}

void Editor::ShowRenderPipelines()
{
	if (!ImGui::CollapsingHeader("Render pipelines"))
		return;

	const std::vector<RenderPipeline*>& pipelines = renderer->GetAllRenderPipelines();
	for (int i = 0; i < pipelines.size(); i++)
	{
		if (i != 0)
			ImGui::Separator();

		RenderPipeline* pipeline = pipelines[i];
		std::string name = renderer->GetRenderPipelineName(pipeline).data();
		std::string msg = name + ": ";

		std::string identifier = "##" + name;
		std::string reload = "Reload" + identifier;

		ImGui::Text(msg.c_str());
		ImGui::SameLine();
		if (ImGui::Button(reload.c_str()))
			pipeline->ReloadShaders(renderer->GetPipelinePayload(renderer->GetActiveCommandBuffer(), camera));

		ImGui::Checkbox(("active" + identifier).c_str(), &pipeline->active);

		std::vector<RenderPipeline::IntVariable> vars = pipeline->GetIntVariables();
		for (const RenderPipeline::IntVariable& var : vars)
		{
			std::string full = std::string(var.name) + ":##" + name;
			ImGui::Text(var.name.data());
			ImGui::SameLine();
			ImGui::InputInt(full.c_str(), var.pValue);
		}
	}
}

void Editor::QueueMeshChange(Object* object)
{
	queuedMeshChange.isApplied = false;
	queuedMeshChange.object = object;
}

void Editor::DestroyCurrentScene()
{
	for (Object* obj : allObjects)
		UIFree(obj);

	for (int i = 1; i < Mesh::materials.size(); i++)
		Mesh::materials[i].Destroy();
	Mesh::materials.resize(1);
}

void Editor::LoadProject()
{
	LoadFile(project.GetBuildFile());
}

std::future<void> fut;
void Editor::LoadFile(const fs::path& path)
{
	Console::WriteLine("started loading {}...", Console::Severity::Debug, path.string());

	fut = std::async([=]()
		{
			SceneLoader loader(path.string());

			progressBar.Start();
			loader.LoadScene();

			const int itemsToLoad = loader.objects.size() + loader.materials.size() + loader.animations.size();
			const float progressStep = 1.0f / itemsToLoad;

			Mesh::materials.resize(loader.materials.size() + 1); // resize the materials ahead of time, even if they arent loaded yet because the material indices of the meshes will be inaccurate

			LoadObjectsParallel(loader.objects, progressStep);
			LoadMaterialsParallel(loader.materials, progressStep);
			LoadAnimationsParallel(loader.animations, progressStep);

			progressBar.Stop();
			
		});
	//fut.get();
}

void Editor::LoadObjectsParallel(const std::span<const ObjectCreationData>& datas, float progressStep)
{
	std::for_each(std::execution::par_unseq, datas.begin(), datas.end(),
		[&](const ObjectCreationData& data)
		{
			AddObject(data);
			progressBar.Progress(progressStep);
		});
}

void Editor::LoadMaterialsParallel(const std::span<const std::variant<MaterialCreationData, MaterialCreateInfo>>& datas, float progressStep)
{
	std::vector<int> indices(datas.size());
	for (int i = 0; i < indices.size(); i++)
		indices[i] = i;

	std::for_each(std::execution::par, indices.begin(), indices.end(),
		[&](int i)
		{
			std::visit(MaterialVisitor(i + 1), datas[i]);
			progressBar.Progress(progressStep);
		});
}

void Editor::LoadAnimationsParallel(const std::span<Animation>& animations, float progressStep) // not actually parallel yet...
{
	AnimationManager* pAnimManager = HalesiaEngine::GetInstance()->GetEngineCore().animationManager;
	for (Animation& anim : animations)
	{
		pAnimManager->AddAnimation(std::move(anim));
		progressBar.Progress(progressStep);
	}
}

void Editor::BuildProject()
{
	project.BuildScene(this);
}

std::string Editor::GetFile(const char* desc, const char* type)
{
	FileDialog::Filter filter{};
	filter.description = desc;
	filter.fileType = type;
	
	return FileDialog::RequestFile(filter);
}

void Editor::UIFree(Object* pObject)
{
	if (selectedObj == pObject)
		selectedObj = nullptr;

	if (rightClickedObj == pObject)
		rightClickedObj = nullptr;

	if (pObjectToCopy == pObject)
		pObjectToCopy = nullptr;

	Free(pObject);
}