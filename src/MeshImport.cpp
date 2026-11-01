module Editor.MeshImport;

namespace importing
{
	namespace mesh
	{
		Window::Window() : eui::Window("import mesh"), 
			divisor("divisor"), finalLodCount("lowest polycount"), ready("import")
		{

		}

		void Window::UpdateSelf()
		{
			divisor.Update();
			finalLodCount.Update();
			ready.Update();

			if (ready.IsPressed())
				Close();
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
				.divisor = divisor.GetValue(),
				.finalLodCount = finalLodCount.GetValue(),
			};
		}
	}
}