#include "core/Editor.h"
#include "core/Object.h"

#include "renderer/gui.h"
#include "renderer/RayTracing.h"

#include "HalesiaEngine.h"

#define IMGUI_IMPLEMENTATION
#define IMGUI_DEFINE_MATH_OPERATORS
#define IMSPINNER_DEMO
#include <implot.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <imgui-1.89.8/imgui-1.89.8/imspinner.h>	

void Editor::Start()
{
	EngineCore& core = HalesiaEngine::GetInstance()->GetEngineCore();

	core.renderer->SetViewportOffsets({ 0.125f, 0 });
	core.renderer->SetViewportModifiers({ 0.75f, 1 }); // doesnt have to be set every frame
}

void Editor::Update(float delta)
{

}

void Editor::UpdateGUI(float delta)
{
	HalesiaEngine* engine = HalesiaEngine::GetInstance();
	EngineCore& core = engine->GetEngineCore();

	ShowSideBars();
	ShowMenuBar();
}

void Editor::ShowSideBars()
{
	HalesiaEngine* engine = HalesiaEngine::GetInstance();
	EngineCore& core = engine->GetEngineCore();

	int selectedIndex = -1;

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.03f, 0.03f, 0.03f, 1));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.03f, 0.03f, 0.03f, 1));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.02f, 0.02f, 0.02f, 1));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 5);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

	ImGuiStyle& style = ImGui::GetStyle();

	ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetFrameHeight() + style.FramePadding.y));
	ImGui::SetNextWindowSize(ImVec2(core.window->GetWidth() / 8, core.window->GetHeight() - ImGui::GetFrameHeight() - style.FramePadding.y));

	ImGui::Begin("scene graph", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
	ImGui::BeginChild(1, ImVec2(0, (core.window->GetHeight() - ImGui::GetFrameHeight() - style.FramePadding.y) / 2));

	for (int i = 0; i < allObjects.size(); i++)
	{
		if (allObjects[i]->HasChildren())
		{
			if (ImGui::Selectable(allObjects[i]->name.c_str()))
				selectedIndex = i;
			continue;
		}

		if (!ImGui::TreeNode(allObjects[i]->name.c_str()))
			continue;
		selectedIndex = i;

		const std::vector<Object*>& children = allObjects[i]->GetChildren();
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

	ShowObjectComponents(selectedIndex, core.window);
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
		if (ImGui::Button("Exit"))
			exit(0);
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
	ImGui::EndMainMenuBar();

	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);
}

void Editor::ShowObjectComponents(int index, Window* window)
{
	static std::string currentItem = "None";
	static int objectIndex = -1;
	if (objectIndex >= allObjects.size())
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

	ImGui::SetNextWindowPos(ImVec2(window->GetWidth() * 7 / 8, ImGui::GetFrameHeight() + style.FramePadding.y));
	ImGui::SetNextWindowSize(ImVec2(window->GetWidth() / 8, window->GetHeight() - ImGui::GetFrameHeight() - style.FramePadding.y));

	std::vector<std::string> items; // not the most optimal way
	for (Object* object : allObjects)
		items.push_back(object->name);

	ImGui::SetNextWindowPos(ImVec2(window->GetWidth() * 7 / 8, ImGui::GetFrameHeight() + style.FramePadding.y));
	ImGui::SetNextWindowSize(ImVec2(window->GetWidth() / 8, window->GetHeight() - ImGui::GetFrameHeight() - style.FramePadding.y));

	ImGui::Begin("components", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);

	GUI::ShowDropdownMenu(items, currentItem, objectIndex, "##ObjectComponents");

	if (objectIndex != -1)
	{
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed;
		if (ImGui::CollapsingHeader("Metadata", flags))
			GUI::ShowObjectData(allObjects[objectIndex]);

		if (ImGui::CollapsingHeader("Transform", flags))
			GUI::ShowObjectTransform(allObjects[objectIndex]->transform);

		if (ImGui::CollapsingHeader("Rigid body", flags) && allObjects[objectIndex]->rigid.type != RIGID_BODY_NONE)
			GUI::ShowObjectRigidBody(allObjects[objectIndex]->rigid);

		if (ImGui::CollapsingHeader("Meshes", flags) && allObjects[objectIndex]->mesh.IsValid())
			GUI::ShowObjectMeshes(allObjects[objectIndex]->mesh);
	}

	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor(3);

	ImGui::End();
}