#include <Windows.h>
#include <commdlg.h>

#include "core/Editor.h"
#include "core/Object.h"

#include "renderer/gui.h"
#include "renderer/RayTracing.h"

#include "io/SceneLoader.h"
#include "io/SceneWriter.h"

#include "system/Input.h"
#include "system/FileDialog.h"

#include "HalesiaEngine.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui-1.91.7/implot.h>
#include <imgui-1.91.7/imgui.h>
#include <imgui-1.91.7/ImGuizmo.h>
#include <imgui-1.91.7/misc/cpp/imgui_stdlib.h>

#include <glm/gtc/type_ptr.hpp>

constexpr float BAR_WIDTH = 0.15f;
constexpr float LOWER_BAR_HEIGHT = 0.2f;

constexpr float VIEWPORT_WIDTH = 1.0f - BAR_WIDTH * 2.0f;
constexpr float VIEWPORT_HEIGHT = 1.0f - LOWER_BAR_HEIGHT;

void EditorCamera::Update(Window* window, float delta)
{
	int viewportWidth  = window->GetWidth()  * VIEWPORT_WIDTH;
	int viewportHeight = window->GetHeight() * VIEWPORT_HEIGHT;

	int viewportX = window->GetWidth()  * BAR_WIDTH;
	int viewportY = window->GetHeight() * LOWER_BAR_HEIGHT;

	int mouseX = 0;
	int mouseY = 0;

	window->GetAbsoluteCursorPosition(mouseX, mouseY);

	bool isInViewport    = (mouseX > viewportX && mouseX < (viewportX + viewportWidth)) && (mouseY > viewportY && mouseY < (viewportY + viewportHeight));
	bool buttonIsPressed = Input::IsKeyPressed(VirtualKey::MiddleMouseButton);

	if (isInViewport && buttonIsPressed)
		active = true;

	else if (!buttonIsPressed)
		active = false;

	if (active)
		DefaultUpdate(window, delta);
}

void Editor::Start()
{
	EngineCore& core = HalesiaEngine::GetInstance()->GetEngineCore();

	camera = AddCustomCamera<EditorCamera>();

	core.renderer->SetViewportOffsets({ BAR_WIDTH, 0.0f });
	core.renderer->SetViewportModifiers({ VIEWPORT_WIDTH, VIEWPORT_HEIGHT });

	src = GetFile("Halesia Scene File (.hsf)", "*.hsf;");
	if (src == "")
		return;

	LoadFile();
}

void Editor::Update(float delta)
{
	if (addObject)
	{
		Object* obj = AddObject({ "new object" });

		addObject = false;
	}

	if (allObjects.size() == 0)
	{
		UIObjects.clear();
		return;
	}
		
	if (UIObjects.size() != allObjects.size())
		UIObjects.resize(allObjects.size());

	memcpy(UIObjects.data(), allObjects.data(), sizeof(Object*) * allObjects.size());
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
	HalesiaEngine* engine = HalesiaEngine::GetInstance();
	EngineCore& core = engine->GetEngineCore();

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

	glm::mat4 view = glm::lookAtRH(camera->position, camera->position + camera->front, camera->up);
	glm::mat4 proj = glm::perspectiveRH(camera->fov, (float)width / (float)height, 0.001f, 10000.0f);

	glm::vec2 mod = core.renderer->GetViewportModifier();
	glm::vec2 off = core.renderer->GetViewportOffset();

	glm::mat4 model = obj->transform.GetModelMatrix();

	ImGuizmo::SetRect(off.x * width, off.y * height, mod.x * width, mod.y * height);
	ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), mode, ImGuizmo::WORLD, glm::value_ptr(model));

	glm::vec3 skew;
	glm::vec4 pers;
	glm::decompose(model, obj->transform.scale, obj->transform.rotation, obj->transform.position, skew, pers);
}

void Editor::MainThreadUpdate(float delta)
{
	if (save)
	{
		SaveToFile();
		save = false;
	}

	if (!queuedMeshChange.isApplied)
		queuedMeshChange.path = GetFile("Object file (.obj)", "*.obj;");
	else return;

	if (queuedMeshChange.path.empty())
	{
		queuedMeshChange.isApplied = true;
		return;
	}

	Object* obj = queuedMeshChange.object;
	
	ObjectCreationData creationData = GenericLoader::LoadObjectFile(queuedMeshChange.path);

	uint32_t matIndex = obj->mesh.GetMaterialIndex();

	obj->mesh.Destroy();
	obj->mesh.Create(creationData.mesh);
	obj->mesh.SetMaterial(Mesh::materials[matIndex]);

	queuedMeshChange.isApplied = true;

	if (loadFile)
	{
		src = GetFile("Halesia Scene File (.hsf)", "*.hsf;");
		if (src != "")
		{
			DestroyCurrentScene();
			LoadFile();
		}
		loadFile = false;
	}
}

void Editor::ShowDefaultRightClick()
{
	static bool gizmoButton = false;

	if (!ImGui::BeginPopupContextVoid("##default_right_click"))
		return;

	if (ImGui::MenuItem("Add object"))
		AddObject({ "new object" });

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
	ShowLowerBar();
	ShowSelectedObject();
	ShowSideBars();
	ShowMenuBar();
	ShowGizmo();
	ShowDefaultRightClick();
	ShowMaterialWindow();
}

void Editor::ShowSideBars()
{
	int selectedIndex = -1;

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

	for (int i = 0; i < UIObjects.size(); i++)
	{
		if (ImGui::Selectable(UIObjects[i]->name.c_str()))
		{
			selectedIndex = i;
			selectedObj = UIObjects[i]; 
		}

		std::string rightClickName = "##right_click_menu_object_" + std::to_string((uint64_t)UIObjects[i]);

		if (ImGui::BeginPopupContextItem(rightClickName.c_str(), ImGuiPopupFlags_MouseButtonRight))
		{
			if (ImGui::MenuItem("Duplicate"))
			{
				DuplicateObject(UIObjects[i], "Duplicated object");
			}
			if (ImGui::MenuItem("Delete"))
			{
				UIFree(UIObjects[i]);
				ImGui::EndPopup();
				continue;
			}
			ImGui::EndPopup();
		}

		if (!UIObjects[i]->HasChildren())
			continue;

		if (!ImGui::TreeNode(UIObjects[i]->name.c_str()))
			continue;

		selectedIndex = i;
		selectedObj = UIObjects[selectedIndex];

		const std::vector<Object*>& children = UIObjects[i]->GetChildren();
		for (int j = 0; j < children.size(); j++)
		{
			ImGui::Text(children[j]->name.c_str());
		}
		ImGui::TreePop();
	}
	ImGui::EndChild();
	ImGui::End();

	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);
}

void Editor::ShowMaterialWindow()
{
	static MaterialCreateInfo createInfo{};

	ImGui::SetNextWindowSize(ImVec2(-1, -1));
	ImGui::Begin("Material creator");

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

	if (ImGui::Button("Create material"))
	{
		Mesh::AddMaterial(Material::Create(createInfo));

		createInfo.albedo.clear();
		createInfo.normal.clear();
		createInfo.roughness.clear();
		createInfo.metallic.clear();
		createInfo.ambientOcclusion.clear();
	}
	ImGui::End();
}

void Editor::UIFree(Object* obj)
{
	// first, remove all references to the object inside the editor
	if (obj == selectedObj)
		selectedObj = nullptr;

	auto it = std::find(UIObjects.begin(), UIObjects.end(), obj);
	if (it != UIObjects.end())
		UIObjects.erase(it);

	// then, let the engine take care of removing the object from its structures
	Free(obj);
}

void Editor::ShowMenuBar()
{
	HalesiaEngine* engine = HalesiaEngine::GetInstance();

	ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.02f, 0.02f, 0.02f, 1));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.02f, 0.02f, 0.02f, 1));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 5);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 2);

	ImGui::BeginMainMenuBar();

	if (ImGui::BeginMenu("file"))
	{
		if (ImGui::MenuItem("Load object"))
		{
			std::string file = GetFile("Object file", "*.obj;");
			ObjectCreationData data = GenericLoader::LoadObjectFile(file);

			AddObject(data);
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Load file")) loadFile = true;
		if (ImGui::MenuItem("Save file")) save = true;

		ImGui::Separator();

		if (ImGui::MenuItem("Exit"))
			HalesiaEngine::Exit();
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("view"))
	{
		if (ImGui::MenuItem("object metadata")) engine->showObjectData = !engine->showObjectData;
		if (ImGui::MenuItem("window data"))     engine->showWindowData = !engine->showWindowData;
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("renderer"))
	{
		if (ImGui::MenuItem("show albedo"))  RayTracingRenderPipeline::showAlbedo = !RayTracingRenderPipeline::showAlbedo;
		if (ImGui::MenuItem("show normals")) RayTracingRenderPipeline::showNormals = !RayTracingRenderPipeline::showNormals;
		if (ImGui::MenuItem("show unique"))  RayTracingRenderPipeline::showUniquePrimitives = !RayTracingRenderPipeline::showUniquePrimitives;
		ImGui::Separator();
		if (ImGui::MenuItem("show collision boxes")) Renderer::shouldRenderCollisionBoxes = !Renderer::shouldRenderCollisionBoxes;
		ImGui::Separator();
		ImGui::MenuItem("view statistics");
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Add"))
	{
		if (ImGui::MenuItem("Object")) addObject = true;
		ImGui::EndMenu();
	}

	ImGui::EndMainMenuBar();

	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);
}

void Editor::ShowSelectedObject()
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

	GUI::ShowObjectSelectMenu(UIObjects, selectedObj, "##ObjectComponents");

	ImGui::BeginChild(2);

	if (selectedObj != nullptr)
	{
		ShowObjectComponents();
	}

	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor(3);

	ImGui::EndChild();
	ImGui::End();
}

void Editor::ShowObjectComponents()
{
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed;

	if (ImGui::CollapsingHeader("Metadata", flags))
		GUI::ShowObjectData(selectedObj);

	if (ImGui::CollapsingHeader("Transform", flags))
		GUI::ShowObjectTransform(selectedObj->transform);

	if (ImGui::CollapsingHeader("Rigid body", flags) && selectedObj->rigid.type != RigidBody::Type::None)
		GUI::ShowObjectRigidBody(selectedObj->rigid);

	if (ImGui::CollapsingHeader("Meshes", flags) && selectedObj->mesh.IsValid())
		GUI::ShowObjectMeshes(selectedObj->mesh);

	if (ImGui::Button("Change mesh"))
		QueueMeshChange(selectedObj);
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

void Editor::QueueMeshChange(Object* object)
{
	queuedMeshChange.isApplied = false;
	queuedMeshChange.object = object;
}

void Editor::DestroyCurrentScene()
{
	for (Object* obj : allObjects)
		delete obj;

	for (int i = 1; i < Mesh::materials.size(); i++)
	{
		auto it = Mesh::materials.begin() + i;

		Mesh::materials[i].Destroy();
		Mesh::materials.erase(it);
	}
}

void Editor::LoadFile()
{
	SceneLoader loader(src);
	loader.LoadScene();

	for (const ObjectCreationData& data : loader.objects)
		AddObject(data);

	for (const MaterialCreationData& data : loader.materials)
		Mesh::AddMaterial(Material::Create(data));
}

void Editor::SaveToFile()
{
	HSFWriter::WriteHSFScene(this, src);
}

std::string Editor::GetFile(const char* desc, const char* type)
{
	FileDialog::Filter filter{};
	filter.description = desc;
	filter.fileType = type;
	
	return FileDialog::RequestFile(filter);
}