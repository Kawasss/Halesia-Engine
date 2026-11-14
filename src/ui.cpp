module;

#include <imgui-1.91.7/imgui.h>
#include <imgui-1.91.7/misc/cpp/imgui_stdlib.h>

#include <cassert>

module Editor.UI;

namespace eui
{
	NamedItem::NamedItem(const std::string_view& str)
	{
		SetName(str);
	}

	void NamedItem::SetName(const std::string_view& str)
	{
		name = str.empty() ? std::format("####{}", static_cast<void*>(this)) : str;
		if (!name.starts_with("##"))
			name = "##" + name;
	}

	std::string_view NamedItem::GetName() const
	{
		return name;
	}

	const char* NamedItem::GetUIName() const
	{
		assert(!name.empty());
		return name.c_str() + 2;
	}

	const char* NamedItem::GetInvisibleUIName() const
	{
		assert(!name.empty());
		return name.c_str();
	}

	bool NamedItem::NameIsInvisible() const
	{
		return name.starts_with("####");
	}

	Window::Window(const std::string_view& str) : NamedItem(str)
	{
	}

	void Window::Open()
	{
		open = true;
	}
	
	void Window::Close()
	{
		open = false;
	}

	void Window::Update()
	{
		if (!IsOpen())
			return;

		ImGui::Begin(GetUIName(), &open, ImGuiWindowFlags_::ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_::ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_::ImGuiWindowFlags_NoResize);

		UpdateSelf();
		
		ImGui::End();
	}

	bool Window::IsOpen() const
	{
		return open;
	}

	Button::Button(const std::string_view& str) : NamedItem(str)
	{

	}

	void Button::Update()
	{
		val = ImGui::Button(GetUIName());
	}

	bool Button::IsPressed() const
	{
		return val;
	}
}