#include "core/Editor.h"
#include "core/Object.h"

#include "renderer/gui.h"

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

	GUI::ShowObjectComponents(allObjects, core.window, selectedIndex);

	GUI::ShowMainMenuBar(engine->showWindowData, engine->showObjectData);
}