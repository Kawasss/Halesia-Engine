#pragma once
#include <cstdint>

// the idea behind the file format is that it consists of a series of nodes where each node start like this:
//
// 16 bit unsigned integer identifier -> 64 bit unsigned integer size of node -> "size of node" amount of bytes
//
// the identifier is best represented by an enum when reading or writing from the file.
// nodes can contain other nodes but will need be immediately read, but may be read recursively when the enclosing node is fully read.
// an "object" node can contain a "mesh" node: in this case the "object" node will be retrieved first after which it can read the nodes inside and then the nodes inside those nodes.
//
// all identifiers are listed here:

typedef uint64_t NodeSize;
enum NodeType : uint16_t
{
	NODE_TYPE_NONE,
	NODE_TYPE_OBJECT,
	NODE_TYPE_NAME,
	NODE_TYPE_MESH,
	NODE_TYPE_RIGIDBODY,
	NODE_TYPE_TRANSFORM,
	NODE_TYPE_CAMERA,
	NODE_TYPE_ARRAY,
	NODE_TYPE_TEXTURE,
	NODE_TYPE_MATERIAL,
	NODE_TYPE_METADATA,
	NODE_TYPE_VERTICES,
	NODE_TYPE_INDICES,
	NODE_TYPE_ALBEDO,
	NODE_TYPE_NORMAL,
	NODE_TYPE_ROUGHNESS,
	NODE_TYPE_METALLIC,
	NODE_TYPE_AMBIENT_OCCLUSION,
	NODE_TYPE_COMPRESSION,
};
inline extern const char* NodeTypeToString(NodeType type);

// These identifiers are represented like this in binary:
//
// NODE_TYPE_NAME:
// 16 bit value corresponding to NodeType
// 64 bit value dictating the size of the name (including the null character)
// the string itself, whose length is given by the previous value.
// 
// NODE_TYPE_ARRAY:
// 16 bit value corresponding to NodeType
// 64 bit value dictating the size of the array in bytes, the type of the array is decided by the implementation
// the elements of the array, the length of which (in bytes) is array_size * sizeof(element)
// 
// NODE_TYPE_OBJECT:
// 16 bit value corresponding to NodeType
// 64 bit value containing the size of the node
// an 8 bit unsigned integer representing the objects state from the enum "ObjectState"
// name node
// a transform node  (NODE_TYPE_TRANSFORM)
// a rigid body node (NODE_TYPE_RIGIDBODY)
// a mesh node       (NODE_TYPE_MESH)
// 
// NODE_TYPE_MESH:
// 16 bit value corresponding to NodeType
// 64 bit value containing the size of the node
// a 32 bit unsigned integer representing the material index
// a vertices node (NODE_TYPE_VERTICES)
// an indices node (NODE_TYPE_INDICES)
// 
// NODE_TYPE_VERTICES:
// 16 bit value corresponding to NodeType
// 64 bit value containing the size of the node
// an array of vertices
// each vertex can be copied directly into the "Vertex" struct.
// 
// NODE_TYPE_INDICES:
// 16 bit value corresponding to NodeType
// 64 bit value containing the size of the node
// an array of 8 bit unsigned integers representing indices
// 
// NODE_TYPE_RIGIDBODY:
// 16 bit value corresponding to NodeType
// 64 bit value containing the size of the node
// an 8 bit unsigned integer from the "RigidBodyType" enum
// an 8 bit unsigned integer from the "ShapeType" enum
// a vec3 containing the data of the shape
// 
// NODE_TYPE_TRANSFORM:
// 16 bit value corresponding to NodeType
// 64 bit value containing the size of the node
// a vec3 containing the position
// a vec3 containing the rotation
// a vec3 containing the scale
// the size of a NODE_TYPE_TRANSFORM is constant.
// 
// NODE_TYPE_CAMERA:
// 16 bit value corresponding to NodeType
// 64 bit value containing the size of the node
// 
// NODE_TYPE_TEXTURE:
// 16 bit value corresponding to NodeType
// 64 bit value containing the size of the node
// 32 bit value determining the width
// 32 bit value determining the height
// an array of which the size is determined by the node size minus the 64 bits used by the dimensions
// the texture data itself is uncompressed
// NODE_TYPE_ALBEDO, NODE_TYPE_NORMAL, NODE_TYPE_ROUGHNESS, NODE_TYPE_METALLIC and NODE_TYPE_AMBIENT_OCCLUSION are specialised versions of this node
// 
// NODE_TYPE_MATERIAL:
// 16 bit value corresponding to NodeType
// 64 bit value containing the size of the node
// a bool to indicate of the material is a light source or not
// albedo texture node            (NODE_TYPE_TEXTURE)
// normal texture node            (NODE_TYPE_TEXTURE)
// roughness texture node         (NODE_TYPE_TEXTURE)
// metallic texture node          (NODE_TYPE_TEXTURE)
// ambient occlusion texture node (NODE_TYPE_TEXTURE)
// 
// NODE_TYPE_METADATA:
// 16 bit value corresponding to NodeType
// 64 bit value containing the size of the node
// 
// NODE_TYPE_COMPRESSION:
// 16 bit value corresponding to NodeType
// 64 bit value containing the size of the node
// 64 bit value representing the uncompressed size
// 32 bit unsigned integer representing the compression algorithm used
// this node is ALWAYS at the start of the file
//