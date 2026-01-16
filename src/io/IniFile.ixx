export module IO.IniFile;

import std;

export namespace ini
{
	class Reader
	{
	public:
		Reader(const std::string& file);
		std::string_view operator[](const std::string_view& name);

		bool Succeeded() { return success; }

		bool  GetBool(const std::string_view& name);
		int   GetInt(const std::string_view& name);
		float GetFloat(const std::string_view& name);

	private:
		void Parse();

		bool success = false;
		std::vector<char> stream;
		std::map<std::string_view, std::string_view> variables; // name -> value
	};

	class Writer
	{
	public:
		Writer(const std::string& file) : dst(file) {}

		void SetGroup(const std::string& name);

		void Write();

		std::string& operator[](const std::string& name);

	private:
		std::string dst, group;
		std::map<std::string, std::string> variables;
	};
}