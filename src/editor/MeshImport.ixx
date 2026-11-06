export module Editor.MeshImport;

import std;

import Editor.UI;

export namespace importing
{
	namespace mesh
	{
		struct CreateInfo
		{
			std::string_view file;
			std::uint32_t divisor;
			std::uint32_t finalLodCount;
		};

		class Window : public eui::Window
		{
		public:
			Window();

			bool IsReady() const;

			CreateInfo RetrieveImport();

		private:
			std::string file;

			eui::InputField<std::uint32_t> divisor;
			eui::InputField<std::uint32_t> finalLodCount;

			eui::Button pickFile;
			eui::Button ready;

			bool done = false;

		protected:
			void UpdateSelf() override;
		};
	}
}