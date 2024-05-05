#pragma once
template<typename Type>
class UniquePointer
{
public:
	UniquePointer(const Type* ptr) : data(ptr) {}
	~UniquePointer() { delete data; }

	UniquePointer(const UniquePointer<Type>&) = delete; // unique pointers cannot be copied

	Type*& operator->() { return data; }
	Type* Get()         { return data; }

private:
	Type* data = nullptr;
};