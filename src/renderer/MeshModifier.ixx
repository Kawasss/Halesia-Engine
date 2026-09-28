export module Renderer.MeshModifier;

import std;

import Renderer.Vertex;

export namespace meshmodifier
{
	using VertexIndexPair = std::tuple<std::vector<Vertex>, std::vector<std::uint32_t>>;

	void AppendLevelOfDetails(std::vector<Vertex>& vertices, std::vector<std::uint32_t>& indices, std::uint32_t divisor, std::uint32_t count);
}