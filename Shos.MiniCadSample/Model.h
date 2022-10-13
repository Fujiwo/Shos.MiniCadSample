#pragma once

#include <afx.h>
#include "Observer.h"
#include "Figure.h"

struct Hint : public CObject
{
	enum class Type
	{
		Added   ,
		Removed ,
		Changed ,
		ViewOnly
	};

	std::vector<Figure*> figures;
	Type				 type;

	Hint(Type type, Figure* figure) : type(type)
	{
		figures.push_back(figure);
	}

	Hint(Type type, std::vector<Figure*> figures) : type(type), figures(figures)
	{}
};

class Model : public Observable<Hint>
{
	static const LONG size = 2000L;

	std::vector<Figure*>   figures;
	const Figure* highlightedFigure;

public:
	using iterator = std::vector<Figure*>::const_iterator;

	const CSize GetSize() const { return CSize(size, size); }
	const CRect GetArea() const { return CRect(CPoint(), GetSize()); }

	Model() : highlightedFigure(nullptr)
	{}

	virtual ~Model()
	{
		Clear();
	}

	iterator begin() const
	{
		return figures.begin();
	}

	iterator end() const
	{
		return figures.end();
	}

	void Add(Figure* figure)
	{
		figures.push_back(figure);
		Update(Hint(Hint::Type::Added, figure));
	}

	bool Change(Figure* oldFigure, Figure* newFigure)
	{
		auto iterator = std::find(figures.begin(), figures.end(), oldFigure);
		if (iterator == figures.end())
			return false;

		*iterator = newFigure;
		std::vector<Figure*> changedFigures = { oldFigure, newFigure };
		Update(Hint(Hint::Type::Changed, changedFigures));
		return true;
	}

	void Select(Figure& figure)
	{
		ClearSelected();
		figure.Select(true);
		Update(Hint(Hint::Type::ViewOnly, std::vector<Figure*>()));
	}

	void Hilight(const Figure* figure)
	{
		highlightedFigure = figure;
	}

	const Figure* Hilight() const
	{
		return highlightedFigure;
	}

	void UnSelectAll()
	{
		ClearSelected();
		Update(Hint(Hint::Type::ViewOnly, std::vector<Figure*>()));
	}

	virtual void Serialize(CArchive& ar)
	{
		if (ar.IsStoring()) {
			ar.WriteCount(figures.size());
			for (auto figure : *this)
				ar.WriteObject(figure);
		}
		else
		{
			auto count = ar.ReadCount();
			for (DWORD_PTR counter = 0L; counter < count; counter++) {
				auto figure = STATIC_DOWNCAST(Figure, ar.ReadObject(NULL));
				if (figure != nullptr)
					figures.push_back(figure);
			}
		}
	}

	void Clear()
	{
		for (auto figure : *this)
			delete figure;
		figures.clear();
		highlightedFigure = nullptr;
	}

	void AddDummyData(size_t count)
	{
		auto newFigures = FigureHelper::GetRandomFigures(count, GetArea());
		std::for_each(newFigures.begin(), newFigures.end(), [&](Figure* figure) { figures.push_back(figure); });
		
		//for (auto figure : newFigures)
		//	figures.push_back(figure);

		Update(Hint(Hint::Type::Added, newFigures));
	}

private:
	void ClearSelected()
	{
		std::for_each(figures.begin(), figures.end(), [](Figure* figure) { figure->Select(false); });
	}
};
