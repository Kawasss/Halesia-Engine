module;

#include <imgui-1.91.7/imgui.h>
#include <imgui-1.91.7/misc/cpp/imgui_stdlib.h>

module Editor.UI;


namespace eui
{
	NamedItem::NamedItem(const std::string_view& str)
	{
		SetName(str);
	}

	void NamedItem::SetName(const std::string_view& str)
	{
		name = str.empty() ? std::format("#{}", static_cast<void*>(this)) : str;
	}

	std::string_view NamedItem::GetName() const
	{
		return name;
	}

	const char* NamedItem::GetUIName() const
	{
		return name.c_str();
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

		ImGui::Begin(GetUIName(), &open);

		UpdateSelf();

		ImGui::End();
	}

	bool Window::IsOpen() const
	{
		return open;
	}
}