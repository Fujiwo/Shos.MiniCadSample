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

	undo_redo_vector<Figure*>   figures;
	const Figure* highlightedFigure;

	FigureAttribute currentFigureAttribute;

public:
	using iterator = undo_redo_vector<Figure*>::const_iterator;

	const CSize GetSize() const { return CSize(size, size); }
	const CRect GetArea() const { return CRect(CPoint(), GetSize()); }

	Model() : highlightedFigure(nullptr)
	{}

	virtual ~Model()
	{
		Reset();
	}

	FigureAttribute& GetCurrentFigureAttribute()
	{
		return currentFigureAttribute;
	}

	virtual void Update(const FigureAttribute& hint) override
	{
		auto selectedFigures = GetSelectedFigures();
		if (selectedFigures.size() == 0) {
			currentFigureAttribute = hint;
			NotifyObservers(Hint(Hint::Type::ViewOnly));
		} else {
			Update(selectedFigures, hint);
		}
	}

	void Update(std::vector<Figure*> selectedFigures, const FigureAttribute& figureAttribute)
	{
		undo_redo_vector<Figure*>::transaction transaction(figures);
		for (auto figure : selectedFigures) {
			auto updatedFigure = figure->Clone();
			updatedFigure->Attribute() = figureAttribute;
			Update(*figure, *updatedFigure);
		}
		NotifyObservers(Hint(Hint::Type::Changed, selectedFigures));
	}

	void Update(Figure& oldFigure, Figure& newFigure)
	{
		auto iterator = std::find(figures.cbegin(), figures.cend(), &oldFigure);
		if (iterator != figures.cend())
			figures.update(iterator, &newFigure);
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
		figure->Attribute() = currentFigureAttribute;
		figures.push_back(figure);
		NotifyObservers(Hint(Hint::Type::Added, figure));
	}

	void Remove(Figure* figure)
	{
		ASSERT_VALID(figure);

		auto iterator = std::find(figures.begin(), figures.end(), figure);
		figures.erase(iterator);
		if ((*iterator)->IsSelected())
			SeSelectedFigureAttribute();

		NotifyObservers(Hint(Hint::Type::Removed, figure));
	}

	bool Change(Figure* oldFigure, Figure* newFigure)
	{
		auto iterator = std::find(figures.begin(), figures.end(), oldFigure);
		if (iterator == figures.end())
			return false;

		figures.update(iterator, newFigure);
		std::vector<Figure*> changedFigures = { oldFigure, newFigure };
		NotifyObservers(Hint(Hint::Type::Changed, changedFigures));
		return true;
	}

	void Select(Figure& figure)
	{
		UnSelectAll();
		figure.Select(true);
		SeSelectedFigureAttribute();
	}

	void UnSelectAll()
	{
		std::for_each(figures.cbegin(), figures.cend(), [](Figure* figure) { figure->Select(false); });
		SeSelectedFigureAttribute();
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

	void Reset()
	{
		for (auto figure : *this)
			delete figure;
		ResetUndoData();
		figures.reset();
		UnSelectAll();
		highlightedFigure = nullptr;
	}

	void ResetUndoData()
	{
		auto undo_data = figures.undo_data();
		std::for_each(undo_data.begin(), undo_data.end(), [](Figure* figure) { delete figure; });
	}

	void AddDummyData(size_t count)
	{
		auto newFigures = FigureHelper::GetRandomFigures(count, GetArea());
		std::for_each(newFigures.cbegin(), newFigures.cend(), [&](Figure* figure) { figures.push_back(figure); });
		NotifyObservers(Hint(Hint::Type::Added, newFigures));
	}

private:
	//void ClearSelected()
	//{
	//	std::for_each(figures.cbegin(), figures.cend(), [](Figure* figure) { figure->Select(false); });
	//	SeSelectedFigureAttribute();
	//}

	const FigureAttribute& GetSelectedFigureAttribute() const
	{
		auto selectedFigures = GetSelectedFigures();

		if (selectedFigures.size() == 1)
			return selectedFigures[0]->Attribute();
		if (selectedFigures.size() > 1)
			return selectedFigures[0]->Attribute();
		return currentFigureAttribute;
	}
	
	void SeSelectedFigureAttribute()
	{
		Application::Set(GetSelectedFigureAttribute());
		NotifyObservers(Hint(Hint::Type::ViewOnly));
	}

	std::vector<Figure*> GetSelectedFigures() const
	{
		std::vector<Figure*> selectedFigures;
		std::for_each(figures.cbegin(), figures.cend(),
			[&](Figure* figure) {
				if (figure->IsSelected())
					selectedFigures.push_back(figure);
			});

		return selectedFigures;
	}
};
