#include <regex>
#include <iostream>
#include <vector>
#include <mutex>
#include <fstream>
#include <sstream>
#include <time.h>

#include "core/Console.h"
#include "system/Input.h"
#include "tools/common.h"

std::vector<std::string>                                            Console::messages{};
std::unordered_map<std::string, Console::VariableMetadata>          Console::commandVariables{};
std::unordered_map<std::string, MessageSeverity>                    Console::messageColorBinding{};
std::unordered_map<std::string, Console::Group>                     Console::groups{};
std::unordered_map<std::string, std::vector<Console::TokenContent>> Console::aliases{};

std::unordered_map<std::string, Console::VariableType> Console::stringToType = 
{ 
	{ "float", VARIABLE_TYPE_FLOAT }, { "int",    VARIABLE_TYPE_INT    }, 
	{ "bool",  VARIABLE_TYPE_BOOL  }, { "string", VARIABLE_TYPE_STRING } 
};

std::unordered_map<std::string, ConsoleVariableAccess> accessKeywords =
{
	{ "readonly",  CONSOLE_ACCESS_READ_ONLY  },
	{ "writeonly", CONSOLE_ACCESS_WRITE_ONLY }
};

std::unordered_map<std::string, Console::Lexeme> Console::stringToLexeme =
{
	{ "get", LEXEME_GET           }, { "set", LEXEME_SET            }, { "enable", LEXEME_ENABLE         }, { "disable", LEXEME_DISABLE          }, { "define", LEXEME_DEFINE            },
	{ "-",   LEXEME_MINUS         }, { "+",   LEXEME_PLUS           }, { "*",      LEXEME_MULTIPLY       }, { "/",       LEXEME_DIVIDE           }, { "=",      LEXEME_EQUALS            },
	{ "-=",  LEXEME_MINUSEQUALS   }, { "+=",  LEXEME_PLUSEQUALS     }, { "*=",     LEXEME_MULTIPLYEQUALS }, { "/=",      LEXEME_DIVIDEEQUALS     }, { "==",     LEXEME_IS                }, 
	{ " ",   LEXEME_WHITESPACE    }, { "",    LEXEME_WHITESPACE     }, { ";",      LEXEME_ENDLINE        }, { "(",       LEXEME_OPEN_PARANTHESIS }, { ")",      LEXEME_CLOSE_PARANTHESIS },
	{ "{",   LEXEME_OPEN_CBRACKET }, { "}",   LEXEME_CLOSE_CBRACKET }, { "[",      LEXEME_OPEN_SBRACKET  }, { "]",       LEXEME_CLOSE_SBRACKET   }, { "!=",     LEXEME_ISNOT             },
	{ ".",   LEXEME_DOT           }, { ",",   LEXEME_COMMA          }, { "\n",     LEXEME_NEWLINE        }, { "exit",    LEXEME_EXIT             }
};

std::unordered_map<std::string, Console::Token> Console::stringToToken =
{
	{ "get", LEXER_TOKEN_KEYWORD   }, { "set", LEXER_TOKEN_KEYWORD   }, { "enable", LEXER_TOKEN_KEYWORD   }, {"disable", LEXER_TOKEN_KEYWORD   }, { "define", LEXER_TOKEN_KEYWORD   }, { "exit", LEXER_TOKEN_KEYWORD },
	{ "=",   LEXER_TOKEN_OPERATOR  }, { "-",   LEXER_TOKEN_OPERATOR  }, { "+",      LEXER_TOKEN_OPERATOR  }, { "/",      LEXER_TOKEN_OPERATOR  }, { "*",      LEXER_TOKEN_OPERATOR  },
	{ "-=",  LEXER_TOKEN_OPERATOR  }, { "+=",  LEXER_TOKEN_OPERATOR  }, { "/=",     LEXER_TOKEN_OPERATOR  }, { "*=",     LEXER_TOKEN_OPERATOR  }, { "==",     LEXER_TOKEN_OPERATOR  },
	{ " ",   LEXER_TOKEN_SEPERATOR }, { ";",   LEXER_TOKEN_SEPERATOR }, { ".",      LEXER_TOKEN_SEPERATOR }, { ",",      LEXER_TOKEN_SEPERATOR }, { "\n",     LEXER_TOKEN_SEPERATOR },
	{ "{",   LEXER_TOKEN_SEPERATOR }, { "}",   LEXER_TOKEN_SEPERATOR }, { "(",      LEXER_TOKEN_SEPERATOR }, { ")",      LEXER_TOKEN_SEPERATOR }, { "!=",     LEXER_TOKEN_OPERATOR  },
};
// this is defined as a macro so that it can directly exit out of a function
#define CheckForInvalidAccess(metadata, ret)                                                \
if (metadata.location == nullptr)                                                           \
{                                                                                           \
	WriteLine("Cannot write to the given location: it is invalid", MESSAGE_SEVERITY_ERROR); \
	return ret;                                                                             \
}

bool Console::isOpen = false;

inline std::string GetTimeAsString()
{
	
	std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::stringstream stream;
	std::tm ltime;

	localtime_s(&ltime, &t);
	stream << '[' << std::put_time(&ltime, "%H:%M:%S") << "] ";
	return stream.str();
}

inline void WriteMessageWithColor(const std::string& message, MessageSeverity severity)
{
	switch (severity)
	{
	case MESSAGE_SEVERITY_DEBUG:
		std::cout << "\x1B[34m" << message << "\033[0m\n";
		break;
	case MESSAGE_SEVERITY_ERROR:
		std::cout << "\x1B[31m" << message << "\033[0m\n";
		break;
	case MESSAGE_SEVERITY_NORMAL:
		std::cout << message << '\n';
		break;
	case MESSAGE_SEVERITY_WARNING:
		std::cout << "\x1B[33m" << message << "\033[0m\n";
		break;
	}
}

std::mutex writingLinesMutex;
void Console::WriteLine(std::string message, MessageSeverity severity)
{
	std::lock_guard<std::mutex> guard(writingLinesMutex);
	message = GetTimeAsString() + message;

	messages.push_back(message);
	messageColorBinding[message] = severity;

	#ifdef _DEBUG
	//std::string severityString = severity == MESSAGE_SEVERITY_NORMAL ? "" : " (" + MessageSeverityToString(severity) + ")";
	WriteMessageWithColor(message/* + severityString*/, severity);
	#endif
}

void Console::Init()
{
	std::ifstream file("cfg/init.cfg", std::ios::ate | std::ios::binary);
	std::cout << "Initialised console with" << (file.good() ? "" : "out") << " init.cfg" << "\n";
	if (!file.good())
		return;
	InterpretCommand(ReadFile("cfg/init.cfg").data());
}

void Console::WriteConVars()
{
	for (const auto& [name, data] : commandVariables)
		WriteLine(name + ": [ " + variableTypeToString(data.type) + ", " + ConsoleVariableAccessToString(data.access) + " ]");
}

void Console::InterpretCommand(std::string command)
{
	if (command == "help")
	{
		WriteConVars();
		return;
	}
	
	std::string variableName;

	std::vector<TokenContent> tokens = LexInput(command), lValues, rValues;
	InsertAliases(tokens);

	TokenContent equalsOperator{ LEXER_TOKEN_INVALID, "" };
	bool isOnRightSideOfOperator = false;
	for (size_t i = 0; i < tokens.size(); i++)
	{
		TokenContent& content = tokens[i];
		
		if (content.content == " ")                            // any whitespace is skipped to save on processing time
			continue;                                          // whitespaces should already be culled out by LexInput() so this is more of a backup measure
		switch (content.lexemeToken)
		{
		case LEXEME_DISABLE:                                   // disables and enables are just a different way of writing "var = false" or "var = true"
		case LEXEME_ENABLE:                                    // i prefer it because it is easier to type in a console
			*static_cast<bool*>(commandVariables[tokens[i + 1].content].location) = content.lexemeToken == LEXEME_ENABLE;
			break;

		case LEXEME_GET:                                       // a "get" is basically printf() or std::cout <<, but functions are not implemented yet so "get" is used instead
		{                                                      // "get" imo also works better for the one line convenience of a console
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

		case LEXEME_DEFINE:
		{
			std::string aliasName = tokens[i + 1].content;
			
			size_t beginIndex = i;
			for (; i < tokens.size(); i++)                     // keep iterating through the tokens with i until the token to stop a define is found
			{
				if (tokens[i].lexemeToken == LEXEME_NEWLINE)   // defines end when a newline char is found (\n)
					break;                                     // this behavior does not really allow for long defines but that really should not be a problem
			} 
			std::vector<TokenContent> aliasTokens = { tokens.begin() + beginIndex + 2, tokens.begin() + i };
			aliases[aliasName] = aliasTokens;
			std::cout << "Created new alias \"" << aliasName << "\" with a defition of " << aliasTokens.size() << " tokens" << "\n";
			if (i + 1 >= tokens.size())                        // safe exit here since the define is the entire command
				return;                                        // this return causes there to not be any error checking on the contents of the alias
			break;                                             // not checking for errors allows for variables that are not yet declared to be used in an alias
		}

		case LEXEME_ENDLINE:
			DispatchCommand(lValues, rValues, equalsOperator); // a line gets executed when it encounters an "endline" character (;)
			lValues.clear();                                   // everything gets reset after the line has been executed to prepare for a new line
			rValues.clear();                                   // it seems best to execute lines this way, as opposed to executing as it is reading a token
			isOnRightSideOfOperator = false;
			break;

		case LEXEME_EXIT:
			std::cout << "Exit has been requested via console" << "\n";
			exit(0);                                           // this exit for some reason does not close the window which is odd (??)
			                                                   // probably never gonna get fixed since alt + F4 exists anyways
		case LEXEME_EQUALS:
		case LEXEME_PLUSEQUALS:
		case LEXEME_MINUSEQUALS:
		case LEXEME_MULTIPLYEQUALS:
		case LEXEME_DIVIDEEQUALS:                              // operators are mostly used a seperator between the lvalue and rvalue when going over all of the tokens
			equalsOperator = content;                          // it looks quite flimsy but it works for now and should be foolproof as long as the lvalue and rvalue are cleared when a new line begins
			isOnRightSideOfOperator = true;
			continue;
		}
		if (!isOnRightSideOfOperator)
			lValues.push_back(tokens[i]);
		else
			rValues.push_back(tokens[i]);
		if (i == tokens.size() - 1)                            // this will dispatch the very last vector of tokens of the end of the input is reached
			DispatchCommand(lValues, rValues, equalsOperator); // this basically means that lines like "var = 1" can be interpreted correctly
	}                                                          // normally a statement should end with a ';', meaning that it normally should be "var = 1;", but this is not necessary for the last statement
}

void Console::DispatchCommand(std::vector<TokenContent>& lvalues, std::vector<TokenContent>& rvalues, TokenContent& op)
{
	if (lvalues.empty() || rvalues.empty())
	{
		WriteLine("Cannot continue interpreting: no lvalues or rvalues were given", MESSAGE_SEVERITY_WARNING);
		return;
	}

	VariableMetadata& lvalue = GetLValue(lvalues);
	CheckVariableValidity(lvalue, CONSOLE_ACCESS_READ_ONLY);

	if (lvalue.type == VARIABLE_TYPE_STRING)
	{
		AssignRValueToLValue(lvalue, &rvalues[0].content);
		return;
	}
	float resultF = CalculateResult(rvalues);
	float lvalueF = GetFloatFromVariable(lvalue);                  // the result of the line is always assigned the lvalue as a begin value for operators like "+=" and "/=" that use the lvalue in its calculation
	resultF = CalculateOperator(lvalueF, resultF, op.lexemeToken); // the result will be neglected by CalculateOperator() either way if it is not one of those operators so it should not introduce any problems
	int resultI = (int)resultF;
	AssignRValueToLValue(lvalue, lvalue.type == VARIABLE_TYPE_INT || lvalue.type == VARIABLE_TYPE_BOOL ? &resultI : (void*)&resultF);

	#ifdef _DEBUG
	std::cout << "Set variable \"" + lvalues.back().content + "\" to " << GetVariableAsString(lvalue) << " at 0x" << lvalue.location << "\n";
	#endif
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

std::vector<Console::TokenContent> Console::LexInput(std::string input)
{
	std::string lexingString;
	std::vector<TokenContent> tokens;
	bool isCommentary = false;
	for (int i = 0; i < input.size(); i++)
	{
		isCommentary = input[i] == '#' || isCommentary && !(isCommentary && input[i] == '\n'); // commentary is indicated by a '#', like in python, and ends with a new line (\n)
		if (isCommentary)                                                                      // so a char is part of commentary if it is a '#' or after a '#' and before a '\n'
			continue;

		if (IsSeperatorToken(input[i]))                                                        // seperators are handled seperately, because that gives the chance to remove any unnecessary tokens like whitespaces
		{                                                                                      // it also allows for the chance to get a token representing the current string and the lexeme of that
			if (!lexingString.empty())                                                         // an important thing to remember is that IsSeperatorToken() also counts a whitespace as a seperator, i do not know if this will cause any problems
			{
				tokens.push_back({ GetToken(lexingString), lexingString });
				EvaluateToken(tokens.back());
			}
			tokens.push_back({ LEXER_TOKEN_SEPERATOR, std::string{input[i]} });
			EvaluateToken(tokens.back());
			if (tokens.back().lexemeToken == LEXEME_WHITESPACE)                                // ignore any whitespace tokens, because those just clog up the interpreter
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
	float lvalue = GetValueFromTokenContent(instruction.lvalue);
	float rvalue = GetValueFromTokenContent(instruction.rvalue);
	
	return CalculateOperator(lvalue, rvalue, instruction.operatorType);
}

float Console::GetValueFromTokenContent(TokenContent& content)
{
	if (content.token == LEXER_TOKEN_INVALID)
		return 0;
	return content.token == LEXER_TOKEN_LITERAL ? std::stof(content.content) : GetFloatFromVariable(commandVariables[content.content]);
}

Console::VariableMetadata& Console::GetLValue(std::vector<TokenContent> tokens)
{
	std::string optGroupName = "";
	std::string varName;
	for (int i = 0; i < tokens.size(); i++)
	{
		switch (tokens[i].lexemeToken)
		{
		case LEXEME_IDENTIFIER:
			varName = tokens[i].content;
			break;
		case LEXEME_DOT:            // this condition will never be met (for now), since it is reserved
			optGroupName = varName; // it is reserved for when struct-like objects are added and variables can be defined like "struct.var"
			break;                  // the current unused implementation is shallow and only allows a depth of 1
		}
	}
	return optGroupName == "" ? commandVariables[varName] : groups[optGroupName].variables[varName];
}

void Console::InsertAliases(std::vector<TokenContent>& tokens)
{
	for (size_t i = 0; i < tokens.size(); i++) // process all aliases first
	{
		if (aliases.count(tokens[i].content) <= 0)
			continue;

		std::cout << "Caught alias \"" << tokens[i].content << "\": replacing tokens...\n";
		std::vector<TokenContent>& aliasTokens = aliases[tokens[i].content];
		tokens[i] = aliasTokens[0];
		if (aliasTokens.size() > 1)
		{
			tokens.insert(tokens.begin() + i + 1, aliasTokens.begin() + 1, aliasTokens.end());
			i = 0; // the loop has to be reset here because the size of the vector has changed by inserting the aliases
		}          // iterating through a changed vector can be dangerous, so this is mostly done out of safety, not necessity
	}
}

float Console::CalculateOperator(float lvalue, float rvalue, Lexeme op)
{
	switch (op)
	{
	case LEXEME_PLUSEQUALS: // the equals operators like "+=" and "-=" always fall back to their base operator ('+' and '-')
	case LEXEME_PLUS:       // because the function assumes that if one of those operators is used that the base value of the result is already given in lvalue
		return lvalue + rvalue;
	case LEXEME_MINUSEQUALS:
	case LEXEME_MINUS:    
		return lvalue - rvalue;
	case LEXEME_MULTIPLYEQUALS:
	case LEXEME_MULTIPLY: 
		return lvalue * rvalue;
	case LEXEME_DIVIDEEQUALS:
	case LEXEME_DIVIDE:   
		return lvalue / rvalue;
	case LEXEME_IS:
		return (bool)(lvalue == rvalue);
	case LEXEME_ISNOT:
		return (bool)(lvalue != rvalue);
	case LEXEME_ISNOT_SINGLE:
		return !(bool)lvalue;
	case LEXEME_EQUALS:
		return rvalue;
	default: WriteLine("Undefined operator \"" + op + '"', MESSAGE_SEVERITY_ERROR);
	}
	return 0;
}

float Console::CalculateResult(std::vector<TokenContent>& tokens)
{
	std::vector<float> values;
	Lexeme operatorType = LEXEME_INVALID;
	for (int i = 0; i < tokens.size(); i++)
	{
		switch (tokens[i].lexemeToken)
		{
		case LEXEME_OPEN_PARANTHESIS:
		{
			std::vector<TokenContent> contents;
			int parenCount = 1;                                              // this can probably be rewritten better with a for loop, but that can jeopardize readability
			i++;
			while (parenCount > 0 && i < tokens.size())                      // i < tokens.size() is used as a failsafe if there aren't enough closing parentheses 
			{                                                                // if that check isn't made the while loop can go outside the bounds of the string and cause undefined behavior
				if (tokens[i].lexemeToken == LEXEME_OPEN_PARANTHESIS)        // i do not know if this can be done any better, right now it just keeps track if the amount of ( and )'s and it will break if there is an equal amount
					parenCount++;
				else if (tokens[i].lexemeToken == LEXEME_CLOSE_PARANTHESIS)
					parenCount--;
				if (parenCount <= 0)
					break;
				contents.push_back(tokens[i]);
				i++;
			}
			values.push_back(CalculateResult(contents));                     // recursively go through all of the brackets
			break;
		}

		case LEXEME_IDENTIFIER:
			if (tokens[i].content.size() > 2 && tokens[i].content[0] == '!') // this checks for the '!' operator, this is not a good way to do this and should be a temporary fix
			{
				std::string varName = tokens[i].content.c_str() + 1;
				if (commandVariables.count(varName) == 0)                    // this is a silent error catcher and is generally bad behavior, should not be permament
					return 0;
				values.push_back(!(bool)GetFloatFromVariable(commandVariables[varName]));
				break;
			}
			if (commandVariables.count(tokens[i].content) == 0)              // last check before getting any variable to prevent read / write access violation
			{
				WriteLine("Failed to get variable metadata: the variable \"" + tokens[i].content + "\" does not exist", MESSAGE_SEVERITY_ERROR);
				return 0;
			}
			values.push_back(GetFloatFromVariable(commandVariables[tokens[0].content]));
			break;

		case LEXEME_INT:
		case LEXEME_FLOAT:
			values.push_back(std::stof(tokens[i].content));
			break;

		case LEXEME_PLUS:
		case LEXEME_MINUS:
		case LEXEME_MULTIPLY:
		case LEXEME_DIVIDE:
		case LEXEME_IS:
		case LEXEME_ISNOT:
		case LEXEME_ISNOT_SINGLE:
			operatorType = tokens[i].lexemeToken;
			break;
		}
	}
	if (operatorType == LEXEME_INVALID)                                      // if there is no operator it means that there is one value given, so that one is returned
		return values[0];                                                    // a caveat for this approach is that an unrecognized operator is given, which will then not be caught
	if (values.size() == 0)
	{
		WriteLine("Failed to perform an operation: the lvalue is an invalid value", MESSAGE_SEVERITY_ERROR);
		return 0;
	}
	return CalculateOperator(values[0], values[1], operatorType);
}

bool Console::CheckVariableValidity(VariableMetadata& metadata, ConsoleVariableAccess access) // check the validity of a variable in a seperate function to group all of the checks together to make it more organized
{
	if (metadata.access == access)
	{
		WriteLine("Failed to meet access requirements for variable \"" + std::to_string((uint64_t)metadata.location) + "\", it is " + ConsoleVariableAccessToString(metadata.access), MESSAGE_SEVERITY_ERROR);
		return false;
	}
	if (metadata.location == nullptr)
	{
		WriteLine("unknown external identifier: the location of the given variable is nullptr", MESSAGE_SEVERITY_ERROR);
		return false;
	}
	return true;
}

bool Console::IsDigitLiteral(std::string string)
{
	for (int i = 0; i < string.size(); i++)                  // check if there are any non-digit chars in the string, if there arent its a number literal
	{                                                        // i do not know if this counts a '.' or ',' is a digit, so this might not catch a float or double but that should be caught later on anyways
		if (i == 0 && string[0] == '-' && string.size() > 1) // a negative number starts with a '-' which is not a digit, so skip the digit check to pretend that the '-' never existed
			continue;
		if (!std::isdigit((unsigned char)string[i]))
			return false;
	}
	return true;
}

bool Console::IsStringLiteral(std::string string)
{
	return string[0] == '"' && string.back() == '"';         // any string literal starts and ends with a '"', this does not however check if there are any syntaxical errors
}

Console::Token Console::GetToken(std::string string)
{
	if (string.empty())                                      // this check is mostly here to make sure that the function does not return LEXER_TOKEN_IDENTIFIER later on
	{                                                        // it return LEXER_TOKEN_WHITESPACE instead, because whitespaces get filtered later on
		WriteLine("Cannot lex input (string.empty())", MESSAGE_SEVERITY_ERROR);
		return LEXER_TOKEN_WHITESPACE;
	}
	if (IsDigitLiteral(string) || IsStringLiteral(string))
		return LEXER_TOKEN_LITERAL;

	return stringToToken.count(string) > 0 ? stringToToken[string] : LEXER_TOKEN_IDENTIFIER; // only identifiers cant be found inside the map
}

void Console::EvaluateToken(TokenContent& token)
{
	switch (token.token)
	{
	case LEXER_TOKEN_IDENTIFIER:
		token.lexemeToken = LEXEME_IDENTIFIER;
		break;
		
	case LEXER_TOKEN_KEYWORD:
	case LEXER_TOKEN_SEPERATOR:
	case LEXER_TOKEN_OPERATOR:
		token.lexemeToken = stringToLexeme[token.content];
		break;

	case LEXER_TOKEN_LITERAL:
		if (IsDigitLiteral(token.content))
		{
			bool isFloat = token.content.find('.') != std::string::npos; // a float must have a '.' to signify a decimal point, std::string::npos is returned by string::find() if the char cannot be found
			token.lexemeToken = isFloat ? LEXEME_FLOAT : LEXEME_INT;
		}
		else
		{
			bool isBool = token.content == "true" || token.content == "false";
			token.lexemeToken = isBool ? LEXEME_BOOL : LEXEME_STRING;
		}
		break;

	case LEXER_TOKEN_WHITESPACE:
		token.lexemeToken = LEXEME_WHITESPACE;
		break;
	}
}

void Console::AssignRValueToLValue(VariableMetadata& lvalue, void* rvalue) // the rvalue is automatically cast to the type of the lvalue since it assumes that the user uses the correct types for everything, otherwise it can cause for incorrect casting
{                                                                          // wrong casting can result in freezing with for example casting a float pointer to an int pointer (a small int is the same as an incredibly large float)
	CheckForInvalidAccess(lvalue,);
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

void Console::AddLocationValue(float& value, VariableMetadata& metadata)
{
	CheckForInvalidAccess(metadata,);
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

std::string Console::GetVariableAsString(VariableMetadata& metadata)
{
	CheckForInvalidAccess(metadata, "");
	switch (metadata.type)
	{
	case VARIABLE_TYPE_BOOL:   return *static_cast<bool*>(metadata.location) == 1 ? "true" : "false";
	case VARIABLE_TYPE_FLOAT:  return std::to_string(*static_cast<float*>(metadata.location));
	case VARIABLE_TYPE_INT:    return std::to_string(*static_cast<int*>(metadata.location));
	case VARIABLE_TYPE_STRING: return *static_cast<std::string*>(metadata.location);
	}
	return "";
}

float Console::GetFloatFromVariable(VariableMetadata& metadata)
{
	CheckForInvalidAccess(metadata, 0);
	switch (metadata.type)
	{
	case VARIABLE_TYPE_BOOL:  return *static_cast<bool*>(metadata.location);
	case VARIABLE_TYPE_FLOAT: return *static_cast<float*>(metadata.location);
	case VARIABLE_TYPE_INT:   return (float)*static_cast<int*>(metadata.location);
	}
	return 0;
}

bool Console::IsSeperatorToken(char item) // there are is a seperate function to check for seperator, because this is faster then getting the token with GetToken(), since that check for every type
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