module;

#include <imgui-1.91.7/imgui.h>
#include <imgui-1.91.7/misc/cpp/imgui_stdlib.h>

export module Editor.UI;

import std;

export namespace eui
{
	template<typename T> 
	concept GuiType = std::is_fundamental_v<T>;

	class NamedItem
	{
	public:
		NamedItem() = default;
		NamedItem(const std::string_view& str);

		void SetName(const std::string_view& str);
		std::string_view GetName() const;
		
		bool NameIsInvisible() const;

	private:
		std::string name;

	protected:
		const char* GetUIName() const;
		const char* GetInvisibleUIName() const;
	};

	class Window : public NamedItem
	{
	public:
		Window() = default;
		Window(const std::string_view& str);

		void Open();
		void Close();

		void Update();

		bool IsOpen() const;

	private:
		bool open = false;

	protected:
		virtual void UpdateSelf() {}
	};
	
	class Button : public NamedItem
	{
	public:
		Button(const std::string_view& str);

		void Update();

		bool IsPressed() const;

	private:
		bool val = false;
	};

	template<GuiType T, int n = 1>
	class InputField : public NamedItem
	{
	public:
		static_assert(n > 0);

		InputField(const std::string_view& str) : NamedItem(str) {}

		void Update()
		{
			if (!NameIsInvisible())
			{
				ImGui::Text(GetUIName());
				ImGui::SameLine();
			}
			ImGui::InputScalarN(GetInvisibleUIName(), AsImGuiType(), static_cast<void*>(fields.data()), n);
		}

		T GetValue() const
		{
			
			return fields[0];
		}

		T operator[](std::size_t idx) const
		{
			return fields[idx];
		}

	private:
		static ImGuiDataType AsImGuiType()
		{
			if constexpr (std::is_same_v<T, char>)               return ImGuiDataType_::ImGuiDataType_S8;
			if constexpr (std::is_same_v<T, unsigned char>)      return ImGuiDataType_::ImGuiDataType_U8;
			if constexpr (std::is_same_v<T, short>)              return ImGuiDataType_::ImGuiDataType_S16;
			if constexpr (std::is_same_v<T, unsigned short>)     return ImGuiDataType_::ImGuiDataType_U16;
			if constexpr (std::is_same_v<T, int>)                return ImGuiDataType_::ImGuiDataType_S32;
			if constexpr (std::is_same_v<T, unsigned int>)       return ImGuiDataType_::ImGuiDataType_U32;
			if constexpr (std::is_same_v<T, long long>)          return ImGuiDataType_::ImGuiDataType_S64;
			if constexpr (std::is_same_v<T, unsigned long long>) return ImGuiDataType_::ImGuiDataType_U64;
			if constexpr (std::is_same_v<T, float>)              return ImGuiDataType_::ImGuiDataType_Float;
			if constexpr (std::is_same_v<T, double>)             return ImGuiDataType_::ImGuiDataType_Double;
			return ImGuiDataType_::ImGuiDataType_U8;
		}

		std::array<T, n> fields{};
	};
}