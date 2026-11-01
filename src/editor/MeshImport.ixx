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

			CreateInfo GetImport();

		private:
			std::string file;
			std::uint32_t divisor;
			std::uint32_t finalLodCount;

			bool done = false;

		protected:
			void UpdateSelf() override;
		};
	}
}