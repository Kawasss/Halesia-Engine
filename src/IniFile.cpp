#include <fstream>
#include <list>

#include "io/IniFile.h"
#include "io/IO.h"

namespace ini
{
	Reader::Reader(const std::string& file) // change to string_view
	{
		stream = IO::ReadFile(file, true);
		stream.back() = '\r';

		Parse();
	}

	void Reader::Parse() // the data is parsed by directly modifying the given data, this speeds up parsing dramatically since it removes any allocations: it points to the names in the data directly instead of allocating new strings
	{
		std::string_view data = stream.data();
		std::string_view name, value;

		for (size_t i = 0; i < stream.size(); i++)
		{
			switch (stream[i])
			{
			case '\0':
			case '\r':
			case '\n':
				break;
			case ' ':
				stream[i] = '\0';
				break;
			case '[':
				i = data.find('\n', i);
				break;
			default:
			{
				size_t divider = data.find('=', i);
				size_t endline = data.find('\r', i);

				for (size_t trim = data.find(' ', i); trim < divider; trim = data.find(' ', i))
				{
					stream[trim] = '\0'; // trim away any excess spaces
				}

				stream[divider] = '\0';
				stream[endline] = stream[endline + 1] = '\0'; // override both the \r and the \n (only a windows thing ??)
				
				name  = &stream[i];
				value = &stream[divider + 1];

				variables.emplace(name, value);
				i = endline;
				break;
			}
			}
		}
	}

	bool Reader::GetBool(const std::string_view& name)
	{
		return variables[name] == "1";
	}

	int Reader::GetInt(const std::string_view& name)
	{
		return std::stoi(variables[name].data());
	}

	float Reader::GetFloat(const std::string_view& name)
	{
		return std::stof(variables[name].data());
	}

	std::string_view Reader::operator[](const std::string_view& name)
	{
		return variables[name];
	}

	void Writer::Write()
	{
		std::ofstream stream(dst);

		if (!group.empty())
			stream << '[' << group << "]\n";

		for (const auto& [name, value] : variables)
		{
			stream << name << " = " << value << '\n';
		}
	}

	std::string& Writer::operator[](const std::string& name)
	{
		return variables[name];
	}

	void Writer::SetGroup(const std::string& name)
	{
		group = name;
	}
}