local Quaternion = {
	x,
	y,
	z,
	w,
}

function Quaternion:new(x, y, z, w)
	local ret = setmetatable({}, Quaternion)

	ret.x = x or 0
	ret.y = y or 0
	ret.z = z or 0
	ret.w = w or 0

	return ret
end

function Quaternion:FromEulerAngles(vector3)
	local c = vec3.Cos(vector3 * 0.5)
	local s = vec3.Sin(vector3 * 0.5)

	local x = s.x * c.y * c.z - c.x * s.y * s.z
	local y = c.x * s.y * c.z + s.x * c.y * s.z
	local z = c.x * c.y * s.z - s.x * s.y * c.z
	local w = c.x * c.y * c.z + s.x * s.y * s.z

	return Quaternion:new(x, y, z, w)
end

function Quaternion.__mul(p, q)
	local w = p.w * q.w - p.x * q.x - p.y * q.y - p.z * q.z
	local x = p.w * q.x + p.x * q.w + p.y * q.z - p.z * q.y
	local y = p.w * q.y + p.y * q.w + p.z * q.x - p.x * q.z
	local z = p.w * q.z + p.z * q.w + p.x * q.y - p.y * q.x

	return Quaternion:new(x, y, z, w)
end

return Quaternion