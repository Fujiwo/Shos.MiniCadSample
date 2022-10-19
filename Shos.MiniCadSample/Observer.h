#pragma once

#include <vector>

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
    void AddObserver(Observer<THint>& observer)
    {
        observers.push_back(&observer);
    }

    bool RemoveObserver(Observer<THint>& observer)
    {
        return std::remove(observers.begin(), observers.end(), &observer) != observers.end();
    }
    
    void NotifyObservers(const THint& hint)
    {
        for (auto observer : observers)
            observer->Update(hint);
    }
};
