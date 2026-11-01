export module Editor.UI;

import std;

export namespace eui
{
	class NamedItem
	{
	public:
		NamedItem() = default;
		NamedItem(const std::string_view& str);

		void SetName(const std::string_view& str);
		std::string_view GetName() const;
		const char* GetUIName() const;

	private:
		std::string name;
	};

	class Window : public NamedItem
	{
	public:
		Window() = default;
		Window(const std::string_view& str);

		void Open();
		void Close();

		void Update();

		bool IsOpen() const;

	private:
		bool open = false;

	protected:
		virtual void UpdateSelf() {}
	};
}