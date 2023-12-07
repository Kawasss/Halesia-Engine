#include "Console.h"
#include <regex>
#include <iostream>
#include <vector>
#include <mutex>
#include <fstream>
#include "system/Input.h"

std::vector<std::string> Console::messages{};
std::unordered_map<std::string, Console::VariableMetadata> Console::commandVariables{};
std::unordered_map<std::string, MessageSeverity> Console::messageColorBinding{};
std::unordered_map<std::string, Console::Group> Console::groups{};
std::unordered_map<std::string, std::vector<Console::TokenContent>> Console::aliases{};

std::unordered_map<std::string, Console::VariableType> Console::stringToType = 
{ 
	{ "float", Console::VariableType::VARIABLE_TYPE_FLOAT }, { "int", Console::VariableType::VARIABLE_TYPE_INT }, 
	{ "bool", Console::VariableType::VARIABLE_TYPE_BOOL }, { "string", Console::VariableType::VARIABLE_TYPE_STRING } 
};

std::unordered_map<std::string, ConsoleVariableAccess> accessKeywords = { { "readonly", CONSOLE_ACCESS_READ_ONLY }, { "writeonly", CONSOLE_ACCESS_WRITE_ONLY } };

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

inline std::vector<char> ReadCfg(const std::string& filePath)
{
	std::ifstream file(filePath, std::ios::ate);
	if (!file.is_open())
		throw std::runtime_error("Failed to open the shader at " + filePath);

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize + 1);
	buffer.back() = '\0';

	file.seekg(0);
	file.read(buffer.data(), fileSize);
	
	file.close();
	return buffer;
}

void Console::Init()
{
	std::ifstream file("cfg/init.cfg", std::ios::ate | std::ios::binary);
	if (file.good())
	{
		InterpretCommand(ReadCfg("cfg/init.cfg").data());
		std::cout << "Initialised console with init.cfg" << "\n";
	}
	else
		std::cout << "Initialised console without init.cfg" << "\n";
		
}

void Console::InterpretCommand(std::string command)
{
	std::string variableName;

	std::vector<TokenContent> tokens = LexInput(command), lValues, rValues;
	for (size_t i = 0; i < tokens.size(); i++) // process all aliases first
	{
		if (tokens[i].lexemeToken != LEXEME_TOKEN_IDENTIFIER || aliases.count(tokens[i].content) <= 0)
			continue;

		std::cout << "Caught alias \"" << tokens[i].content << "\": replacing tokens...\n";
		std::vector<TokenContent>& aliasTokens = aliases[tokens[i].content];
		tokens[i] = aliasTokens[0];
		if (aliasTokens.size() > 1)
		{
			tokens.insert(tokens.begin() + i + 1, aliasTokens.begin() + 1, aliasTokens.end());
			i = 0;
		}
	}

	TokenContent equalsOperator{ LEXER_TOKEN_SEPERATOR, "" };
	bool isOnRightSideOfOperator = false;
	for (size_t i = 0; i < tokens.size(); i++)
	{
		TokenContent& content = tokens[i];
		
		if (content.content == " ")
			continue;
		switch (content.lexemeToken)
		{
		case LEXEME_TOKEN_DISABLE:
		case LEXEME_TOKEN_ENABLE:
			*static_cast<bool*>(commandVariables[tokens[i + 1].content].location) = content.lexemeToken == LEXEME_TOKEN_ENABLE;
			break;

		case LEXEME_TOKEN_GET:
		{
			if (commandVariables.count(tokens[i + 1].content) == 0)
			{
				WriteLine("Unknown identfier \"" + tokens[i + 1].content + "\"", MESSAGE_SEVERITY_ERROR);
				return;
			}
			VariableMetadata& printVarMetadata = commandVariables[tokens[i + 1].content];
			if (printVarMetadata.access == CONSOLE_ACCESS_WRITE_ONLY)
			{
				WriteLine("Failed to print variable: it is write only (" + ConsoleVariableAccessToString(printVarMetadata.access) + ")", MESSAGE_SEVERITY_ERROR);
				return;
			}
			WriteLine(GetVariableAsString(printVarMetadata));
			i++;
			break;
		}

		case LEXEME_TOKEN_DEFINE:
		{
			std::string aliasName = tokens[i + 1].content;
			
			size_t beginIndex = i;
			for (; i < tokens.size(); i++) // keep iterating through the tokens with i until the token to stop a define is found
			{
				if (tokens[i].lexemeToken == LEXEME_TOKEN_NEWLINE)
					break;
			} 
			std::vector<TokenContent> aliasTokens = { tokens.begin() + beginIndex + 2, tokens.begin() + i };
			aliases[aliasName] = aliasTokens;
			std::cout << "Created new alias \"" << aliasName << "\" with a defition of " << aliasTokens.size() << " tokens" << "\n";
			if (i + 1 >= tokens.size()) // safe exit here since the define is the entire command
				return;
			break;
		}

		case LEXEME_TOKEN_ENDLINE:
			DispatchCommand(lValues, rValues, equalsOperator);
			lValues.clear();
			rValues.clear();
			isOnRightSideOfOperator = false;
			break;

		case LEXEME_TOKEN_EXIT:
			std::cout << "Exit has been requested via console" << "\n";
			exit(0);

		case LEXEME_TOKEN_EQUALS:
		case LEXEME_TOKEN_PLUSEQUALS:
		case LEXEME_TOKEN_MINUSEQUALS:
		case LEXEME_TOKEN_MULTIPLYEQUALS:
		case LEXEME_TOKEN_DIVIDEEQUALS:
			equalsOperator = content;
			isOnRightSideOfOperator = true;
			continue;
		}
		if (!isOnRightSideOfOperator)
			lValues.push_back(tokens[i]);
		else
			rValues.push_back(tokens[i]);
		if (i == tokens.size() - 1 && !lValues.empty() && !rValues.empty())
			DispatchCommand(lValues, rValues, equalsOperator);
	}
}

void Console::DispatchCommand(std::vector<TokenContent>& lvalues, std::vector<TokenContent>& rvalues, TokenContent& op)
{
	if (lvalues.empty() || rvalues.empty())
	{
		WriteLine("Cannot continue interpreting: no lvalues or rvalues were given", MESSAGE_SEVERITY_WARNING);
		return;
	}

	VariableMetadata& lvalue = GetLValue(lvalues);
	if (lvalue.access == CONSOLE_ACCESS_READ_ONLY)
	{
		WriteLine("Failed to assign a value to variable \"" + lvalues[0].content + "\", it is read only (" + ConsoleVariableAccessToString(lvalue.access) + ")", MESSAGE_SEVERITY_ERROR);
		return;
	}
	if (lvalue.location == nullptr)
	{
		WriteLine("unknown external identifier: the location of the lvalue is nullptr", MESSAGE_SEVERITY_ERROR);
		return;
	}

	if (lvalue.type == VARIABLE_TYPE_STRING)
	{
		AssignRValueToLValue(lvalue, &rvalues[0].content);
		return;
	}
	float resultF = CalculateRValue(rvalues);
	float lvalueF = GetFloatFromLValue(lvalue);
	resultF = CalculateOperator(lvalueF, resultF, op.lexemeToken);
	int resultI = (int)resultF;
	AssignRValueToLValue(lvalue, lvalue.type == VARIABLE_TYPE_INT || lvalue.type == VARIABLE_TYPE_BOOL ? &resultI : (void*)&resultF);

#ifdef _DEBUG
	std::cout << "Set variable \"" + lvalues.back().content + "\" to " << GetVariableAsString(lvalue) << " at 0x" << lvalue.location << "\n";
#endif
}

std::string Console::GetVariableAsString(VariableMetadata& metadata)
{
	switch (metadata.type)
	{
	case VARIABLE_TYPE_BOOL:   return *static_cast<bool*>(metadata.location) == 1 ? "true" : "false";
	case VARIABLE_TYPE_FLOAT:  return std::to_string(*static_cast<float*>(metadata.location));
	case VARIABLE_TYPE_INT:    return std::to_string(*static_cast<int*>(metadata.location));
	case VARIABLE_TYPE_STRING: return *static_cast<std::string*>(metadata.location);
	}
	return "";
}

void Console::BindVariableToExternVariable(std::string externalVariable, void* variable)
{
	std::vector<TokenContent> tokens = LexInput(externalVariable);
	VariableMetadata& metadata = GetLValue(tokens);
	metadata.location = variable;
}

ConsoleGroup Console::CreateGroup(std::string name)
{
	Group group;
	group.name = name;
	groups[name] = group;
	return &groups[name].name;
}

float Console::GetFloatFromLValue(VariableMetadata& metadata)
{
	switch (metadata.type)
	{
	case VARIABLE_TYPE_BOOL:  return *static_cast<bool*>(metadata.location);
	case VARIABLE_TYPE_FLOAT: return *static_cast<float*>(metadata.location);
	case VARIABLE_TYPE_INT:   return (float)*static_cast<int*>(metadata.location);
	}
	return 0;
}

std::vector<Console::TokenContent> Console::LexInput(std::string input)
{
	std::string lexingString;
	std::vector<TokenContent> tokens;
	bool isCommentary = false;
	for (int i = 0; i < input.size(); i++)
	{
		isCommentary = input[i] == '#' || isCommentary && !(isCommentary && input[i] == '\n');
		if (isCommentary)
			continue;

		if (IsSeperatorToken(input[i]))
		{
			if (!lexingString.empty())
			{
				tokens.push_back({ GetToken(lexingString), lexingString });
				EvaluateToken(tokens.back());
			}
			tokens.push_back({ LEXER_TOKEN_SEPERATOR, std::string{input[i]} });
			EvaluateToken(tokens.back());
			if (tokens.back().lexemeToken == LEXEME_TOKEN_WHITESPACE)
				tokens.pop_back();
			lexingString.clear();
			continue;
		}
		lexingString += input[i];
		if (i != input.size() - 1)
			continue;

		tokens.push_back({ GetToken(lexingString), lexingString });
		EvaluateToken(tokens.back());
	}
	return tokens;
}

float Console::SolveInstruction(Instruction& instruction)
{
	float lvalue = 0, rvalue = 0;
	if (instruction.lvalue.token != LEXER_TOKEN_INVALID)
		lvalue = instruction.lvalue.token == LEXER_TOKEN_LITERAL ? std::stof(instruction.lvalue.content) : GetFloatFromLValue(commandVariables[instruction.lvalue.content]); // if the value is an identifier it should be converted to a float first
	if (instruction.rvalue.token != LEXER_TOKEN_INVALID)
		rvalue = instruction.rvalue.token == LEXER_TOKEN_LITERAL ? std::stof(instruction.rvalue.content) : GetFloatFromLValue(commandVariables[instruction.rvalue.content]);
	LexemeToken op = instruction.op;
	
	return CalculateOperator(lvalue, rvalue, op);
}

Console::VariableMetadata& Console::GetLValue(std::vector<TokenContent> tokens)
{
	std::string optGroupName = "";
	std::string varName;
	for (int i = 0; i < tokens.size(); i++)
	{
		switch (tokens[i].lexemeToken)
		{
		case LEXEME_TOKEN_IDENTIFIER:
			varName = tokens[i].content;
			break;
		case LEXEME_TOKEN_DOT:
			optGroupName = varName;
			break;
		}
	}
	return optGroupName == "" ? commandVariables[varName] : groups[optGroupName].variables[varName];
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

float Console::CalculateOperator(float lvalue, float rvalue, LexemeToken op)
{
	switch (op)
	{
	case LEXEME_TOKEN_PLUSEQUALS:
	case LEXEME_TOKEN_PLUS:     
		return lvalue + rvalue;
	case LEXEME_TOKEN_MINUSEQUALS:
	case LEXEME_TOKEN_MINUS:    
		return lvalue - rvalue;
	case LEXEME_TOKEN_MULTIPLYEQUALS:
	case LEXEME_TOKEN_MULTIPLY: 
		return lvalue * rvalue;
	case LEXEME_TOKEN_DIVIDEEQUALS:
	case LEXEME_TOKEN_DIVIDE:   
		return lvalue / rvalue;
	case LEXEME_TOKEN_IS:
		return (bool)(lvalue == rvalue);
	case LEXEME_TOKEN_ISNOT:
		return (bool)(lvalue != rvalue);
	case LEXEME_TOKEN_ISNOT_SINGLE:
		return !(bool)lvalue;
	case LEXEME_TOKEN_EQUALS:
		return rvalue;
	default: WriteLine("Undefined operator \"" + op + '"', MESSAGE_SEVERITY_ERROR);
	}
	return 0;
}

float Console::CalculateRValue(std::vector<TokenContent> tokens)
{
	if (tokens.size() != 3 && tokens.size() != 1)
	{
		WriteLine("Invalid amount of tokens: the rvalue can only have 3 tokens for a statement", MESSAGE_SEVERITY_ERROR);
		return 0;
	}
	if (tokens.size() == 1)
	{
		return tokens[0].token == LEXER_TOKEN_LITERAL ? std::stof(tokens[0].content) : GetFloatFromLValue(commandVariables[tokens[0].content]);
	}

	Instruction statement = { tokens[1].lexemeToken, tokens[0], tokens[2] };
	float result = SolveInstruction(statement); // always do the calculations with a float, because if the lvalue is an int it can just be rounded into one

	return result;
}

void Console::EvaluateToken(TokenContent& token)
{
	static std::unordered_map<std::string, LexemeToken> stringToToken =
	{
		{ "get", LEXEME_TOKEN_GET }, { "set", LEXEME_TOKEN_SET }, { "enable", LEXEME_TOKEN_ENABLE}, { "disable", LEXEME_TOKEN_DISABLE }, { "define", LEXEME_TOKEN_DEFINE }, { "exit", LEXEME_TOKEN_EXIT },
		{ "-", LEXEME_TOKEN_MINUS }, { "+", LEXEME_TOKEN_PLUS }, { "*", LEXEME_TOKEN_MULTIPLY }, { "/", LEXEME_TOKEN_DIVIDE },
		{ "=", LEXEME_TOKEN_EQUALS }, { "-=", LEXEME_TOKEN_MINUSEQUALS }, { "+=", LEXEME_TOKEN_PLUSEQUALS }, { "*=", LEXEME_TOKEN_MULTIPLYEQUALS }, { "/=", LEXEME_TOKEN_DIVIDEEQUALS }, { "==", LEXEME_TOKEN_IS }, { "!=", LEXEME_TOKEN_ISNOT },
		{ " ", LEXEME_TOKEN_WHITESPACE }, { "", LEXEME_TOKEN_WHITESPACE }, { ";", LEXEME_TOKEN_ENDLINE }, { "(", LEXEME_TOKEN_OPEN_PARANTHESIS }, { ")", LEXEME_TOKEN_CLOSE_PARANTHESIS },
		{ "{", LEXEME_TOKEN_OPEN_CBRACKET }, { "}", LEXEME_TOKEN_CLOSE_CBRACKET }, { "[", LEXEME_TOKEN_OPEN_SBRACKET }, { "]", LEXEME_TOKEN_CLOSE_SBRACKET },
		{ ".", LEXEME_TOKEN_DOT }, { ",", LEXEME_TOKEN_COMMA }, { "\n", LEXEME_TOKEN_NEWLINE }
	};

	switch (token.token)
	{
	case LEXER_TOKEN_IDENTIFIER:
		token.lexemeToken = LEXEME_TOKEN_IDENTIFIER;
		break;
		
	case LEXER_TOKEN_KEYWORD:
	case LEXER_TOKEN_SEPERATOR:
	case LEXER_TOKEN_OPERATOR:
		token.lexemeToken = stringToToken[token.content];
		break;

	case LEXER_TOKEN_LITERAL:
		if (IsDigitLiteral(token.content))
		{
			bool isFloat = token.content.find('.') != std::string::npos;
			token.lexemeToken = isFloat ? LEXEME_TOKEN_FLOAT : LEXEME_TOKEN_INT;
		}
		else
		{
			bool isBool = token.content == "true" || token.content == "false";
			token.lexemeToken = isBool ? LEXEME_TOKEN_BOOL : LEXEME_TOKEN_STRING;
		}
		break;

	case LEXER_TOKEN_WHITESPACE:
		token.lexemeToken = LEXEME_TOKEN_WHITESPACE;
		break;
	}
}

bool Console::IsDigitLiteral(std::string string)
{
	for (int i = 0; i < string.size(); i++) // check if there are any non-digit chars in the string, if there arent its a number literal
	{
		if (i == 0 && string[0] == '-')
			continue;
		if (!std::isdigit((unsigned char)string[i]))
			return false;
	}
	return true;
}

Console::Token Console::GetToken(std::string string)
{
	static std::unordered_map<std::string, Token> stringToToken = 
	{ 
		{ "get", LEXER_TOKEN_KEYWORD }, { "set", LEXER_TOKEN_KEYWORD }, { "enable", LEXER_TOKEN_KEYWORD}, {"disable", LEXER_TOKEN_KEYWORD}, { "define", LEXER_TOKEN_KEYWORD }, { "exit", LEXER_TOKEN_KEYWORD },
		{ "=", LEXER_TOKEN_OPERATOR }, { "-", LEXER_TOKEN_OPERATOR }, { "+", LEXER_TOKEN_OPERATOR }, { "/", LEXER_TOKEN_OPERATOR }, { "*", LEXER_TOKEN_OPERATOR },
		{ "-=", LEXER_TOKEN_OPERATOR }, { "+=", LEXER_TOKEN_OPERATOR }, { "/=", LEXER_TOKEN_OPERATOR }, { "*=", LEXER_TOKEN_OPERATOR }, { "==", LEXER_TOKEN_OPERATOR }, { "!=", LEXER_TOKEN_OPERATOR },
		{ " ", LEXER_TOKEN_SEPERATOR }, { ";", LEXER_TOKEN_SEPERATOR }, { ".", LEXER_TOKEN_SEPERATOR }, { ",", LEXER_TOKEN_SEPERATOR }, { "\n", LEXER_TOKEN_SEPERATOR },
		{ "{", LEXER_TOKEN_SEPERATOR }, { "}", LEXER_TOKEN_SEPERATOR }, { "(", LEXER_TOKEN_SEPERATOR }, { ")", LEXER_TOKEN_SEPERATOR },
	};

	if (string.empty())
	{
		WriteLine("Cannot lex input (string.empty())", MESSAGE_SEVERITY_ERROR);
		return LEXER_TOKEN_WHITESPACE;
	}

	if (string[0] == '"' && string.back() == '"') // literal strings are encased in "'s
		return LEXER_TOKEN_LITERAL;

	if (IsDigitLiteral(string))
		return LEXER_TOKEN_LITERAL;
	
	/*if (stringToToken.count(string) <= 0 && commandVariables.count(string) == 0 && aliases.count(string) == 0)
	{
		WriteLine("Identifier \"" + string + "\" is undefined", MESSAGE_SEVERITY_ERROR);
		return LEXER_TOKEN_WHITESPACE;
	}*/

	return stringToToken.count(string) > 0 ? stringToToken[string] : LEXER_TOKEN_IDENTIFIER; // only identifiers cant be found inside the map
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
	case '.':
	case ',':
	case ';':
	case '(':
	case ')':
	case '{':
	case '}':
	case '\n':
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