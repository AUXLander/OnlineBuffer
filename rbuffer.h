#pragma once

template<class T>
class rbuffer
{
	template<class T>
	struct rbuffer_node
	{
		T* object;

		rbuffer_node<T>* next;
		rbuffer_node<T>* back;

		template<typename... Args>
		rbuffer_node(Args... args)
		{
			object = new T(args...);
			next = nullptr;
			back = nullptr;
		}

		~rbuffer_node()
		{
			delete object;
		}
	};

	rbuffer_node<T>* first;
	rbuffer_node<T>* last;

	size_t m_size;
	size_t m_allocated;

public:

	rbuffer(size_t size) : m_size(size), m_allocated(0)
	{
		first = nullptr;
		last  = nullptr;
	}


	template<typename... Args>
	rbuffer_node<T>* emplace(Args... args)
	{
		rbuffer_node<T>* node = nullptr;

		if (m_allocated < m_size)
		{
			node = new rbuffer_node<T>(args...);
			m_allocated++;

			if (last == nullptr || first == nullptr)
			{
				first = last = node;
			}

			first->back = node;
			node->next = first;

			last->next = node;
			node->back = last;

			last = last->next;
		}

		return node;
	}


	void erase(rbuffer_node<T>* node)
	{
		if (node != nullptr)
		{
			auto back = node->back;
			auto next = node->next;

			back->next = next;
			next->back = back;

			if (node == first)
			{
				first = node->next;
			}
			else if (node == last)
			{
				last = node->back;
			}

			delete node;

			m_allocated--;
		}
	}


	rbuffer_node<T>* pFirst()
	{
		return first;
	}


	rbuffer_node<T>* pLast()
	{
		return last;
	}


	size_t size()
	{
		return m_allocated;
	}
};