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

	std::string lexingString;
	std::vector<TokenContent> tokens;
	for (int i = 0; i < command.size(); i++)
	{
		if (IsSeperatorToken(command[i]))
		{
			tokens.push_back({ GetToken(lexingString), lexingString });
			lexingString.clear();
			continue;
		}
		lexingString += command[i];
		if (i == command.size() - 1)
			tokens.push_back({ GetToken(lexingString), lexingString });
	}

	std::vector<TokenContent> lValues;
	std::vector<TokenContent> rValues;
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
			if (content.content == "=")
			{
				lValues = { tokens.begin(), tokens.begin() + i };
				rValues = { tokens.begin() + i + 1, tokens.end() };
			}
			break;
		}
	}
	if (lValues.empty() || rValues.empty())
	{
		WriteLine("Cannot continue interpreting: no lvalues or rvalues were given", MESSAGE_SEVERITY_WARNING);
		return;
	}

	VariableMetadata& metadata = GetLValue(lValues);
	if (metadata.access == CONSOLE_ACCESS_READ_ONLY)
	{
		WriteLine("Failed to assign a value to variable \"" + lValues[0].content + "\", it is read only (" + ConsoleVariableAccessToString(metadata.access) + ")", MESSAGE_SEVERITY_ERROR);
		return;
	}
	CalculateRValue(metadata.location, metadata.variableSize, rValues, metadata.type);

#ifdef _DEBUG
	std::string result = "";
	if (metadata.type == VARIABLE_TYPE_INT)
		result = std::to_string(*static_cast<int*>(metadata.location));
	else if (metadata.type == VARIABLE_TYPE_FLOAT)
		result = std::to_string(*static_cast<float*>(metadata.location));
	else if (metadata.type == VARIABLE_TYPE_BOOL)
		result = *static_cast<bool*>(metadata.location) == 1 ? "true" : "false";
	else if (metadata.type == VARIABLE_TYPE_STRING)
		result = *static_cast<std::string*>(metadata.location);
	std::cout << "Set variable \"" + lValues[0].content + "\" to " << result << " at 0x" << metadata.location << "\n";
#endif
}

Console::VariableMetadata& Console::GetLValue(std::vector<TokenContent> tokens)
{
	return commandVariables[tokens[0].content];
}

void Console::CalculateRValue(void* locationToWriteTo, int expectedWriteSize, std::vector<TokenContent> tokens, VariableType lValueType)
{
	float calculation = 0; // always do the calculations with a float, because if the lvalue is an int it can just be rounded into one

	for (size_t i = 0; i < tokens.size(); i++)
	{
		switch (tokens[i].token)
		{
		case LEXER_TOKEN_LITERAL:
			if (i == 0 && tokens[i].content[0] == '"' && tokens[i].content.back() == '"') // string
			{
				if (tokens.size() > 1)
				{
					WriteLine("Illegal operation on string literal: multiple rvalues detected, which is not allowed for strings", MESSAGE_SEVERITY_ERROR);
					return;
				}
				tokens[i].content.back() = '\0'; // set the second to last value to null to remove the " at the end
				*static_cast<std::string*>(locationToWriteTo) = tokens[i].content.c_str() + 1; // move the pointer up by one to bypass the first "
				return;
			}
			else // if its not a string its an int or float, but because the calculations are done with a float it is automatically converted to a float no matter what
				calculation = std::stof(tokens[i].content);
			break;

		case LEXER_TOKEN_IDENTIFIER:
		{
			TokenContent& rValueOfIdentifier = tokens[i];
			if (rValueOfIdentifier.token == LEXER_TOKEN_IDENTIFIER)
			{
				VariableMetadata& data = commandVariables[rValueOfIdentifier.content];
				AddLocationValue(calculation, data);
			}
			break;
		}

		case LEXER_TOKEN_OPERATOR:
			if (i == 0)
			{
				WriteLine("Failed to calculate an operation: no lvalue was given", MESSAGE_SEVERITY_ERROR);
				return;
			}
			if (i + 1 >= tokens.size()) // prevent an index out of bounds error
			{
				WriteLine("Failed to calculate the rvalue: no second argument was given for the operation", MESSAGE_SEVERITY_ERROR);
				return;
			}
			TokenContent& rValueOfOperation = tokens[i + 1];
			float rValueFloat = 0;
			if (rValueOfOperation.token == LEXER_TOKEN_IDENTIFIER)
			{
				VariableMetadata& metadata = commandVariables[rValueOfOperation.content];
				AddLocationValue(rValueFloat, metadata);
			}
			else
				rValueFloat = std::stof(rValueOfOperation.content);

			switch (tokens[i].content[0])
			{
			case '+':
				calculation += rValueFloat;
				break;
			case '-':
				calculation -= rValueFloat;
				break;
			case '*':
				calculation *= rValueFloat;
				break;
			case '/':
				calculation /= rValueFloat;
				break;
			default:
				WriteLine("Failed to calculate an operation, an invalid operator has been given: " + tokens[i].content, MESSAGE_SEVERITY_ERROR);
				break;
			}
			i++;
			break;
		}
	}
	switch (lValueType)
	{
	case VARIABLE_TYPE_BOOL:
		*static_cast<bool*>(locationToWriteTo) = (bool)calculation;
		break;
	case VARIABLE_TYPE_FLOAT:
		*static_cast<float*>(locationToWriteTo) = calculation;
		break;
	case VARIABLE_TYPE_INT:
		*static_cast<int*>(locationToWriteTo) = (int)calculation;
	}
}

Console::Token Console::GetToken(std::string string)
{
	if (string.empty())
	{
		WriteLine("Invalid string passed to the console lexer", MESSAGE_SEVERITY_ERROR);
		return LEXER_TOKEN_SEPERATOR;
	}

	if (string == "get" || string == "set" || string == "enable" || string == "disable")
		return LEXER_TOKEN_KEYWORD;
	if (string == "=" || string == "-" || string == "+" || string == "/" || string == "*")
		return LEXER_TOKEN_OPERATOR;
	if (string[0] == '"' && string.back() == '"') // literal strings are encased in "'s
		return LEXER_TOKEN_LITERAL;

	for (int i = 0; i < string.size(); i++) // check if there are any non-digit chars in the string, if there arent its a number literal
	{
		if (!std::isdigit(string[i]))
			break;
		if (i == string.size() - 1)
			return LEXER_TOKEN_LITERAL;
	}
	if (commandVariables.count(string) == 0)
	{
		WriteLine("Failed to find a token for the given string \"" + string + '"', MESSAGE_SEVERITY_ERROR);
		return LEXER_TOKEN_SEPERATOR;
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