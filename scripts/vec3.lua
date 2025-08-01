local vec3 = {
	x,
	y,
	z,
}

function vec3.new(x, y, z)
	local ret = setmetatable({}, vec3)

	ret.x = x or 0
	ret.y = y or 0
	ret.z = z or 0

	return ret
end

function vec3.__add(lhs, rhs)
	if type(rhs) == "number" then
		return vec3.new(lhs.x + rhs, lhs.y + rhs, lhs.z + rhs)
	else
		return vec3.new(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z)
	end
end

function vec3.__sub(lhs, rhs)
	if type(rhs) == "number" then
		return vec3.new(lhs.x - rhs, lhs.y - rhs, lhs.z - rhs)
	else
		return vec3.new(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z)
	end
end

function vec3.__mul(lhs, rhs)
	if type(rhs) == "number" then
		return vec3.new(lhs.x * rhs, lhs.y * rhs, lhs.z * rhs)
	else
		return vec3.new(lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z)
	end
end

function vec3.__div(lhs, rhs)
	if type(rhs) == "number" then
		return vec3.new(lhs.x / rhs, lhs.y / rhs, lhs.z / rhs)
	else
		return vec3.new(lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z)
	end
end

function vec3:Length()
	return math.sqrt(self.x*self.x + self.y*self.y + self.z*self.z)
end

function vec3.Normalize(vec)
	return vec / vec.Length()
end

function vec3.Distance(lhs, rhs)
	return (lhs - rhs):Length()
end

function vec3.Cos(vec)
	return vec3.new(math.cos(vec.x), math.cos(vec.y), math.cos(vec.z))
end

function vec3.Sin(vec)
	return vec3.new(math.sin(vec.x), math.sin(vec.y), math.sin(vec.z))
end

function vec3:ToString()
	return "(" .. self.x .. ", " .. self.y .. ", " .. self.z .. ")"
end

return vec3