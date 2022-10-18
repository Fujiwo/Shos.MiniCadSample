#pragma once

#include <vector>
#include <algorithm>

template <typename TElement>
class undo_redo_vector
{
	class undo_step
	{
	protected:
		enum class operation_type
		{
			add	  ,
			remove,
			update,
			group
		};

	private:
		std::vector<TElement>&  collection;
		operation_type			operation;
		size_t					index;
		TElement				element;
		
	public:
		static undo_step* add(std::vector<TElement>& collection, TElement element)
		{
			collection.push_back(element);
			return new undo_step(collection, operation_type::add, collection.size() - 1);
		}

		static undo_step* remove(std::vector<TElement>& collection, size_t index)
		{
			auto element = collection[index];
			collection.erase(collection.begin() + index);
			return new undo_step(collection, operation_type::remove, index, element);
		}

		static undo_step* update(std::vector<TElement>& collection, size_t index, TElement element)
		{
			std::swap(element, collection[index]);
			return new undo_step(collection, operation_type::update, index, element);
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

		virtual void redo()
		{
			undo();
		}

	protected:
		undo_step(std::vector<TElement>& collection, operation_type operation) : collection(collection), operation(operation), index(0), element()
		{}

	private:
		undo_step(std::vector<TElement>& collection, operation_type operation, size_t index) : collection(collection), operation(operation), index(index), element()
		{}

		undo_step(std::vector<TElement>& collection, operation_type operation, size_t index, TElement element) : collection(collection), operation(operation), index(index), element(element)
		{}
	};

	class undo_step_group : public undo_step
	{
		std::vector<undo_step*> undo_steps;

	public:
		using iterator = typename std::vector<undo_step*>::pointer;

		undo_step_group(std::vector<TElement>& collection) : undo_step(collection, operation_type::group)
		{}
		
		virtual ~undo_step_group()
		{
			for_each(undo_steps.begin(), undo_steps.end(), [](undo_step* step) { delete step; });
		}

		size_t size() const
		{
			return undo_steps.size();
		}

		void push_back(undo_step* step)
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
			for_each(undo_steps.rbegin(), undo_steps.rend(), [](undo_step* step) { step->undo(); });
		}

		virtual void redo() override
		{
			for_each(undo_steps.begin(), undo_steps.end(), [](undo_step* step) { step->undo(); });
		}
	};

	std::vector<TElement>   data;
	
	size_t					undo_steps_index;
	std::vector<undo_step*> undo_steps;
	undo_step_group*		current_undo_step_group;

public:
	using iterator = typename std::vector<TElement>::iterator;
	
	undo_redo_vector() : undo_steps_index(0), current_undo_step_group(nullptr)
	{}

	virtual ~undo_redo_vector()
	{
		for_each(undo_steps.begin(), undo_steps.end(), [](undo_step* step) { delete step; });
		delete current_undo_step_group;
	}

	const TElement& operator[](size_t index) const
	{
		return data[index];
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

	void clear()
	{
		transaction transaction(*this);
		while (data.size() > 0)
			erase(begin());
	}

	void push_back(TElement element)
	{
		auto step = undo_step::add(data, element);
		push(step);
	}

	void erase(iterator iterator)
	{
		auto step = undo_step::remove(data, iterator - data.begin());
		push(step);
	}

	void update(iterator iterator, TElement element)
	{
		auto step = undo_step::update(data, iterator - data.begin(), element);
		push(step);
	}

	bool undo()
	{
		if (undo_steps_index == 0)
			return false;

		undo_steps[undo_steps_index - 1]->undo();
		undo_steps_index--;
		return true;
	}

	bool redo()
	{
		if (undo_steps_index == undo_steps.size())
			return false;

		undo_steps[undo_steps_index]->redo();
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

	class transaction
	{
		undo_redo_vector<TElement>& collection;
		
	public:
		transaction(undo_redo_vector<TElement>& collection) : collection(collection)
		{
			collection.begin_transaction();
		}

		virtual ~transaction()
		{
			collection.end_transaction();
		}
	};
	
private:
	void begin_transaction()
	{
		if (current_undo_step_group != nullptr)
			throw std::exception();

		current_undo_step_group = new undo_step_group(data);
	}

	void end_transaction()
	{
		if (current_undo_step_group == nullptr)
			throw std::exception();

		if (current_undo_step_group->size() == 0)
			delete current_undo_step_group;
		else
			push_to_steps(current_undo_step_group);
		current_undo_step_group = nullptr;
	}
	
	void push(undo_step* step)
	{
		if (current_undo_step_group == nullptr)
			push_to_steps(step);
		else
			push_to_group(step);
	}

	void push_to_steps(undo_step* step)
	{
		if (undo_steps_index != undo_steps.size()) {
			for_each(undo_steps.begin() + undo_steps_index, undo_steps.end(), [](undo_step* step) { delete step; });
			undo_steps.erase(undo_steps.begin() + undo_steps_index, undo_steps.end());
		}

		undo_steps.push_back(step);
		undo_steps_index++;
	}

	void push_to_group(undo_step* step)
	{
		if (current_undo_step_group == nullptr)
			throw std::exception();

		current_undo_step_group->push_back(step);
	}
};
