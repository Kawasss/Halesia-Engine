export module Octree;

import "../../thirdparty/octree-cpp/include/octree-cpp/OctreeCpp.h";

import "../glm.h";

export template<typename Point>
using Octree = OctreeCpp<glm::vec3, Point>;