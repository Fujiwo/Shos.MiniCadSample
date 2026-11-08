#pragma once

// FigureAttribute: the color and pen width of a figure, with per-item validity (see the class comment).

#include <afx.h>
#include <ranges>
#include <vector>
#include "Observer.h"

namespace Shos::MiniCadSample::Models {

// The attribute of a figure, or a set of them.
// A "valid" flag tells whether the item has a value:
// - The attribute of a figure, and the one used for new figures, always have both items valid.
// - GetSum of several attributes makes an item invalid where their values differ (shown blank in the dialog).
// - A change (Empty() plus SetColor / SetPenWidth) has only the changed items valid; ApplyTo overwrites only those.
class FigureAttribute
{
    COLORREF color           = RGB(0x00, 0x00, 0x00); // Meaningful only if isColorValid.
    bool     isColorValid    = true;
    int      penWidth        = 0; // Meaningful only if isPenWidthValid. In logical units.
    bool     isPenWidthValid = true;

public:
    // An attribute with no valid item: the start of a change.
    static FigureAttribute Empty()
    {
        FigureAttribute attribute;
        attribute.isColorValid = attribute.isPenWidthValid = false;
        return attribute;
    }

    // Merges attributes: an item stays valid only if all the attributes have the same value for it.
    // Returns Empty() for no attributes.
    static FigureAttribute GetSum(const std::vector<FigureAttribute>& attributes)
    {
        if (attributes.empty())
            return Empty();

        auto result = attributes.front();
        for (const auto& attribute : attributes | std::views::drop(1)) {
            if (attribute.color != result.color)
                result.isColorValid = false;
            if (attribute.penWidth != result.penWidth)
                result.isPenWidthValid = false;
        }
        return result;
    }

    COLORREF GetColor() const
    {
        return color;
    }

    bool IsColorValid() const
    {
        return isColorValid;
    }

    // Sets the color and makes it valid. Returns whether anything changed.
    bool SetColor(COLORREF newColor)
    {
        if (isColorValid && newColor == color)
            return false;
        color        = newColor;
        isColorValid = true;
        return true;
    }

    int GetPenWidth() const
    {
        return penWidth;
    }

    bool IsPenWidthValid() const
    {
        return isPenWidthValid;
    }

    // Sets the pen width and makes it valid. Returns whether anything changed.
    bool SetPenWidth(int newPenWidth)
    {
        if (isPenWidthValid && newPenWidth == penWidth)
            return false;
        penWidth        = newPenWidth;
        isPenWidthValid = true;
        return true;
    }

    // Whether no item is valid (no change at all).
    bool IsEmpty() const
    {
        return !isColorValid && !isPenWidthValid;
    }

    // Overwrites the items of target with the valid items of this. Returns whether target changed.
    bool ApplyTo(FigureAttribute& target) const
    {
        bool changed = false;
        if (isColorValid && target.SetColor(color))
            changed = true;
        if (isPenWidthValid && target.SetPenWidth(penWidth))
            changed = true;
        return changed;
    }

    FigureAttribute() = default;

    // Stores / loads the values only. The valid flags are not stored: a stored attribute is always fully valid.
    void Serialize(CArchive& ar)
    {
        if (ar.IsStoring())
            ar << color << penWidth;
        else
            ar >> color >> penWidth;
    }
};

} // namespace Shos::MiniCadSample::Models
