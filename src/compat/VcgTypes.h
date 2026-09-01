#pragma once
#include <vcg/complex/complex.h>

#include <vcg/complex/algorithms/local_optimization/tri_edge_collapse_quadric.h>
#include <vcg/complex/algorithms/local_optimization.h>

class MyVertex;
class MyFace;

struct MyUsedTypes : public vcg::UsedTypes<
	vcg::Use<MyVertex>::AsVertexType,
	vcg::Use<MyFace>::AsFaceType>
{};

class MyVertex : public vcg::Vertex<MyUsedTypes,
	vcg::vertex::VFAdj,
	vcg::vertex::Coord3f,
	vcg::vertex::Normal3f,
	vcg::vertex::TexCoord2f,
	vcg::vertex::Mark,
	vcg::vertex::BitFlags>
{
public:
	vcg::math::Quadric<double>& Qd() { return q; }
private:
	vcg::math::Quadric<double> q;
};
 
 using VertexPair = vcg::tri::BasicVertexPair<MyVertex>;
 
class MyFace : public vcg::Face<MyUsedTypes,
	vcg::face::VFAdj,
	vcg::face::FFAdj,
	vcg::face::VertexRef,
	vcg::face::BitFlags > {};

class MyMesh : public vcg::tri::TriMesh<std::vector<MyVertex>, std::vector<MyFace>> {};

class MyTriEdgeCollapse : public vcg::tri::TriEdgeCollapseQuadric<MyMesh, VertexPair, MyTriEdgeCollapse, vcg::tri::QInfoStandard<MyVertex>>
{
public:
	using TECQ = vcg::tri::TriEdgeCollapseQuadric<MyMesh, VertexPair, MyTriEdgeCollapse, vcg::tri::QInfoStandard<MyVertex>>;
	using EdgeType = MyMesh::VertexType::EdgeType;

	MyTriEdgeCollapse(const VertexPair& p, int i, vcg::BaseParameterClass* pp) : TECQ(p, i, pp) {}
};