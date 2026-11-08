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
        if (std::find(observers.begin(), observers.end(), &observer) != observers.end())
            return false;
        observers.push_back(&observer);
        return true;
    }

    // Unregisters observer. Returns false if it was not registered.
    bool RemoveObserver(Observer<THint>& observer)
    {
        // std::remove only moves the remaining elements forward; erase actually shrinks the vector.
        auto newEnd = std::remove(observers.begin(), observers.end(), &observer);
        const bool removed = newEnd != observers.end();
        observers.erase(newEnd, observers.end());
        return removed;
    }
    
    void NotifyObservers(const THint& hint)
    {
        for (auto observer : observers)
            observer->Update(hint);
    }
};

} // namespace Shos::MiniCadSample::Utilities
