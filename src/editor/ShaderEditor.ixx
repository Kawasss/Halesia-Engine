export module Editor.ShaderEditor;

import std;

import Core.Scene;

import System.Window;

import Editor.ShaderNodes;

export namespace editor
{
	class ShaderEditor : public Scene
	{
	public:
		void Start() override;
		void Update(float delta) override;
		void UpdateGUI(float delta) override;
		void MainThreadUpdate(float delta) override;

		void Destroy() override;

		

	private:
		Window* window;

		std::list<std::unique_ptr<BasicNode>> nodes; // there may be a better structure for this, but for now this works.
		std::unordered_set<Link> links;
	};
}