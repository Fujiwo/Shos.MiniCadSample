// Checks that undo_redo_vector.h does not leak memory, using the CRT debug heap.
//
// Usage (Debug build):
//   Shos.MiniCadSample.MemoryLeakTest.exe               Runs all the scenarios. Exit code 0: no leak, 1: leaks (reported to stderr).
//   Shos.MiniCadSample.MemoryLeakTest.exe --self-check  Leaks on purpose, to make sure that leaks are detected (exit code 1).
//
// In a Release build, leaks cannot be detected; the scenarios only run.

#include "MemoryLeakTest.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include "../Shos.MiniCadSample/undo_redo_vector.h"

using namespace shos;

// A small heap-allocated element type; the scenarios below allocate it with new and must free every instance.
class foo
{
    int value;

public:
    foo(int value) : value(value) {}
    operator int() { return value; }
};

// Owns a foo and passes it to a clean-up function in the destructor (the pattern used by the collections).
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

// The clean-up function frees the element: no leak.
void function_clean_up_test()
{
    auto clean_up = [](foo* foo) { delete foo; };
    function_test function_test1(clean_up);
}

// Mixes push_back, erase, update, undo and redo on a vector with a custom clean-up function.
void undo_redo_vector_memory_leak_test()
{
    undo_redo_vector<foo*> array([](foo* foo) { delete foo; });

    array.push_back(new foo(300));
    array.push_back(new foo(600));
    array.push_back(new foo(800));

    array.undo();

    array.erase(std::next(array.begin(), 1));

    array.undo();
    array.redo();

    array.update(array.begin(), new foo(1200));

    array.erase(array.begin());
    array.push_back(new foo(1400));

    array.undo();
    array.redo();
    array.redo();
}

// The same sequence on the pointer collection, which deletes its elements by itself.
void undo_redo_pointer_vector_memory_leak_test()
{
    undo_redo_pointer_vector<foo> array;

    array.push_back(new foo(300));
    array.push_back(new foo(600));
    array.push_back(new foo(800));

    array.undo();

    array.erase(std::next(array.begin(), 1));

    array.undo();
    array.redo();

    array.update(array.begin(), new foo(1200));

    array.erase(array.begin());
    array.push_back(new foo(1400));

    array.undo();
    array.redo();
    array.redo();
}

// Nested transactions, clear inside a transaction, and a transaction left by an exception.
void transaction_memory_leak_test()
{
    undo_redo_pointer_vector<foo> array;
    array.push_back(new foo(100));
    {
        undo_redo_pointer_vector<foo>::transaction outer(array);
        array.push_back(new foo(200));
        {
            undo_redo_pointer_vector<foo>::transaction inner(array);
            array.update(array.begin(), new foo(1000));
            array.erase(std::next(array.begin()));
        }
        array.clear();
        array.push_back(new foo(300));
    }
    array.undo();
    array.redo();
    array.undo();

    // A transaction left by an exception.
    try {
        undo_redo_pointer_vector<foo>::transaction transaction(array);
        array.push_back(new foo(400));
        throw 0;
    } catch (int) {
    }
}

// Elements held only by the history must be deleted when the history is dropped or the collection is reset.
void history_memory_leak_test()
{
    undo_redo_pointer_vector<foo> array;
    array.push_back(new foo(100));
    array.push_back(new foo(200));
    array.push_back(new foo(300));
    array.erase(array.begin());
    array.update(array.begin(), new foo(2000));
    array.undo();

    array.clear_history(); // Deletes the elements held only by the history.
    array.push_back(new foo(400));
    array.undo();

    array.reset();
    array.push_back(new foo(500));
}

// Move construction and move assignment must transfer ownership without leaking or double deletion.
void move_memory_leak_test()
{
    undo_redo_pointer_vector<foo> source;
    source.push_back(new foo(100));
    source.push_back(new foo(200));
    source.erase(source.begin());

    auto target = std::move(source);
    target.undo();

    undo_redo_pointer_vector<foo> other;
    other.push_back(new foo(900));
    other = std::move(target);

    source.push_back(new foo(300)); // A moved-from collection still deletes its elements.
}

// Elements that can only be moved (unique_ptr) are cleaned up by their own destructor.
void move_only_element_memory_leak_test()
{
    undo_redo_vector<std::unique_ptr<foo>> array;
    array.push_back(std::make_unique<foo>(100));
    array.push_back(std::make_unique<foo>(200));
    array.update(array.begin(), std::make_unique<foo>(1000));
    array.erase(std::next(array.begin()));
    array.undo();
    array.undo();
}

// The thread-safe wrapper, including changes made through modify().
void synchronized_memory_leak_test()
{
    synchronized_undo_redo_vector<foo*> array([](foo*& element) { delete element; });
    array.push_back(new foo(100));
    array.push_back(new foo(200));
    array.update(0, new foo(1000));
    array.erase(1);
    array.modify([](undo_redo_vector<foo*>& collection) {
        collection.push_back(new foo(300));
        collection.push_back(new foo(400));
    });
    array.undo();
    array.clear_history();
}

// Runs the scenarios (or the self-check) and returns 1 if the CRT debug heap reports a leak.
int main(int argc, char* argv[])
{
#ifdef _DEBUG
    // Report to stderr as well as to the debugger, so that leaks are visible on the command line.
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
#endif // _DEBUG

    CheckMemoryLeaksStart();

    if (argc > 1 && std::strcmp(argv[1], "--self-check") == 0) {
        new foo(12345); // Leaks on purpose.
    } else {
        function_clean_up_test();
        undo_redo_vector_memory_leak_test();
        undo_redo_pointer_vector_memory_leak_test();
        transaction_memory_leak_test();
        history_memory_leak_test();
        move_memory_leak_test();
        move_only_element_memory_leak_test();
        synchronized_memory_leak_test();
    }

#ifdef _DEBUG
    // Everything allocated above has been freed by now, so any block left is a leak.
    // The check at exit (CheckMemoryLeaksStart) still runs too; this one sets the exit code.
    if (_CrtDumpMemoryLeaks()) {
        std::fputs("Memory leaks detected.\n", stderr);
        return 1;
    }
    std::puts("No memory leaks detected.");
#else // _DEBUG
    std::puts("Release build: memory leaks are not checked.");
#endif // _DEBUG
    return 0;
}
