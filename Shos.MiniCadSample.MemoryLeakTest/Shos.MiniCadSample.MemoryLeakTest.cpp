//#include <iostream>
#include <algorithm>
#include <cassert>
#include "MemoryLeakTest.h"
#include "../Shos.MiniCadSample/undo_redo_vector.h"

using namespace shos;

class Assert
{
public:
    template <typename T>
    static void are_equal(T value1, T value2) { assert(value1 == value2); }
    static void is_true  (bool value)         { assert(value); }
    static void is_false (bool value)         { assert(!value);}
};

class foo
{
    int value;

public:
    foo(int value) : value(value) {}
    operator int() { return value; }
};

class function_test
{
    std::function<void(foo*)> func;
    foo* foo1;

public:
    function_test(std::function<void(foo*)> func) : func(func), foo1(new foo(100))
    {}

    virtual ~function_test()
    {
        if (func != nullptr)
            func(foo1);
    }
};


void function_clean_up_test()
{
    auto clean_up = [](foo* foo) { delete foo; };
    function_test function_test1(clean_up);
}

void undo_redo_vector_memory_leak_test()
{
    undo_redo_collection<foo*, std::vector<foo*>> array([](foo* foo) { delete foo; });
    Assert::is_false(array.can_redo());

    array.push_back(new foo(300));
    Assert::is_false(array.can_redo());

    array.push_back(new foo(600));
    array.push_back(new foo(800));

    Assert::are_equal<size_t>(array.size(), 3UL);
    Assert::are_equal<int>(*array[0], 300);
    Assert::are_equal<int>(*array[1], 600);
    Assert::are_equal<int>(*array[2], 800);

    array.erase(std::next(array.begin(), 1));
    array.update(array.begin(), new foo(1200));
    array.erase(array.begin());
    array.push_back(new foo(1400));

    Assert::are_equal<size_t>(array.size(), 2UL);
    Assert::are_equal<int>(*array[0], 800);
    Assert::are_equal<int>(*array[1], 1400);

    array.undo();
    Assert::are_equal<size_t>(array.size(), 1UL);
    Assert::are_equal<int>(*array[0], 800);

    array.undo();
    Assert::are_equal<size_t>(array.size(), 2UL);
    Assert::are_equal<int>(*array[0], 1200);
    Assert::are_equal<int>(*array[1], 800);

    array.undo();
    Assert::are_equal<size_t>(array.size(), 2UL);
    Assert::are_equal<int>(*array[0], 300);
    Assert::are_equal<int>(*array[1], 800);

    Assert::is_true(array.can_redo());
    Assert::is_true(array.redo());
    Assert::are_equal<size_t>(array.size(), 2UL);
    Assert::are_equal<int>(*array[0], 1200);
    Assert::are_equal<int>(*array[1], 800);

    Assert::is_true(array.redo());
    Assert::are_equal<size_t>(array.size(), 1UL);
    Assert::are_equal<int>(*array[0], 800);

    Assert::is_true(array.redo());
    Assert::are_equal<size_t>(array.size(), 2UL);
    Assert::are_equal<int>(*array[0], 800);
    Assert::are_equal<int>(*array[1], 1400);

    Assert::is_false(array.can_redo());
    Assert::is_false(array.redo());

    std::for_each(array.begin(), array.end(), [](foo* p) { delete p; });
}

int main()
{
    CheckMemoryLeaksStart();

    function_clean_up_test();
    undo_redo_vector_memory_leak_test();
}
