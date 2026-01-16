export module IO.SceneWriter;

import std;

import Core.Scene;

export namespace SceneWriter
{
	extern void WriteSceneToArchive(const std::string& file, const Scene* scene);
}