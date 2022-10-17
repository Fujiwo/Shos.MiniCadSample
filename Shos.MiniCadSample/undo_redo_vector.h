#pragma once

#include <vector>
#include <algorithm>

template <typename TElement>
class undo_redo_vector
{
	class undo_step
	{
		enum class operation_type
		{
			add	  ,
			remove,
			update
		};
		
		std::vector<TElement>&  collection;
		operation_type			operation;
		size_t					index;
		TElement				element;
		
	public:
		static undo_step* add(std::vector<TElement>& collection, TElement element)
		{
			collection.push_back(element);
			return new undo_step(collection, operation::add, collection.size() - 1);
		}

		static undo_step* remove(std::vector<TElement>& collection, size_t index)
		{
			auto element = collection[index];
			collection.erase(collection.begin() + index);
			return new undo_step(collection, operation::remove, index, element);
		}

		static undo_step* update(std::vector<TElement>& collection, size_t index, TElement element)
		{
			std::swap(element, collection[index]);
			return new undo_step(collection, operation::update, index, element);
		}

		virtual void undo()
		{
			switch (operation) {
				case operation_type::add:
					operation = operation_type::remove;
					element = collection[index];

					collection.erase(collection.begin() + index);
					break;
				case operation_type::remove:
					collection.insert(collection.begin() + index, element);

					operation = operation_type::add;
					break;
				case operation_type::update:
					std::swap(collection[index], element);
					break;
			}
		}

	private:
		undo_step(std::vector<TElement>& collection, operation_type operation, size_t index) : collection(collection), index(index), operation(operation)
		{}

		undo_step(std::vector<TElement>& collection, operation_type operation, size_t index, TElement element) : collection(collection), index(index), operation(operation)
		{}
	};

	class undo_step_group : public undo_step
	{
		std::vector<undo_step*> undo_steps;

	public:
		using iterator = typename std::vector<undo_step>::pointer;

		virtual ~undo_step_group()
		{
			for_each(undo_steps.begin(), undo_steps.end(), [](undo_step* step) { delete step; });
		}

		void push_back(undo_step step)
		{
			undo_steps.push_back(step);
		}

		iterator begin()
		{
			return undo_steps.begin();
		}

		iterator end()
		{
			return undo_steps.end();
		}

		virtual void undo() override
		{
			for_each(undo_steps.begin(), undo_steps.end(), [](undo_step* step) { step->undo(); });
		}
	};

	std::vector<TElement> data;
	
	size_t undo_steps_index;
	std::vector<undo_step*> undo_steps;

public:
	using iterator = typename std::vector<TElement>::pointer;
	
	undo_redo_vector() : undo_steps_index(0)
	{}

	virtual ~undo_redo_vector()
	{
		for_each(undo_steps.begin(), undo_steps.end(), [](undo_step* step) { delete step; });
	}

	size_t size() const
	{
		return data.size();
	}

	iterator begin()
	{
		return data.begin();
	}

	iterator end()
	{
		return data.end();
	}

	//void clear()
	//{
	//	data.clear();
	//}

	void push_back(TElement element)
	{
		auto step = undo_step::add(data, element);
		push(step);
	}

	void erace(iterator iterator)
	{
		auto step = undo_step::remove(data, iterator - data.begin());
		push(step);
	}

	void update(iterator iterator, TElement element)
	{
		auto step = undo_step::update(collection, iterator - data.begin(), element);
		push(step);
	}

	bool undo()
	{
		if (undo_steps_index == 0)
			return false;

		undo_steps[undo_steps_index - 1].undo();
		undo_steps_index--;
		return true;
	}

	bool redo()
	{
		if (undo_steps_index == undo_steps.size())
			return false;

		undo_steps[undo_steps_index].undo();
		undo_steps_index++;
		return true;
	}

	bool can_undo() const
	{
		return undo_steps_index != 0;
	}

	bool can_redo() const
	{
		return undo_steps_index != undo_steps.size();
	}

private:
	void push(const undo_step& step)
	{
		if (undo_steps_index != undo_steps.size()) {
			for_each(undo_steps.begin() + undo_steps_index, undo_steps.end(), [](undo_step* step) { delete step; });
			undo_steps.erase(undo_steps.begin() + undo_steps_index, undo_steps.end());
		}

		undo_steps.push_back(step);
		undo_steps_index++;
	}
};
