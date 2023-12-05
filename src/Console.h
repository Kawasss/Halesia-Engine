#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>
#include "glm.h"

enum MessageSeverity
{
	MESSAGE_SEVERITY_NORMAL,  // white
	MESSAGE_SEVERITY_WARNING, // yellow
	MESSAGE_SEVERITY_ERROR,   // red
	MESSAGE_SEVERITY_DEBUG    // blue
};

enum ConsoleVariableAccess
{
	CONSOLE_ACCESS_READ_WRITE,
	CONSOLE_ACCESS_READ_ONLY,
	CONSOLE_ACCESS_WRITE_ONLY
};

inline std::string MessageSeverityToString(MessageSeverity severity)
{
	switch (severity)
	{
	case MESSAGE_SEVERITY_NORMAL:
		return "MESSAGE_SEVERITY_NORMAL";
	case MESSAGE_SEVERITY_WARNING:
		return "MESSAGE_SEVERITY_WARNING";
	case MESSAGE_SEVERITY_ERROR:
		return "MESSAGE_SEVERITY_ERROR";
	case MESSAGE_SEVERITY_DEBUG:
		return "MESSAGE_SEVERITY_DEBUG";
	}
	return "";
}

inline std::string ConsoleVariableAccessToString(ConsoleVariableAccess access)
{
	switch (access)
	{
	case CONSOLE_ACCESS_READ_ONLY:
		return "CONSOLE_ACCESS_READ_ONLY";
	case CONSOLE_ACCESS_READ_WRITE:
		return "CONSOLE_ACCESS_READ_WRITE";
	case CONSOLE_ACCESS_WRITE_ONLY:
		return "CONSOLE_ACCESS_WRITE_ONLY";
	}
	return "";
}

class Console
{
public:
	static std::vector<std::string> messages;
	static bool isOpen;

	static void WriteLine(std::string message, MessageSeverity severity = MESSAGE_SEVERITY_NORMAL);
	static void InterpretCommand(std::string command = "");
	static glm::vec3 GetColorFromMessage(std::string message);

	template<typename T> static void AddConsoleVariable(std::string variableName, T* variable, ConsoleVariableAccess access = CONSOLE_ACCESS_READ_WRITE);

private:
	enum Token
	{
		LEXER_TOKEN_IDENTIFIER,
		LEXER_TOKEN_SEPERATOR,
		LEXER_TOKEN_LITERAL,
		LEXER_TOKEN_KEYWORD,
		LEXER_TOKEN_OPERATOR,
		LEXER_TOKEN_WHITESPACE
	};

	enum VariableType
	{
		VARIABLE_TYPE_INT,
		VARIABLE_TYPE_FLOAT,
		VARIABLE_TYPE_BOOL,
		VARIABLE_TYPE_STRING
	};

	struct TokenContent
	{
		Token token;
		std::string content;
	};

	struct VariableMetadata
	{
		void* location;
		int variableSize;
		VariableType type;
		ConsoleVariableAccess access;
	};

	struct Statement
	{
		TokenContent lvalue;
		TokenContent op;
		TokenContent rvalue;
	};

	static Token GetToken(std::string string);
	static float GetFloatFromLValue(VariableMetadata& metadata);
	static std::string TokenToString(Token token);
	static bool IsSeperatorToken(char item);
	static VariableMetadata& GetLValue(std::vector<TokenContent> tokens);
	static float CalculateRValue(std::vector<TokenContent> tokens);
	static void AddLocationValue(float& value, VariableMetadata& metadata);
	static float CalculateOperator(float lvalue, float rvalue, char op);
	static void AssignRValueToLValue(VariableMetadata& lvalue, void* rvalue);
	static std::vector<TokenContent> LexInput(std::string input);
	static float SolveStatement(Statement& statement);

	static std::map<std::string, VariableMetadata> commandVariables;
	static std::map<std::string, MessageSeverity> messageColorBinding;
};

template<typename T> void Console::AddConsoleVariable(std::string variableName, T* variable, ConsoleVariableAccess access)
{
	VariableType type = VARIABLE_TYPE_INT;

	if (std::is_same_v<T, int>)
		type = VARIABLE_TYPE_INT;
	else if (std::is_same_v<T, float>)
		type = VARIABLE_TYPE_FLOAT;
	else if (std::is_same_v<T, bool>)
		type = VARIABLE_TYPE_BOOL;
	else if (std::is_same_v<T, std::string>)
		type = VARIABLE_TYPE_STRING;

	commandVariables[variableName] = { static_cast<void*>(variable), sizeof(*variable), type, access };
}