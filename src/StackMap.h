#pragma once
#include <initializer_list>
#include <stdexcept>

namespace hstd
{
	template<typename Key, typename Value>
	struct Pair
	{
		Key   first{};
		Value second{};
	};

	template<typename Key, typename Value, size_t size>
	class StackMap // a stack allocated map
	{
	public:
		static_assert(size > 0, "size must be greater than 0");

		using Container = Pair<Key, Value>;

		using Iterator  = Container*;
		using Reference = Value&;

		using ConstIterator  = const Iterator;
		using ConstReference = const Reference;
		
		StackMap() = default;
		StackMap(std::initializer_list<Container> list)
		{
			if (list.size() != size)
				throw std::exception("Cannot initialize a stack map: the given list isnt the correct size");

			for (int i = 0; i < size; i++) // just copy the list into the pairs
				pairs[i] = *(list.begin() + i);
		}

		Reference operator[](const Key& key)
		{
			for (int i = 0; i < size; i++)
			{
				if (pairs[i].first == key)
					return pairs[i].second;
			}
			throw std::exception("Could not find a given key inside the stack map");
		}

		ConstReference operator[](const Key& key) const
		{
			for (int i = 0; i < size; i++)
			{
				if (pairs[i].first == key)
					return pairs[i].second;
			}
			throw std::exception("Could not find a given key inside the stack map");
		}

		Iterator Find(const Key& key) noexcept
		{
			for (int i = 0; i < size; i++)
			{
				if (pairs[i].first == key)
					return pairs + i;
			}
			return end();
		}

		ConstIterator Find(const Key& key) const noexcept
		{
			for (int i = 0; i < size; i++)
			{
				if (pairs[i].first == key)
					return pairs + i;
			}
			return end();
		}

		bool Contains(const Key& key) const noexcept
		{
			return Find(key) != end();
		}

		Iterator begin() noexcept 
		{ 
			return pairs; 
		}

		ConstIterator begin() const noexcept 
		{ 
			return pairs;
		}

		Iterator end() noexcept 
		{ 
			return pairs + size; 
		}

		
		ConstIterator end() const noexcept 
		{ 
			return pairs + size; 
		}

		constexpr size_t GetSize() const noexcept 
		{ 
			return size;
		}

	private:
		Container pairs[size];
	};
}