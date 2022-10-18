#pragma once

#include <afx.h>
#include "Observer.h"
#include "Figure.h"
#include "Application.h"
#include "undo_redo_vector.h"

struct Hint : public CObject
{
	enum class Type
	{
		Added   ,
		Removed ,
		Changed ,
		All		,
		ViewOnly
	};

	std::vector<Figure*> figures;
	Type				 type;

	Hint(Type type) : type(type)
	{}

	Hint(Type type, Figure* figure) : type(type)
	{
		figures.push_back(figure);
	}

	Hint(Type type, std::vector<Figure*> figures) : type(type), figures(figures)
	{}
};

class Model : public Observable<Hint>, public Observer<FigureAttribute>
{
	static const LONG size = 2000L;

	//std::vector<Figure*>   figures;
	undo_redo_vector<Figure*>   figures;
	const Figure* highlightedFigure;

	FigureAttribute currentFigureAttribute;
	FigureAttribute* selectedFigureAttribute;

public:
	//using iterator = std::vector<Figure*>::const_iterator;
	using iterator = undo_redo_vector<Figure*>::const_iterator;

	const CSize GetSize() const { return CSize(size, size); }
	const CRect GetArea() const { return CRect(CPoint(), GetSize()); }

	FigureAttribute& GetCurrentFigureAttribute()
	{
		return currentFigureAttribute;
	}

	void SetSelectedFigureAttribute(FigureAttribute& figureAttribute)
	{
		if (selectedFigureAttribute != nullptr)
			selectedFigureAttribute->RemoveObserver(*this);

		selectedFigureAttribute = &figureAttribute;
		figureAttribute.AddObserver(*this);
		Application::Set(figureAttribute);
	}

	void ResetSelectedFigureAttribute()
	{
		if (selectedFigureAttribute != nullptr)
			selectedFigureAttribute->RemoveObserver(*this);

		selectedFigureAttribute = nullptr;
		Application::Set(currentFigureAttribute);
	}
	
	virtual void Update(FigureAttribute& hint) override
	{
		NotifyObservers(Hint(Hint::Type::ViewOnly));
	}

	Model() : highlightedFigure(nullptr), selectedFigureAttribute(nullptr)
	{}

	virtual ~Model()
	{
		Clear();
	}

	iterator begin() const
	{
		return figures.cbegin();
	}

	iterator end() const
	{
		return figures.cend();
	}

	void Add(Figure* figure)
	{
		ASSERT_VALID(figure);
		figure->Attribute() = GetCurrentFigureAttribute();
		figures.push_back(figure);
		NotifyObservers(Hint(Hint::Type::Added, figure));
	}

	bool Change(Figure* oldFigure, Figure* newFigure)
	{
		auto iterator = std::find(figures.begin(), figures.end(), oldFigure);
		if (iterator == figures.end())
			return false;

		*iterator = newFigure;
		std::vector<Figure*> changedFigures = { oldFigure, newFigure };
		NotifyObservers(Hint(Hint::Type::Changed, changedFigures));
		return true;
	}

	void Select(Figure& figure)
	{
		ClearSelected();
		figure.Select(true);
		NotifyObservers(Hint(Hint::Type::ViewOnly));
	}

	void Undo()
	{
		if (figures.undo())
			NotifyObservers(Hint(Hint::Type::All));
	}

	bool CanUndo() const
	{
		return figures.can_undo();
	}

	void Redo()
	{
		if (figures.redo())
			NotifyObservers(Hint(Hint::Type::All));
	}

	bool CanRedo() const
	{
		return figures.can_redo();
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
		NotifyObservers(Hint(Hint::Type::ViewOnly));
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
		std::for_each(newFigures.cbegin(), newFigures.cend(), [&](Figure* figure) { figures.push_back(figure); });
		NotifyObservers(Hint(Hint::Type::Added, newFigures));
	}

private:
	void ClearSelected()
	{
		std::for_each(figures.cbegin(), figures.cend(), [](Figure* figure) { figure->Select(false); });
	}
};
