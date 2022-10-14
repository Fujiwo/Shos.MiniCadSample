#pragma once

#include <vector>

template <typename TElement>
class undo_redo_vector
{
	vector<TElement> data;
	
public:
	using iterator = vector<TElement>::iterator;
	
	undo_redo_vector()
	{}

	iterator begin()
	{
		return data.begin();
	}

	iterator end()
	{
		return data.end();
	}

	void clear()
	{
		data.clear();
	}

	void push_back(const TElement& element)
	{
		data.push_back(element);
	}

	void erace(Iterator iterator)
	{
		data.erace(iterator);
	}

	void update(Iterator iterator, const TElement& element)
	{
		*iterator = element;
	}

	bool Undo()
	{
		
	}

	bool Redo()
	{
		
	}
};
