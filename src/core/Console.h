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
		std::string text;
		Severity severity;
	};

	static std::vector<Message> messages;
	static bool isOpen;

	static void Init();

	static void WriteLine(std::string message, Severity severity = Severity::Normal);
	static void InterpretCommand(std::string command = "");

	static Color GetColorFromMessage(const Message& message);

	template<typename T> 
	static void AddConsoleVariable(std::string variableName, T* variable, Access access = Access::ReadWrite);

	template<typename T> 
	static void AddConsoleVariables(std::vector<std::string> variableNames, std::vector<T*> variables);

private:
	static void BindVariableToExternVariable(std::string externalVariable, void* variable);
	static void BindExternFunctions();

	static bool init;
};

template<typename T> 
void Console::AddConsoleVariable(std::string variableName, T* variable, Access access)
{
	BindVariableToExternVariable(variableName, (void*)variable);
}

template<typename T> 
void Console::AddConsoleVariables(std::vector<std::string> variableNames, std::vector<T*> variables)
{
	for (int i = 0; i < variableNames.size() && i < variables.size(); i++)
		BindVariableToExternVariable(variableNames[i], (void*)variables[i]);
}