#include "Console.h"
#include <regex>
#include <iostream>
#include <vector>
#include <mutex>
#include "system/Input.h"

std::vector<std::string> Console::messages{};
std::map<std::string, Console::VariableMetadata> Console::commandVariables{};
std::map<std::string, MessageSeverity> Console::messageColorBinding{};

bool Console::isOpen = false;

std::mutex writingLinesMutex;
void Console::WriteLine(std::string message, MessageSeverity severity)
{
	std::lock_guard<std::mutex> guard(writingLinesMutex);
	messages.push_back(message);
	messageColorBinding[message] = severity;

	#ifdef _DEBUG
	std::string severityString = severity == MESSAGE_SEVERITY_NORMAL ? "" : " (" + MessageSeverityToString(severity) + ")";
	std::cout << message << severityString << "\n";
	#endif
}

void Console::InterpretCommand(std::string command)
{
	std::string variableName;

	if (command == "exit")
		exit(0);

	std::vector<TokenContent> tokens = LexInput(command);
	std::vector<TokenContent> lValues;
	std::vector<TokenContent> rValues;
	TokenContent equalsOperator{ LEXER_TOKEN_SEPERATOR, "" };
	bool printNextValue = false;
	for (int i = 0; i < tokens.size(); i++)
	{
		TokenContent& content = tokens[i];
		switch (content.token)
		{
		case LEXER_TOKEN_KEYWORD:
			if (tokens.size() == 1)
			{
				WriteLine("Failed to validate a keyword, no arguments were given", MESSAGE_SEVERITY_ERROR);
				return;
			}
			if (content.content == "disable" || content.content == "enable")
			{
				*static_cast<bool*>(commandVariables[tokens[(size_t)i + 1].content].location) = content.content == "enable";
				return;
			}
			printNextValue = content.content == "get";
			break;
		case LEXER_TOKEN_IDENTIFIER:
			if (commandVariables.count(content.content) == 0)
			{
				WriteLine("Unidentified token: the variable \"" + content.content + "\" was queried but it does not exist", MESSAGE_SEVERITY_ERROR);
				return;
			}
			if (!printNextValue)
				break;
			if (commandVariables[content.content].access = CONSOLE_ACCESS_WRITE_ONLY)
			{
				WriteLine("Failed to print variable \"" + content.content + "\": it is write only (" + ConsoleVariableAccessToString(commandVariables[content.content].access) + ")", MESSAGE_SEVERITY_ERROR);
				return;
			}
			switch (commandVariables[content.content].type)
			{
			case VARIABLE_TYPE_BOOL:
				WriteLine(*static_cast<bool*>(commandVariables[content.content].location) == 1 ? "true" : "false");
				break;
			case VARIABLE_TYPE_FLOAT:
				WriteLine(std::to_string(*static_cast<float*>(commandVariables[content.content].location)));
				break;
			case VARIABLE_TYPE_INT:
				WriteLine(std::to_string(*static_cast<int*>(commandVariables[content.content].location)));
				break;
			case VARIABLE_TYPE_STRING:
				WriteLine(*static_cast<std::string*>(commandVariables[content.content].location));
				break;
			}
			return;

		case LEXER_TOKEN_OPERATOR:
			if (content.content == "=" || (content.content.size() == 2 && content.content[0] != '=')) // an operator like += uses two chars of which the first isnt =
			{
				lValues = { tokens.begin(), tokens.begin() + i };
				rValues = { tokens.begin() + i + 1, tokens.end() };
				if (content.content.size() == 2 && content.content[0] != '=')
					equalsOperator = content;
			}
			break;
		}
	}
	if (lValues.empty() || rValues.empty())
	{
		WriteLine("Cannot continue interpreting: no lvalues or rvalues were given", MESSAGE_SEVERITY_WARNING);
		return;
	}

	VariableMetadata& lvalue = GetLValue(lValues);
	if (lvalue.access == CONSOLE_ACCESS_READ_ONLY)
	{
		WriteLine("Failed to assign a value to variable \"" + lValues[0].content + "\", it is read only (" + ConsoleVariableAccessToString(lvalue.access) + ")", MESSAGE_SEVERITY_ERROR);
		return;
	}
	if (rValues.size() == 1) // messy
	{
		if (lvalue.type == VARIABLE_TYPE_STRING)
		{
			AssignRValueToLValue(lvalue, &rValues[0].content);
			return;
		}

		float rvalueF = 0;
		int rvalueI = 0;
		bool useIntRvalue = lvalue.type == VARIABLE_TYPE_INT || lvalue.type == VARIABLE_TYPE_BOOL;
		if (useIntRvalue)
			rvalueI = std::stoi(rValues[0].content);
		else
			rvalueF = std::stof(rValues[0].content);
		void* ptr = useIntRvalue ? &rvalueI : (void*)&rvalueF;
		AssignRValueToLValue(lvalue, ptr);
	}
	else
	{
		float result = CalculateRValue(rValues);
		if (equalsOperator.content[0] == '-' || equalsOperator.content[0] == '+' || equalsOperator.content[0] == '*' || equalsOperator.content[0] == '/')
		{
			float lvalueF = GetFloatFromLValue(lvalue);
			result = CalculateOperator(lvalueF, result, equalsOperator.content[0]);

		}
		int resultI = (int)result;
		AssignRValueToLValue(lvalue, lvalue.type == VARIABLE_TYPE_INT ? &resultI : (void*)&result);
	}
#ifdef _DEBUG
	std::string result = "";
	if (lvalue.type == VARIABLE_TYPE_INT)
		result = std::to_string(*static_cast<int*>(lvalue.location));
	else if (lvalue.type == VARIABLE_TYPE_FLOAT)
		result = std::to_string(*static_cast<float*>(lvalue.location));
	else if (lvalue.type == VARIABLE_TYPE_BOOL)
		result = *static_cast<bool*>(lvalue.location) == 1 ? "true" : "false";
	else if (lvalue.type == VARIABLE_TYPE_STRING)
		result = *static_cast<std::string*>(lvalue.location);
	std::cout << "Set variable \"" + lValues[0].content + "\" to " << result << " at 0x" << lvalue.location << "\n";
#endif
}

float Console::GetFloatFromLValue(VariableMetadata& metadata)
{
	switch (metadata.type)
	{
	case VARIABLE_TYPE_BOOL:  return *static_cast<bool*>(metadata.location);
	case VARIABLE_TYPE_FLOAT: return *static_cast<float*>(metadata.location);
	case VARIABLE_TYPE_INT:   return *static_cast<int*>(metadata.location);
	}
	return 0;
}

std::vector<Console::TokenContent> Console::LexInput(std::string input)
{
	std::string lexingString;
	std::vector<TokenContent> tokens;
	for (int i = 0; i < input.size(); i++)
	{
		if (IsSeperatorToken(input[i]))
		{
			tokens.push_back({ GetToken(lexingString), lexingString });
			tokens.push_back({ LEXER_TOKEN_SEPERATOR, "" + input[i] });
			lexingString.clear();
			continue;
		}
		lexingString += input[i];
		if (i == input.size() - 1)
			tokens.push_back({ GetToken(lexingString), lexingString });
	}
	return tokens;
}

float Console::SolveStatement(Statement& statement)
{
	float lvalue = std::stof(statement.lvalue.content);
	float rvalue = std::stof(statement.rvalue.content);
	char op = statement.op.content[0];
	return CalculateOperator(lvalue, rvalue, op);
}

Console::VariableMetadata& Console::GetLValue(std::vector<TokenContent> tokens)
{
	return commandVariables[tokens[0].content];
}

void Console::AssignRValueToLValue(VariableMetadata& lvalue, void* rvalue)
{
	switch (lvalue.type)
	{
	case VARIABLE_TYPE_BOOL:
		*static_cast<bool*>(lvalue.location) = *static_cast<bool*>(rvalue);
		break;
	case VARIABLE_TYPE_FLOAT:
		*static_cast<float*>(lvalue.location) = *static_cast<float*>(rvalue);
		break;
	case VARIABLE_TYPE_INT:
		*static_cast<int*>(lvalue.location) = *static_cast<int*>(rvalue);
		break;
	case VARIABLE_TYPE_STRING:
		*static_cast<std::string*>(lvalue.location) = *static_cast<std::string*>(rvalue);
		break;
	}
}

float Console::CalculateOperator(float lvalue, float rvalue, char op)
{
	switch (op)
	{
	case '+': return lvalue + rvalue;
	case '-': return lvalue - rvalue;
	case '*': return lvalue * rvalue;
	case '/': return lvalue / rvalue;
	default: WriteLine("Undefined operator \"" + op + '"', MESSAGE_SEVERITY_ERROR);
	}
	return 0;
}

float Console::CalculateRValue(std::vector<TokenContent> tokens)
{
	std::vector<TokenContent> usableTokens;
	for (TokenContent& token : tokens)
	{
		if (token.token != LEXER_TOKEN_SEPERATOR && token.token != LEXER_TOKEN_WHITESPACE)
			usableTokens.push_back(token);
	}

	if (usableTokens.size() != 3 && usableTokens.size() != 1)
	{
		WriteLine("Invalid amount of tokens: the rvalue can only have 3 tokens for a statement", MESSAGE_SEVERITY_ERROR);
		return 0;
	}
	if (usableTokens.size() == 1)
		return std::stof(usableTokens[0].content);

	Statement statement;
	statement.lvalue = tokens[0];
	statement.op	 = tokens[1];
	statement.rvalue = tokens[2];
	float result = SolveStatement(statement); // always do the calculations with a float, because if the lvalue is an int it can just be rounded into one

	return result;
}

Console::Token Console::GetToken(std::string string)
{
	if (string.empty())
	{
		WriteLine("Cannot lex giving input (string.empty())", MESSAGE_SEVERITY_ERROR);
		return LEXER_TOKEN_WHITESPACE;
	}

	if (string == "get" || string == "set" || string == "enable" || string == "disable")
		return LEXER_TOKEN_KEYWORD;
	if (string == "=" || string == "-" || string == "+" || string == "/" || string == "*" || (string.size() == 2 || string[0] != '=' && string[1] == '='))
		return LEXER_TOKEN_OPERATOR;

	if (string[0] == '"' && string.back() == '"') // literal strings are encased in "'s
		return LEXER_TOKEN_LITERAL;
	if (IsSeperatorToken(string[0]))
		return LEXER_TOKEN_SEPERATOR;
	if (string[0] == ' ')
		return LEXER_TOKEN_WHITESPACE;

	for (int i = 0; i < string.size(); i++) // check if there are any non-digit chars in the string, if there arent its a number literal
	{
		if (!std::isdigit(string[i]))
			break;
		if (i == string.size() - 1)
			return LEXER_TOKEN_LITERAL;
	}
	if (commandVariables.count(string) == 0)
	{
		WriteLine("Identifier \"" + string + "\" is undefined", MESSAGE_SEVERITY_ERROR);
		return LEXER_TOKEN_WHITESPACE;
	}
	return LEXER_TOKEN_IDENTIFIER;
}

void Console::AddLocationValue(float& value, VariableMetadata& metadata)
{
	switch (metadata.type)
	{
	case VARIABLE_TYPE_BOOL:
		value = *static_cast<bool*>(metadata.location);
		break;
	case VARIABLE_TYPE_FLOAT:
		value = *static_cast<float*>(metadata.location);
		break;
	case VARIABLE_TYPE_INT:
		value = (float)*static_cast<int*>(metadata.location);
		break;
	}
}

std::string Console::TokenToString(Token token)
{
	switch (token)
	{
	case LEXER_TOKEN_IDENTIFIER:
		return "LEXER_TOKEN_IDENTIFIER";
	case LEXER_TOKEN_KEYWORD:
		return "LEXER_TOKEN_KEYWORD";
	case LEXER_TOKEN_LITERAL:
		return "LEXER_TOKEN_LITERAL";
	case LEXER_TOKEN_OPERATOR:
		return "LEXER_TOKEN_OPERATOR";
	case LEXER_TOKEN_SEPERATOR:
		return "LEXER_TOKEN_SEPERATOR";
	}
	return "";
}

bool Console::IsSeperatorToken(char item)
{
	switch (item)
	{
	case ' ':
	case ';':
	case '(':
	case ')':
		return true;
	default: 
		return false;
	}
	return false;
}

glm::vec3 Console::GetColorFromMessage(std::string message)
{
	switch (messageColorBinding[message])
	{
	case MESSAGE_SEVERITY_NORMAL:
		return glm::vec3(1);
	case MESSAGE_SEVERITY_WARNING:
		return glm::vec3(1, 1, 0);
	case MESSAGE_SEVERITY_ERROR:
		return glm::vec3(1, 0, 0);
	case MESSAGE_SEVERITY_DEBUG:
		return glm::vec3(.1f, .1f, 1);
	default:
		return glm::vec3(1);
	}
}