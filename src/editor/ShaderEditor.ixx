export module Editor.ShaderEditor;

import Core.Scene;

export namespace editor
{
	class ShaderEditor : public Scene
	{
	public:
		void Start() override;
		void Update(float delta) override;
		void UpdateGUI(float delta) override;
		void MainThreadUpdate(float delta) override;

		~ShaderEditor();

	private:
		struct Node
		{
			std::string_view name;
		};
	};
}