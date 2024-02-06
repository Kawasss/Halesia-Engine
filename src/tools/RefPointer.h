#pragma once
#include <utility>
#include <unordered_map>

template<typename Type>
class RefPointer
{
public:
	RefPointer() = default;
	RefPointer(Type* ptr);
	RefPointer(const RefPointer<Type>& rhs);
	~RefPointer();

	Type* Get() { return value; }
	Type* operator->() { return value; }
	Type& operator*() { return *value; }

	template<typename... Args>
	static RefPointer<Type> Create(Args&&... args);

private:
	Type* value = nullptr;
	static std::unordered_map<Type*, size_t> counter;
};
template<typename Type>
std::unordered_map<Type*, size_t> RefPointer<Type>::counter;

template<typename Type>
RefPointer<Type>::RefPointer(Type* ptr) : value(ptr)
{
	if (counter.find(ptr) == counter.end())
		counter[ptr] = 0;
}

template<typename Type>
RefPointer<Type>::RefPointer(const RefPointer<Type>& rhs) : value(rhs.value)
{
	counter[value]++;
}

template<typename Type>
RefPointer<Type>::~RefPointer()
{
	if (counter[value] <= 1)
	{
		counter.erase(value);
		delete value;
		return;
	}
	counter[value]--;
}

template<typename Type>
template<typename... Args>
RefPointer<Type> RefPointer<Type>::Create(Args&&... args)
{
	return RefPointer(new Type(std::forward<Args>(args)...));
}