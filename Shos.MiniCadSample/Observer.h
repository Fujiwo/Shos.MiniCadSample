#pragma once

// A minimal Observer pattern: an Observable sends hints of type THint to its registered Observers.
// Used to tell the document and the views what a Model change affected.

#include <algorithm>
#include <vector>

namespace Shos::MiniCadSample::Utilities {

// Receives notifications from an Observable<THint>.
template <typename THint>
class Observer
{
public:
    // Called on every notification. Observers must not add or remove observers while being notified.
    virtual void Update(const THint& hint) = 0;
};

// Keeps a list of observers and notifies them. It does not own the observers: they must stay alive
// while registered, and unregister themselves (RemoveObserver) before they are destroyed.
template <typename THint>
class Observable
{
    std::vector<Observer<THint>*> observers; // In registration order, without duplicates.

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
    
    // Calls Update(hint) on every observer, in registration order.
    void NotifyObservers(const THint& hint)
    {
        for (auto observer : observers)
            observer->Update(hint);
    }
};

} // namespace Shos::MiniCadSample::Utilities
