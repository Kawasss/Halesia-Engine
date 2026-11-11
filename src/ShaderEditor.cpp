module;

#include <imgui-1.91.7/imgui.h>
#include <imgui-1.91.7/misc/cpp/imgui_stdlib.h>

#include <imnodes/imnodes.h>

module Editor.ShaderEditor;

namespace editor
{
	void ShaderEditor::Start()
	{
		ImNodes::CreateContext(); // it is assumed that the shader editor is always created _after_ the main editor, meaning that the imgui context already exists
	}

	void ShaderEditor::Update(float delta)
	{

	}

	void ShaderEditor::UpdateGUI(float delta)
	{

	}

	void ShaderEditor::MainThreadUpdate(float delta)
	{

	}

	void ShaderEditor::Destroy()
	{
		ImNodes::DestroyContext();
	}
}