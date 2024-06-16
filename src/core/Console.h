#pragma once
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <iostream>
#include "glm.h"

#ifdef _DEBUG
#define DEBUG_ONLY(val) val
#else
#define DEBUG_ONLY(val)
#endif

enum MessageSeverity
{
	MESSAGE_SEVERITY_NORMAL,  // white
	MESSAGE_SEVERITY_WARNING, // yellow
	MESSAGE_SEVERITY_ERROR,   // red
	MESSAGE_SEVERITY_DEBUG    // blue
};
inline extern std::string MessageSeverityToString(MessageSeverity severity);

enum ConsoleVariableAccess
{
	CONSOLE_ACCESS_READ_WRITE,
	CONSOLE_ACCESS_READ_ONLY,
	CONSOLE_ACCESS_WRITE_ONLY
};
inline extern std::string ConsoleVariableAccessToString(ConsoleVariableAccess access);

class Console
{
public:
	static std::vector<std::string> messages;
	static bool isOpen;

	static void Init();
	static void WriteLine(std::string message, MessageSeverity severity = MESSAGE_SEVERITY_NORMAL);
	static void InterpretCommand(std::string command = "");
	static glm::vec3 GetColorFromMessage(std::string message);

	#define HalesiaDebugLog(message) DEBUG_ONLY(Console::WriteLine(message))

	template<typename T> static void AddConsoleVariable(std::string variableName, T* variable, ConsoleVariableAccess access = CONSOLE_ACCESS_READ_WRITE);
	template<typename T> static void AddConsoleVariables(std::vector<std::string> variableNames, std::vector<T*> variables);
	static void BindVariableToExternVariable(std::string externalVariable, void* variable);
	
private:
	static void BindExternFunctions();
	
	static std::unordered_map<std::string, MessageSeverity> messageColorBinding;
	static bool init;
};

template<typename T> void Console::AddConsoleVariable(std::string variableName, T* variable, ConsoleVariableAccess access)
{
	BindVariableToExternVariable(variableName, (void*)variable);
}

template<typename T> void Console::AddConsoleVariables(std::vector<std::string> variableNames, std::vector<T*> variables)
{
	for (int i = 0; i < variableNames.size() && i < variables.size(); i++)
		BindVariableToExternVariable(variableNames[i], (void*)variables[i]);
}