export module Templates.Colony;

import std;

import "../../thirdparty/plf_colony/plf_colony.h";

export template<typename T, class allocator_type = std::allocator<T>, plf::priority priority = plf::priority::performance> 
using Colony = plf::colony<T, allocator_type, priority>;