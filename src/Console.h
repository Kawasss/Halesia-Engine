#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <unordered_map>
#include "glm.h"

typedef std::string* ConsoleGroup;

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

	static void Init();
	static void WriteLine(std::string message, MessageSeverity severity = MESSAGE_SEVERITY_NORMAL);
	static void InterpretCommand(std::string command = "");
	static glm::vec3 GetColorFromMessage(std::string message);

	static ConsoleGroup CreateGroup(std::string name);
	template<typename T> static void AddVariableToGroup(ConsoleGroup group, T* variable, std::string name, ConsoleVariableAccess access = CONSOLE_ACCESS_READ_WRITE);
	template<typename T> static void AddConsoleVariable(std::string variableName, T* variable, ConsoleVariableAccess access = CONSOLE_ACCESS_READ_WRITE);
	static void BindVariableToExternVariable(std::string externalVariable, void* variable);
	
private:
	enum Token
	{
		LEXER_TOKEN_INVALID,
		LEXER_TOKEN_IDENTIFIER,
		LEXER_TOKEN_SEPERATOR,
		LEXER_TOKEN_LITERAL,
		LEXER_TOKEN_KEYWORD,
		LEXER_TOKEN_OPERATOR,
		LEXER_TOKEN_WHITESPACE
	};

	enum LexemeToken
	{
		LEXEME_TOKEN_INVALID,
		LEXEME_TOKEN_IDENTIFIER,
		LEXEME_TOKEN_ENDLINE,
		LEXEME_TOKEN_NEWLINE,
		LEXEME_TOKEN_EXIT,
		LEXEME_TOKEN_DEFINE,
		LEXEME_TOKEN_STRING,
		LEXEME_TOKEN_FLOAT,
		LEXEME_TOKEN_INT,
		LEXEME_TOKEN_BOOL,
		LEXEME_TOKEN_DOT,
		LEXEME_TOKEN_COMMA,
		LEXEME_TOKEN_GET,
		LEXEME_TOKEN_SET,
		LEXEME_TOKEN_DISABLE,
		LEXEME_TOKEN_ENABLE,
		LEXEME_TOKEN_EQUALS,
		LEXEME_TOKEN_PLUSEQUALS,
		LEXEME_TOKEN_MINUSEQUALS,
		LEXEME_TOKEN_MULTIPLYEQUALS,
		LEXEME_TOKEN_DIVIDEEQUALS,
		LEXEME_TOKEN_ISNOT,
		LEXEME_TOKEN_ISNOT_SINGLE, // !{var}
		LEXEME_TOKEN_IS,
		LEXEME_TOKEN_PLUS,
		LEXEME_TOKEN_MINUS,
		LEXEME_TOKEN_MULTIPLY,
		LEXEME_TOKEN_DIVIDE,
		LEXEME_TOKEN_WHITESPACE,
		LEXEME_TOKEN_OPEN_CBRACKET,  // {
		LEXEME_TOKEN_CLOSE_CBRACKET, // }
		LEXEME_TOKEN_OPEN_SBRACKET,  // [
		LEXEME_TOKEN_CLOSE_SBRACKET, // ]
		LEXEME_TOKEN_OPEN_PARANTHESIS,
		LEXEME_TOKEN_CLOSE_PARANTHESIS,
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
		Token token             = LEXER_TOKEN_INVALID;
		std::string content     = "";
		LexemeToken lexemeToken = LEXEME_TOKEN_INVALID;
	};

	struct VariableMetadata
	{
		void* location               = nullptr;
		int variableSize             = 0;
		VariableType type            = VARIABLE_TYPE_INT;
		ConsoleVariableAccess access = CONSOLE_ACCESS_READ_WRITE;
	};

	struct Group
	{
		std::string name;
		std::unordered_map<std::string, VariableMetadata> variables;
	};

	struct Instruction
	{
		LexemeToken op;
		TokenContent lvalue;
		TokenContent rvalue;
	};

	static Token GetToken(std::string string);
	static float GetFloatFromLValue(VariableMetadata& metadata);
	static std::string TokenToString(Token token);
	static bool IsSeperatorToken(char item);
	static bool IsDigitLiteral(std::string string);
	static VariableMetadata& GetLValue(std::vector<TokenContent> tokens);
	static float CalculateRValue(std::vector<TokenContent> tokens);
	static void AddLocationValue(float& value, VariableMetadata& metadata);
	static float CalculateOperator(float lvalue, float rvalue, LexemeToken op);
	static void AssignRValueToLValue(VariableMetadata& lvalue, void* rvalue);
	static std::vector<TokenContent> LexInput(std::string input);
	static float SolveInstruction(Instruction& instruction);
	static void EvaluateToken(TokenContent& token);
	static void DispatchCommand(std::vector<TokenContent>& lvalues, std::vector<TokenContent>& rvalues, TokenContent& op);
	static std::string GetVariableAsString(VariableMetadata& metadata);
	template<typename T> static VariableType GetVariableType();

	static std::unordered_map<std::string, VariableMetadata> commandVariables;
	static std::unordered_map<std::string, MessageSeverity> messageColorBinding;
	static std::unordered_map<std::string, Group> groups;
	static std::unordered_map<std::string, VariableType> stringToType;
	static std::unordered_map<std::string, std::vector<TokenContent>> aliases;
};

template<typename T> Console::VariableType Console::GetVariableType()
{
	if (std::is_same_v<T, int>)
		return VARIABLE_TYPE_INT;
	if (std::is_same_v<T, float>)
		return VARIABLE_TYPE_FLOAT;
	if (std::is_same_v<T, bool>)
		return VARIABLE_TYPE_BOOL;
	if (std::is_same_v<T, std::string>)
		return VARIABLE_TYPE_STRING;
	return VARIABLE_TYPE_INT;
}

template<typename T> void Console::AddVariableToGroup(ConsoleGroup group, T* variable, std::string name, ConsoleVariableAccess access)
{
	std::string groupName = *static_cast<std::string*>(group);
	VariableMetadata metadata{};
	metadata.access = access;
	metadata.location = variable;
	metadata.variableSize = sizeof(*variable);
	metadata.type = GetVariableType<T>();

	groups[groupName].variables[name] = metadata;
}

template<typename T> void Console::AddConsoleVariable(std::string variableName, T* variable, ConsoleVariableAccess access)
{
	commandVariables[variableName] = { static_cast<void*>(variable), sizeof(*variable), GetVariableType<T>(), access};
}