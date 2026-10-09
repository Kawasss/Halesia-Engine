module;

#include <vcg/complex/complex.h>

#include <vcg/complex/algorithms/local_optimization/tri_edge_collapse_quadric.h>
#include <vcg/complex/algorithms/local_optimization.h>

#include <vcg/complex/algorithms/clean.h>

#include "compat/VcgTypes.h"

module Renderer.MeshModifier;

import std;

import Renderer.Vertex;

import "glm.h";

namespace meshmodifier
{
	template<typename T>
	glm::vec<3, T, glm::defaultp> VcgPointToVec3(const vcg::Point3<T>& p) // TEXTURE COORDINATES NOT YET SUPPORTED
	{
		return glm::vec<3, T, glm::defaultp>(p[0], p[1], p[2]);
	}

	void AppendDecimate(std::vector<Vertex>& vertices, std::vector<std::uint32_t>& indices, std::uint32_t divisor, std::size_t idxStart, std::size_t vertStart)
	{
		MyMesh mesh;

		std::size_t finalSize = indices.size() / divisor;

		auto vit = vcg::tri::Allocator<MyMesh>::AddVertices(mesh, vertices.size() - vertStart);
		auto fit = vcg::tri::Allocator<MyMesh>::AddFaces(mesh, (indices.size() - idxStart) / 3);

		for (std::size_t i = vertStart; i < vertices.size(); i++, vit++)
		{
			const Vertex& src = vertices[i];
			MyVertex& dst = *vit;

			dst.P() = MyVertex::CoordType(src.position.x, src.position.y, src.position.z);
			dst.N() = MyVertex::NormalType(src.normal.x, src.normal.y, src.normal.z);
			dst.T() = MyVertex::TexCoordType(src.textureCoordinates.x, src.textureCoordinates.y);

			dst.SetW();
		}

		for (std::size_t i = idxStart; i < indices.size(); i += 3, fit++)
		{
			MyFace& f = *fit;

			f.V(0) = &mesh.vert[indices[i]];
			f.V(1) = &mesh.vert[indices[i + 1]];
			f.V(2) = &mesh.vert[indices[i + 2]];

			f.SetW();
		}

		vcg::tri::Clean<MyMesh>::RemoveDuplicateVertex(mesh);
		vcg::tri::Clean<MyMesh>::RemoveUnreferencedVertex(mesh);

		//vcg::tri::UpdateBounding<MyMesh>::Box(mesh);
		//vcg::tri::UpdateTopology<MyMesh>::VertexFace(mesh);
		//vcg::tri::UpdateTopology<MyMesh>::FaceFace(mesh);
		vcg::tri::UpdateFlags<MyMesh>::FaceBorderFromFF(mesh);
		vcg::tri::UpdateFlags<MyMesh>::VertexBorderFromFaceAdj(mesh);

		vcg::tri::TriEdgeCollapseQuadricParameter params;
		params.QualityThr = .0f;
		params.OptimalPlacement = true;
		params.PreserveTopology = true;
		params.PreserveBoundary = false;
		params.QualityCheck = false;
		params.NormalCheck = true;
		params.QualityQuadric = false;
		params.UseArea = true;

		vcg::LocalOptimization<MyMesh> deciSession(mesh, &params);

		deciSession.Init<MyTriEdgeCollapse>();
		deciSession.SetTargetSimplices(finalSize);
		deciSession.SetTimeBudget(120.0f);
		deciSession.SetTargetOperations(100000);

		std::cout << "heap: " << deciSession.h.size() << '\n';

		std::cout << "faces: " << mesh.fn << '\n';

		while (mesh.fn > finalSize && deciSession.DoOptimization());

		deciSession.Finalize<MyTriEdgeCollapse>();

		std::cout << "faces: " << mesh.fn << '\n';

		vcg::tri::Allocator<MyMesh>::CompactFaceVector(mesh);
		vcg::tri::Allocator<MyMesh>::CompactVertexVector(mesh);
		vcg::tri::UpdateTopology<MyMesh>::VertexFace(mesh);

		assert(mesh.fn == finalSize);

		std::size_t vertOffset = vertices.size();
		std::size_t idxOffset  = indices.size();

		vertices.resize(vertOffset + mesh.vn);
		indices.resize(idxOffset + mesh.fn * 3);

		for (std::size_t i = 0; i < mesh.vert.size(); i++)
		{
			Vertex& dst = vertices[vertOffset + i];
			const MyVertex& src = mesh.vert[i];

			dst.position = VcgPointToVec3(src.P());
			dst.normal = glm::normalize(glm::abs(VcgPointToVec3(src.N())));
		}

		for (std::size_t i = 0; i < mesh.face.size(); i++)
		{
			const MyFace& src = mesh.face[i];
			const MyVertex* start = &(*mesh.vert.begin());

			indices[idxOffset + i * 3]     = static_cast<std::uint32_t>(src.V(0) - start);
			indices[idxOffset + i * 3 + 1] = static_cast<std::uint32_t>(src.V(1) - start);
			indices[idxOffset + i * 3 + 2] = static_cast<std::uint32_t>(src.V(2) - start);
		}
	}

	void AppendLevelOfDetails(std::vector<Vertex>& vertices, std::vector<std::uint32_t>& indices, std::uint32_t divisor, std::uint32_t count)
	{
		std::size_t idxStart = 0;
		std::size_t vertStart = 0;

		for (std::uint32_t i = 0; i < count; i++)
		{
			std::size_t newIdxStart = indices.size();
			std::size_t newVertStart = vertices.size();

			AppendDecimate(vertices, indices, divisor, idxStart, vertStart);

			idxStart = newIdxStart;
			vertStart = newVertStart;
		}
	}
}