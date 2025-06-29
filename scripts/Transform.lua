vec3 = require 'vec3'
Quaternion = require 'Quaternion'

local Transform = {
	position,
	scale,
	rotation,
}
Transform.__index = Transform

function Transform:new(position, rotation, scale)
	local ret = setmetatable({}, Transform)
	
	ret.position = position or vec3:new()
	ret.rotation = rotation or Quaternion:new()
	ret.scale    = scale    or vec3:new()

	return ret
end

return Transform