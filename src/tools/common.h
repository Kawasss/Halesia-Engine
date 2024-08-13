#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <fstream>

inline std::vector<std::string> SeparateStringByChar(const std::string& input, char separator)
{
	constexpr auto EMPTY_STRING = "";
	std::vector<std::string> ret;
	std::string currentString = EMPTY_STRING;

	for (int i = 0; i < input.size(); i++)                                             // This function checks every char of the string one by one to find the separator char.
	{
		if (input[i] != separator)                                                     // If the current char does not match the separator, then that char gets unto the string of processed chars.
			currentString += input[i];

		else if (!currentString.empty())                                               // If the string of processed chars empty, then the previous instance of the separator is followed up by another one.
		{                                                                              // Multiple separators after each other renders every separator except the first one useless, because separating an empty string does not do anything.
			ret.push_back(currentString);                                              // When the string is not empty, the string gets pushed onto the vector and resets to the process the rest of the input.
			currentString = EMPTY_STRING;
		}
	}
	if (!currentString.empty())                                                        // The string gets separated only when a separator has been found, so if the input does not end on a separator it normally gets ignored.
		ret.push_back(currentString);                                                  // This simply adds the last part of the input to the vector (if it is not empty)
	return ret;
}

inline std::vector<std::string> SeparateStringByString(const std::string& input, const std::string& separator)
{
	constexpr auto EMPTY_STRING = "";
	std::vector<std::string> ret;
	std::string currentString = EMPTY_STRING;
	const size_t separatorSize = separator.size();

	for (int i = 0; i < input.size(); i++)
	{
		currentString += input[i];
		if (currentString.c_str() + currentString.size() - separatorSize != separator) // "currentString.c_str() + currentString.size() - separatorSize" constructs a new string that only has the last part of the string of which the length matches the separator.
			continue;                                                                  // If the separator is "sep" with the example string "I'd like to sep this string", then it will construct the new string "sep" with the same length as the separator.

		currentString.resize(currentString.size() - separatorSize);                    // The separator matches the last 3 chars of the example string, so it will construct another string by resizing the original example string to its size minus the length of the separator.
		ret.push_back(currentString);                                                  // This will result in the new string "example string ", with the separator removed.

		currentString = EMPTY_STRING;	                                               // This string will be pushed onto the vector and reset, after the which the loop continues.
	}

	if (!currentString.empty())
		ret.push_back(currentString);
	return ret;
}