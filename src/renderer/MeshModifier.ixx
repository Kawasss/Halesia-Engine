export module Renderer.MeshModifier;

import std;

import Renderer.Vertex;

export namespace meshmodifier
{
	using VertexIndexPair = std::tuple<std::vector<Vertex>, std::vector<std::uint32_t>>;

	VertexIndexPair Decimate(const std::span<const Vertex>& vertices, const std::span<const std::uint32_t>& indices, std::size_t finalSize);
}