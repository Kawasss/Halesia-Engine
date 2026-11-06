module Editor.MeshImport;

import System.FileDialog;

namespace importing
{
	namespace mesh
	{
		Window::Window() : eui::Window("import mesh"), 
			divisor("divisor"), finalLodCount("lowest polycount"), ready("import"), pickFile("pick file")
		{

		}

		void Window::UpdateSelf()
		{
			divisor.Update();
			finalLodCount.Update();
			pickFile.Update();
			ready.Update();

			if (pickFile.IsPressed())
			{
				constexpr std::string_view SUPPORTED_FILES = "*.obj;*.glb;*.gltf;*.fbx;*.stl;*.dat;";

				FileDialog::Filter filter;
				filter.description = "file";
				filter.fileType = SUPPORTED_FILES.data();

				file = FileDialog::RequestFile(filter);
			}

			if (ready.IsPressed())
			{
				Close();
				done = true;
			}
		}

		bool Window::IsReady() const
		{
			return done;
		}

		CreateInfo Window::RetrieveImport()
		{
			done = false;
			return CreateInfo
			{
				.file = file,
				.divisor = divisor.GetValue(),
				.finalLodCount = finalLodCount.GetValue(),
			};
		}
	}
}