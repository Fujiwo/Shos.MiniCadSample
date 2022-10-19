#pragma once

#include <afx.h>
#include "Observer.h"

class FigureAttribute
{
    COLORREF color;
    bool     isColorValid;
    int      penWidth;
    bool     isPenWidthValid;

public:
    static FigureAttribute GetSum(std::vector<FigureAttribute> attributes)
    {
        FigureAttribute result;
        if (attributes.size() == 0) {
            result.isColorValid = result.isPenWidthValid = false;
        } else {
            result = attributes[0];
            for (size_t index = 1; index < attributes.size(); index++) {
                if (attributes[index].color != result.color)
                    result.isColorValid = false;
                if (attributes[index].penWidth != result.penWidth)
                    result.isPenWidthValid = false;
            }
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

    bool SetColor(COLORREF color)
    {
        if (color != this->color) {
            this->color = color;
            return true;
        }
        return false;
    }

    int GetPenWidth() const
    {
        return penWidth;
    }

    bool IsPenWidthValid() const
    {
        return isPenWidthValid;
    }

    bool SetPenWidth(int penWidth)
    {
        if (penWidth != this->penWidth) {
            this->penWidth = penWidth;
            return true;
        }
        return false;
    }

    FigureAttribute() : color(RGB(0x00, 0x00, 0x00)), isColorValid(true), penWidth(0), isPenWidthValid(true)
    {}

    void Serialize(CArchive& ar)
    {
        if (ar.IsStoring())
            ar << color << penWidth;
        else
            ar >> color >> penWidth;
    }
};
