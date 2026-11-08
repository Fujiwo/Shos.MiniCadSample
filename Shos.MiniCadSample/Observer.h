#pragma once

#include <algorithm>
#include <vector>

namespace Shos::MiniCadSample::Utilities {

template <typename THint>
class Observer
{
public:
    virtual void Update(const THint& hint) = 0;
};

template <typename THint>
class Observable
{
    std::vector<Observer<THint>*> observers;

public:
    // Registers observer. Registering the same observer again has no effect, so it is never notified twice.
    // Returns false if it was already registered.
    bool AddObserver(Observer<THint>& observer)
    {
        if (std::ranges::find(observers, &observer) != observers.end())
            return false;
        observers.push_back(&observer);
        return true;
    }

    // Unregisters observer. Returns false if it was not registered.
    bool RemoveObserver(Observer<THint>& observer)
    {
        return std::erase(observers, &observer) != 0;
    }
    
    void NotifyObservers(const THint& hint)
    {
        for (auto observer : observers)
            observer->Update(hint);
    }
};

} // namespace Shos::MiniCadSample::Utilities
