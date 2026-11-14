module;

#include <imgui-1.91.7/imgui.h>
#include <imgui-1.91.7/misc/cpp/imgui_stdlib.h>

#include <imnodes/imnodes.h>

module Editor.ShaderEditor;

import HalesiaEngine;

import Editor.ShaderNodes;

namespace editor
{
	void ShaderEditor::Start()
	{
		window = HalesiaEngine::GetInstance()->GetEngineCore().window;

		ImNodes::CreateContext(); // it is assumed that the shader editor is always created _after_ the main editor, meaning that the imgui context already exists
		nodes.insert(nodes.begin(), std::make_unique<ConstVec2Node>());
		nodes.insert(nodes.begin(), std::make_unique<ConstFloatNode>());
		nodes.insert(nodes.begin(), std::make_unique<FragmentOutputNode>());
	}

	void ShaderEditor::Update(float delta)
	{
		
	}
	
	void ShaderEditor::UpdateGUI(float delta)
	{
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(ImVec2(static_cast<float>(window->GetWidth()), static_cast<float>(window->GetHeight())));

		ImGui::Begin("shader editor", nullptr, ImGuiWindowFlags_::ImGuiWindowFlags_NoDecoration);

		ImNodes::BeginNodeEditor();

		for (const std::unique_ptr<BasicNode>& node : nodes)
			node->Draw();

		for (const Link& link : links)
			link.Draw();

		ImNodes::EndNodeEditor();

		int start, end;
		if (ImNodes::IsLinkCreated(&start, &end))
			links.emplace(start, end);

		int destroyed;
		if (ImNodes::IsLinkDestroyed(&destroyed))
		{
			auto it = std::find_if(links.begin(), links.end(), [&](const Link& l) { return l.Contains(destroyed); });
			if (it != links.end())
				links.erase(it);
		}

		ImGui::End();
	}

	void ShaderEditor::MainThreadUpdate(float delta)
	{

	}

	void ShaderEditor::Destroy()
	{
		ImNodes::DestroyContext();
	}
}