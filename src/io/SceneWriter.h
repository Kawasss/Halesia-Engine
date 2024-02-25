#pragma once
#include <vector>
#include <string>

class Scene;
class Object;

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
	NODE_TYPE_METADATA
};

// These identifiers are represented like this in binary:
//
// NODE_TYPE_NAME:
// 16 bit value corresponding to NodeType
// 64 bit value dictating the size of the name (including the null character)
// the string itself
// 
// NODE_TYPE_ARRAY:
// 16 bit value corresponding to NodeType
// 64 bit value dictating the size of the array in bytes, the type of the array is decided by the implementation
// the elements of the array, the length of which (in bytes) is array_size * sizeof(element)
// 
// NODE_TYPE_OBJECT:
// 16 bit value corresponding to NodeType
// 
// NODE_TYPE_MESH:
// 16 bit value corresponding to NodeType
// 64 bit value dictating the size of the node
// a NODE_TYPE_ARRAY containing the vertices
// a NODE_TYPE_ARRAY containing the indices
// 
// NODE_TYPE_RIGIDBODY:
// 16 bit value corresponding to NodeType
// 
// NODE_TYPE_TRANSFORM:
// 16 bit value corresponding to NodeType
// 
// NODE_TYPE_CAMERA:
// 16 bit value corresponding to NodeType
// 
// NODE_TYPE_TEXTURE:
// 16 bit value corresponding to NodeType
// 
// NODE_TYPE_METADATA:
// 16 bit value corresponding to NodeType

namespace HSFWriter
{
	inline extern void WriteHSFScene(Scene* scene, std::string destination);
	inline extern void WriteObject(Object* object, std::string destination);
}