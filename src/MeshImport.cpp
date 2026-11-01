module Editor.MeshImport;

namespace importing
{
	namespace mesh
	{
		Window::Window() : eui::Window("import mesh")
		{

		}

		void Window::UpdateSelf()
		{

		}

		bool Window::IsReady() const
		{
			return done;
		}

		CreateInfo Window::GetImport()
		{
			return CreateInfo
			{
				.file = file,
				.divisor = divisor,
				.finalLodCount = finalLodCount,
			};
		}
	}
}