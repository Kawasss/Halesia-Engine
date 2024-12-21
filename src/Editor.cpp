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

#define IMGUI_IMPLEMENTATION
#define IMGUI_DEFINE_MATH_OPERATORS
#define IMSPINNER_DEMO
#include <implot.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <imgui-1.89.8/imgui-1.89.8/imspinner.h>	

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

	ShowSideBars();
	ShowMenuBar();
}

void Editor::MainThreadUpdate(float delta)
{
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

	if (save)
	{
		SaveToFile();
		save = false;
	}

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
		if (UIObjects[i]->HasChildren())
		{
			if (ImGui::Selectable(UIObjects[i]->name.c_str()))
				selectedIndex = i;
			continue;
		}

		if (!ImGui::TreeNode(UIObjects[i]->name.c_str()))
			continue;
		selectedIndex = i;

		const std::vector<Object*>& children = UIObjects[i]->GetChildren();
		for (int j = 0; j < children.size(); j++)
		{
			ImGui::Text(children[j]->name.c_str());
		}
		ImGui::TreePop();
	}
	ImGui::EndChild();
	ImGui::End();

	ShowLowerBar();

	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);

	ShowObjectComponents(selectedIndex);
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
		ImGui::Text("Load object");

		ImGui::Separator();

		if (ImGui::Button("Load file")) loadFile = true;
		if (ImGui::Button("Save file")) save = true;

		ImGui::Separator();

		if (ImGui::Button("Exit"))
			throw std::exception("Exit requested via UI");
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("view"))
	{
		if (ImGui::Button("object metadata")) engine->showObjectData = !engine->showObjectData;
		if (ImGui::Button("window data"))     engine->showWindowData = !engine->showWindowData;
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("renderer"))
	{
		if (ImGui::Button("show albedo"))  RayTracingPipeline::showAlbedo = !RayTracingPipeline::showAlbedo;
		if (ImGui::Button("show normals")) RayTracingPipeline::showNormals = !RayTracingPipeline::showNormals;
		if (ImGui::Button("show unique"))  RayTracingPipeline::showUniquePrimitives = !RayTracingPipeline::showUniquePrimitives;
		ImGui::Separator();
		if (ImGui::Button("show collision boxes")) Renderer::shouldRenderCollisionBoxes = !Renderer::shouldRenderCollisionBoxes;
		ImGui::Separator();
		ImGui::Button("view statistics");
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Add"))
	{
		if (ImGui::Button("Object")) addObject = true;
		ImGui::EndMenu();
	}

	ImGui::EndMainMenuBar();

	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);
}

void Editor::ShowObjectComponents(int index)
{
	static int objectIndex = -1;
	if (objectIndex >= UIObjects.size())
		objectIndex = -1;
	if (index != -1)
		objectIndex = index;

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

	GUI::ShowObjectSelectMenu(UIObjects, objectIndex, "##ObjectComponents");

	ImGui::BeginChild(2);

	if (objectIndex != -1)
	{
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed;

		if (ImGui::CollapsingHeader("Metadata", flags))
			GUI::ShowObjectData(UIObjects[objectIndex]);

		if (ImGui::CollapsingHeader("Transform", flags))
			GUI::ShowObjectTransform(UIObjects[objectIndex]->transform);

		if (ImGui::CollapsingHeader("Rigid body", flags) && UIObjects[objectIndex]->rigid.type != RigidBody::Type::None)
			GUI::ShowObjectRigidBody(UIObjects[objectIndex]->rigid);

		if (ImGui::CollapsingHeader("Meshes", flags) && UIObjects[objectIndex]->mesh.IsValid())
			GUI::ShowObjectMeshes(UIObjects[objectIndex]->mesh);

		if (ImGui::Button("Change mesh"))
			QueueMeshChange(UIObjects[objectIndex]);
	}

	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor(3);

	ImGui::EndChild();
	ImGui::End();
}

void Editor::ShowLowerBar()
{
	HalesiaEngine* engine = HalesiaEngine::GetInstance();
	EngineCore& core = engine->GetEngineCore();

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
	std::async(&HSFWriter::WriteHSFScene, this, src);
}

std::string Editor::GetFile(const char* desc, const char* type)
{
	FileDialog::Filter filter{};
	filter.description = desc;
	filter.fileType = type;
	
	return FileDialog::RequestFile(filter);
}