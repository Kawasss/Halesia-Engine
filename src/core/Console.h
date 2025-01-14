#pragma once
#include <string>
#include <vector>

class Console
{
public:
	struct Color
	{
		Color() = default;
		Color(float val) : r(val), g(val), b(val) {}
		Color(float r, float g, float b) : r(r), g(g), b(b) {}

		float r = 0.0f, g = 0.0f, b = 0.0f;
	};

	enum class Severity
	{
		Normal,  // white
		Warning, // yellow
		Error,   // red
		Debug,   // blue
	};
	static std::string SeverityToString(Severity severity);

	enum class Access
	{
		ReadWrite,
		ReadOnly,
		WriteOnly,
	};
	static std::string VariableAccessToString(Access access);

	struct Message
	{
		Message() = default;
		Message(const std::string& str, Severity sev) : text(str), severity(sev) {}

		std::string text;
		Severity severity;
	};

	static std::vector<Message> messages;
	static bool isOpen;

	static void Init();

	static void WriteLine(std::string message, Severity severity = Severity::Normal);
	static void InterpretCommand(std::string_view command);

	static Color GetColorFromMessage(const Message& message);

	template<typename T> 
	static void AddCVar(const std::string& name, T* variable, Access access = Access::ReadWrite);

private:
	enum class DataType
	{
		None,
		Float,
		Int,
		Bool,
	};

	struct CVarInfo
	{
		void* ptr;
		DataType type;
	};

	template<typename T>
	static DataType GetDataType()
	{
		return DataType::None; // default value
	}

	static void AddCVariable(const std::string& name, DataType type, void* ptr);
};

template<> inline Console::DataType Console::GetDataType<float>() { return DataType::Float; }
template<> inline Console::DataType Console::GetDataType<int>()   { return DataType::Int;   }
template<> inline Console::DataType Console::GetDataType<bool>()  { return DataType::Bool;  }

template<typename T> 
void Console::AddCVar(const std::string& name, T* variable, Access access)
{
	DataType type = GetDataType<T>();
	AddCVariable(name, type, reinterpret_cast<void*>(variable));
}