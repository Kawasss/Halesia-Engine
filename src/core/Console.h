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

typedef std::string* ConsoleGroup;

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

	static ConsoleGroup CreateGroup(std::string name);
	template<typename T> static void AddVariableToGroup(ConsoleGroup group, T* variable, std::string name, ConsoleVariableAccess access = CONSOLE_ACCESS_READ_WRITE);
	template<typename T> static void AddConsoleVariable(std::string variableName, T* variable, ConsoleVariableAccess access = CONSOLE_ACCESS_READ_WRITE);
	template<typename T> static void AddConsoleVariables(std::vector<std::string> variableNames, std::vector<T*> variables);
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

	enum Lexeme
	{
		LEXEME_INVALID,
		LEXEME_IDENTIFIER,
		LEXEME_ENDLINE,
		LEXEME_NEWLINE,
		LEXEME_EXIT,
		LEXEME_DEFINE,
		LEXEME_STRING,
		LEXEME_FLOAT,
		LEXEME_INT,
		LEXEME_BOOL,
		LEXEME_DOT,
		LEXEME_COMMA,
		LEXEME_GET,
		LEXEME_SET,
		LEXEME_DISABLE,
		LEXEME_ENABLE,
		LEXEME_EQUALS,
		LEXEME_PLUSEQUALS,
		LEXEME_MINUSEQUALS,
		LEXEME_MULTIPLYEQUALS,
		LEXEME_DIVIDEEQUALS,
		LEXEME_ISNOT,
		LEXEME_ISNOT_SINGLE, // !identifier
		LEXEME_IS,
		LEXEME_PLUS,
		LEXEME_MINUS,
		LEXEME_MULTIPLY,
		LEXEME_DIVIDE,
		LEXEME_WHITESPACE,
		LEXEME_OPEN_CBRACKET,  // {
		LEXEME_CLOSE_CBRACKET, // }
		LEXEME_OPEN_SBRACKET,  // [
		LEXEME_CLOSE_SBRACKET, // ]
		LEXEME_OPEN_PARANTHESIS,
		LEXEME_CLOSE_PARANTHESIS,
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
		Token token         = LEXER_TOKEN_INVALID;
		std::string content = "";
		Lexeme lexemeToken  = LEXEME_INVALID;
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
		Lexeme operatorType;
		TokenContent lvalue;
		TokenContent rvalue;
	};

	static std::string TokenToString(Token token);
	static std::string variableTypeToString(VariableType type);
	static std::string GetVariableAsString(VariableMetadata& metadata);
	static std::vector<TokenContent> LexInput(std::string input);

	static Token GetToken(std::string string);
	static VariableMetadata& GetLValue(std::vector<TokenContent> tokens);
	template<typename T> static VariableType GetVariableType();

	static float GetValueFromTokenContent(TokenContent& content);
	static float GetFloatFromVariable(VariableMetadata& metadata);
	static float CalculateResult(std::vector<TokenContent>& tokens);
	static float CalculateOperator(float lvalue, float rvalue, Lexeme op);
	static float SolveInstruction(Instruction& instruction);
	
	static bool IsSeperatorToken(char item);
	static bool IsDigitLiteral(std::string string);
	static bool IsStringLiteral(std::string string);
	static bool CheckVariableValidity(VariableMetadata& metadata, ConsoleVariableAccess access);

	static void AddLocationValue(float& value, VariableMetadata& metadata);
	static void AssignRValueToLValue(VariableMetadata& lvalue, void* rvalue);
	static void EvaluateToken(TokenContent& token);
	static void DispatchCommand(std::vector<TokenContent>& lvalues, std::vector<TokenContent>& rvalues, TokenContent& op);
	static void InsertAliases(std::vector<TokenContent>& tokens);
	static void WriteConVars();

	static std::unordered_map<std::string, VariableMetadata>          commandVariables;
	static std::unordered_map<std::string, MessageSeverity>           messageColorBinding;
	static std::unordered_map<std::string, Group>                     groups;
	static std::unordered_map<std::string, VariableType>              stringToType;
	static std::unordered_map<std::string, Lexeme>                    stringToLexeme;
	static std::unordered_map<std::string, Token>                     stringToToken;
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

template<typename T> void Console::AddConsoleVariables(std::vector<std::string> variableNames, std::vector<T*> variables)
{
	for (int i = 0; i < variableNames.size(); i++)
		AddConsoleVariable(variableNames[i], variables[i]);
}