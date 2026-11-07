#include "pch.h"
#include "CppUnitTest.h"
#include <chrono>
#include <cstdlib>
#include <iterator>
#include <list>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include "../Shos.MiniCadSample/undo_redo_vector.h"
#include "../Shos.MiniCadSample/Observer.h"
#include "AllocationFailure.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// ---- Allocation failure injection (see AllocationFailure.h) ----

namespace allocation_failure_injection {

thread_local int countdown = -1;

} // namespace allocation_failure_injection

void* operator new(std::size_t size)
{
    using namespace allocation_failure_injection;
#if _ITERATOR_DEBUG_LEVEL != 0
    // In debug builds, the MSVC containers allocate a small "container proxy" for iterator debugging,
    // even in their noexcept constructors (default and move). Failing that allocation would throw from
    // a noexcept function and call std::terminate, so it is never failed. This is a property of the
    // debug STL, not of the code under test (release builds have no proxy).
    const bool mayFail = size != sizeof(std::_Container_proxy);
#else // _ITERATOR_DEBUG_LEVEL != 0
    const bool mayFail = true;
#endif // _ITERATOR_DEBUG_LEVEL != 0
    if (mayFail && countdown >= 0 && countdown-- == 0)
        throw std::bad_alloc();
    if (void* pointer = std::malloc(size == 0 ? 1 : size))
        return pointer;
    throw std::bad_alloc();
}

void operator delete(void* pointer) noexcept
{
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept
{
    std::free(pointer);
}

namespace ShosMiniCadSampleTest
{
    using namespace shos;

    // Counts its live instances, to check that every element is deleted exactly once.
    class tracked
    {
        int value;

    public:
        static inline int live    = 0;
        static inline int created = 0;
        static inline int deleted = 0;

        static void reset_counters()
        {
            live = created = deleted = 0;
        }

        explicit tracked(int value) : value(value)
        {
            ++live;
            ++created;
        }

        ~tracked()
        {
            --live;
            ++deleted;
        }

        tracked(const tracked&)            = delete;
        tracked& operator=(const tracked&) = delete;

        int get() const
        {
            return value;
        }
    };

    template <typename TCollection>
    std::vector<int> values_of(const TCollection& collection)
    {
        std::vector<int> values;
        for (const auto& element : collection)
            values.push_back(element);
        return values;
    }

    std::vector<int> values_of(const undo_redo_pointer_vector<tracked>& collection)
    {
        std::vector<int> values;
        for (const auto element : collection)
            values.push_back(element->get());
        return values;
    }

    template <typename TCollection>
    void assert_values(const std::vector<int>& expected, const TCollection& collection)
    {
        const auto actual = values_of(collection);
        Assert::AreEqual<size_t>(expected.size(), actual.size(), L"size");
        for (size_t index = 0; index < expected.size(); index++)
            Assert::AreEqual<int>(expected[index], actual[index], (L"index " + std::to_wstring(index)).c_str());
    }

    TEST_CLASS(standard_container_test)
    {
    public:
        TEST_METHOD(check_vector)
        {
            std::vector<int> array = { 0, 1, 1, 2, 3, 5, 8, 13 };

            for (auto iterator = array.begin(); iterator != array.end(); iterator++)
                Assert::AreEqual(array[iterator - array.begin()], *iterator);

            auto iterator = std::find(array.begin(), array.end(), 8);
            Assert::AreEqual<std::ptrdiff_t>(6, std::distance(array.begin(), iterator));
            Assert::AreEqual<std::ptrdiff_t>(6, iterator - array.begin());
        }

        TEST_METHOD(check_list)
        {
            std::list<int> array = { 0, 1, 1, 2, 3, 5, 8, 13 };

            auto iterator = std::find(array.begin(), array.end(), 8);
            Assert::AreEqual<std::ptrdiff_t>(6, std::distance(array.begin(), iterator));
        }
    };

    TEST_CLASS(undo_redo_vector_test)
    {
    public:
        // ---- Basic modifications ----

        TEST_METHOD(construct)
        {
            undo_redo_vector<int> array;
            Assert::AreEqual<size_t>(0, array.size());
            Assert::IsTrue(array.empty());
            Assert::IsFalse(array.can_undo());
            Assert::IsFalse(array.can_redo());
        }

        TEST_METHOD(push_back)
        {
            undo_redo_vector<int> array;
            array.push_back(100);
            assert_values({ 100 }, array);

            array.push_back(300);
            assert_values({ 100, 300 }, array);
        }

        TEST_METHOD(insert)
        {
            undo_redo_vector<int> array;
            array.insert(array.begin(), 200);
            array.insert(array.begin(), 100);
            array.insert(array.end(), 400);
            array.insert(std::next(array.begin(), 2), 300);
            assert_values({ 100, 200, 300, 400 }, array);

            Assert::IsTrue(array.undo());
            assert_values({ 100, 200, 400 }, array);
            Assert::IsTrue(array.redo());
            assert_values({ 100, 200, 300, 400 }, array);
        }

        TEST_METHOD(erase)
        {
            undo_redo_vector<int> array;
            array.push_back(100);

            array.erase(array.begin());
            assert_values({}, array);

            array.push_back(400);
            array.push_back(500);
            array.push_back(600);
            array.push_back(700);

            array.erase(std::next(array.begin(), 1));
            assert_values({ 400, 600, 700 }, array);

            array.erase(std::next(array.begin(), 2));
            assert_values({ 400, 600 }, array);

            array.erase(array.begin());
            assert_values({ 600 }, array);

            array.erase(array.begin());
            assert_values({}, array);
        }

        TEST_METHOD(update)
        {
            undo_redo_vector<int> array;
            array.push_back(100);
            array.push_back(200);
            array.push_back(400);

            array.update(std::next(array.begin(), 1), 1200);
            assert_values({ 100, 1200, 400 }, array);
        }

        // ---- Element access ----

        TEST_METHOD(element_access)
        {
            undo_redo_vector<int> array;
            array.push_back(100);
            array.push_back(200);
            array.push_back(300);

            Assert::AreEqual(200, array[1]);
            Assert::AreEqual(300, array.at(2));
            Assert::AreEqual(100, array.front());
            Assert::AreEqual(300, array.back());
            Assert::IsFalse(array.empty());

            Assert::ExpectException<std::out_of_range>([&] { (void)array.at(3); });
        }

        // ---- Undo / redo ----

        TEST_METHOD(undo_push_back)
        {
            undo_redo_vector<int> array;
            Assert::IsFalse(array.can_undo());
            Assert::IsFalse(array.undo());

            array.push_back(400);
            array.push_back(500);
            array.push_back(700);

            Assert::IsTrue(array.can_undo());
            Assert::IsTrue(array.undo());
            assert_values({ 400, 500 }, array);

            Assert::IsTrue(array.undo());
            assert_values({ 400 }, array);

            Assert::IsTrue(array.undo());
            assert_values({}, array);

            Assert::IsFalse(array.can_undo());
            Assert::IsFalse(array.undo());

            array.push_back(1000);
            array.push_back(1100);
            assert_values({ 1000, 1100 }, array);
        }

        TEST_METHOD(undo_erase)
        {
            undo_redo_vector<int> array;
            array.push_back(100);
            array.push_back(200);
            array.push_back(400);

            array.erase(std::next(array.begin(), 1));
            Assert::IsTrue(array.undo());
            assert_values({ 100, 200, 400 }, array);

            array.erase(std::next(array.begin(), 2));
            array.erase(std::next(array.begin(), 1));
            array.erase(array.begin());

            Assert::IsTrue(array.can_undo());
            Assert::IsTrue(array.undo());
            assert_values({ 100 }, array);

            Assert::IsTrue(array.undo());
            assert_values({ 100, 200 }, array);

            Assert::IsTrue(array.undo());
            assert_values({ 100, 200, 400 }, array);

            Assert::IsTrue(array.undo());
            assert_values({ 100, 200 }, array);
        }

        TEST_METHOD(undo_update)
        {
            undo_redo_vector<int> array;
            array.push_back(100);
            array.push_back(200);
            array.push_back(400);

            array.update(std::next(array.begin(), 1), 1200);
            assert_values({ 100, 1200, 400 }, array);

            Assert::IsTrue(array.can_undo());
            Assert::IsTrue(array.undo());
            assert_values({ 100, 200, 400 }, array);
        }

        TEST_METHOD(undo_redo_update_repeatedly)
        {
            undo_redo_vector<int> array;
            array.push_back(100);
            array.update(array.begin(), 200);
            array.update(array.begin(), 300);

            for (int count = 0; count < 3; count++) {
                Assert::IsTrue(array.undo());
                assert_values({ 200 }, array);
                Assert::IsTrue(array.undo());
                assert_values({ 100 }, array);
                Assert::IsTrue(array.redo());
                assert_values({ 200 }, array);
                Assert::IsTrue(array.redo());
                assert_values({ 300 }, array);
            }
        }

        TEST_METHOD(redo)
        {
            undo_redo_vector<int> array;
            Assert::IsFalse(array.can_redo());

            array.push_back(300);
            Assert::IsFalse(array.can_redo());

            array.push_back(600);
            array.push_back(800);
            assert_values({ 300, 600, 800 }, array);

            array.erase(std::next(array.begin(), 1));
            array.update(array.begin(), 1200);
            array.erase(array.begin());
            array.push_back(1400);
            array.undo();
            array.push_back(1400);
            Assert::IsFalse(array.can_redo());
            assert_values({ 800, 1400 }, array);

            array.undo();
            assert_values({ 800 }, array);

            array.undo();
            assert_values({ 1200, 800 }, array);

            array.undo();
            assert_values({ 300, 800 }, array);

            Assert::IsTrue(array.can_redo());
            Assert::IsTrue(array.redo());
            assert_values({ 1200, 800 }, array);

            Assert::IsTrue(array.redo());
            assert_values({ 800 }, array);

            Assert::IsTrue(array.redo());
            assert_values({ 800, 1400 }, array);

            Assert::IsFalse(array.can_redo());
            Assert::IsFalse(array.redo());
        }

        // ---- Transactions ----

        TEST_METHOD(undo_group)
        {
            undo_redo_vector<int> array;

            array.push_back(100);
            {
                undo_redo_vector<int>::transaction transaction(array);
                array.push_back(300);
                array.push_back(400);
                array.erase(array.begin());
            }
            array.erase(array.begin());
            assert_values({ 400 }, array);

            Assert::IsTrue(array.undo());
            assert_values({ 300, 400 }, array);

            Assert::IsTrue(array.undo());
            assert_values({ 100 }, array);

            Assert::IsTrue(array.undo());
            assert_values({}, array);
            Assert::IsFalse(array.can_undo());

            Assert::IsTrue(array.redo());
            assert_values({ 100 }, array);

            Assert::IsTrue(array.redo());
            assert_values({ 300, 400 }, array);

            Assert::IsTrue(array.redo());
            assert_values({ 400 }, array);
            Assert::IsFalse(array.can_redo());

            Assert::IsTrue(array.undo());
            assert_values({ 300, 400 }, array);
        }

        TEST_METHOD(nested_transaction)
        {
            undo_redo_vector<int> array;
            {
                undo_redo_vector<int>::transaction outer(array);
                array.push_back(100);
                {
                    undo_redo_vector<int>::transaction inner(array);
                    array.push_back(200);
                    array.push_back(300);
                }
                Assert::IsTrue(array.in_transaction());
                array.push_back(400);
                array.clear(); // clear uses a transaction internally.
                array.push_back(500);
            }
            Assert::IsFalse(array.in_transaction());
            assert_values({ 500 }, array);

            // The outermost transaction is one undo step.
            Assert::IsTrue(array.undo());
            assert_values({}, array);
            Assert::IsFalse(array.can_undo());

            Assert::IsTrue(array.redo());
            assert_values({ 500 }, array);
        }

        TEST_METHOD(empty_transaction)
        {
            undo_redo_vector<int> array;
            array.push_back(100);
            {
                undo_redo_vector<int>::transaction transaction(array);
            }
            // No undo step was added: one undo empties the collection.
            Assert::IsTrue(array.undo());
            Assert::IsFalse(array.can_undo());
        }

        TEST_METHOD(undo_and_reset_during_transaction)
        {
            undo_redo_vector<int> array;
            array.push_back(100);
            array.push_back(200);
            Assert::IsTrue(array.undo());
            {
                undo_redo_vector<int>::transaction transaction(array);
                Assert::IsTrue(array.in_transaction());
                Assert::IsFalse(array.can_undo());
                Assert::IsFalse(array.can_redo());
                Assert::IsFalse(array.undo());
                Assert::IsFalse(array.redo());
                Assert::ExpectException<std::logic_error>([&] { array.reset(); });
                Assert::ExpectException<std::logic_error>([&] { array.clear_history(); });
            }
            Assert::IsTrue(array.can_undo());
            Assert::IsTrue(array.can_redo());
        }

        TEST_METHOD(exception_in_transaction_keeps_modifications)
        {
            undo_redo_vector<int> array;
            try {
                undo_redo_vector<int>::transaction transaction(array);
                array.push_back(100);
                array.push_back(200);
                throw std::runtime_error("test");
            } catch (const std::runtime_error&) {
            }
            Assert::IsFalse(array.in_transaction());
            assert_values({ 100, 200 }, array);

            // The modifications made before the exception are one undo step.
            Assert::IsTrue(array.undo());
            assert_values({}, array);
            Assert::IsFalse(array.can_undo());
        }

        TEST_METHOD(clear)
        {
            undo_redo_vector<int> array;
            array.push_back(100);
            array.push_back(200);

            array.clear();
            assert_values({}, array);

            Assert::IsTrue(array.undo());
            assert_values({ 100, 200 }, array);

            Assert::IsTrue(array.redo());
            assert_values({}, array);

            Assert::IsTrue(array.undo());
            assert_values({ 100, 200 }, array);
        }

        // ---- History ----

        TEST_METHOD(reset)
        {
            undo_redo_vector<int> array; // No clean_up function.
            array.push_back(100);
            array.push_back(200);
            array.erase(array.begin());
            Assert::IsTrue(array.undo());

            array.reset();
            assert_values({}, array);
            Assert::IsFalse(array.can_undo());
            Assert::IsFalse(array.can_redo());
        }

        TEST_METHOD(clear_history)
        {
            undo_redo_vector<int> array;
            array.push_back(100);
            array.push_back(200);
            array.push_back(300);
            Assert::IsTrue(array.undo());

            array.clear_history();
            assert_values({ 100, 200 }, array);
            Assert::IsFalse(array.can_undo());
            Assert::IsFalse(array.can_redo());

            // The collection is still usable.
            array.push_back(400);
            assert_values({ 100, 200, 400 }, array);
            Assert::IsTrue(array.undo());
            assert_values({ 100, 200 }, array);
            Assert::IsFalse(array.can_undo());
        }

        // ---- Containers and element types ----

        TEST_METHOD(list_container)
        {
            undo_redo_collection<int, std::list<int>> array;
            array.push_back(100);
            array.push_back(300);
            array.insert(std::next(array.begin()), 200);
            array.update(array.begin(), 1000);
            array.erase(std::next(array.begin(), 2));
            assert_values({ 1000, 200 }, array);
            Assert::AreEqual(200, array[1]);

            Assert::IsTrue(array.undo());
            assert_values({ 1000, 200, 300 }, array);
            Assert::IsTrue(array.undo());
            assert_values({ 100, 200, 300 }, array);
            Assert::IsTrue(array.undo());
            assert_values({ 100, 300 }, array);
            Assert::IsTrue(array.redo());
            Assert::IsTrue(array.redo());
            Assert::IsTrue(array.redo());
            assert_values({ 1000, 200 }, array);
        }

        TEST_METHOD(move_only_element)
        {
            undo_redo_vector<std::unique_ptr<int>> array;
            array.push_back(std::make_unique<int>(100));
            array.push_back(std::make_unique<int>(200));
            array.update(array.begin(), std::make_unique<int>(1000));
            array.erase(std::next(array.begin()));

            Assert::AreEqual<size_t>(1, array.size());
            Assert::AreEqual(1000, *array[0]);

            Assert::IsTrue(array.undo());
            Assert::AreEqual(200, *array[1]);
            Assert::IsTrue(array.undo());
            Assert::AreEqual(100, *array[0]);
            Assert::IsTrue(array.redo());
            Assert::AreEqual(1000, *array[0]);
        }

        // ---- Ownership (clean_up) ----

        TEST_METHOD(clean_up_function)
        {
            tracked::reset_counters();
            {
                undo_redo_vector<tracked*> array([](tracked*& element) { delete element; });

                array.push_back(new tracked(300));
                array.push_back(new tracked(600));
                array.push_back(new tracked(800));

                array.undo();                              // 800 is held by the history.
                array.erase(std::next(array.begin(), 1)); // Discards the redo of 800 (deleted); 600 is held.
                Assert::AreEqual(1, tracked::deleted);

                array.undo();
                array.redo();

                array.update(array.begin(), new tracked(1200)); // 300 is held.
                array.erase(array.begin());                     // 1200 is held.
                array.push_back(new tracked(1400));
                assert_values_of_tracked({ 1400 }, array);

                array.undo();
                array.redo();
                Assert::IsFalse(array.redo());
                Assert::AreEqual(4, tracked::live); // 1400 in the collection; 600, 300 and 1200 in the history.
            }
            Assert::AreEqual(0, tracked::live);
            Assert::AreEqual(tracked::created, tracked::deleted);
        }

        TEST_METHOD(pointer_vector)
        {
            tracked::reset_counters();
            {
                undo_redo_pointer_vector<tracked> array;

                array.push_back(new tracked(300));
                array.push_back(new tracked(600));
                array.push_back(new tracked(800));
                assert_values({ 300, 600, 800 }, array);

                array.undo();
                array.erase(std::next(array.begin(), 1));
                assert_values({ 300 }, array);
                Assert::AreEqual(1, tracked::deleted); // 800, whose redo was discarded.

                array.undo();
                array.redo();
                assert_values({ 300 }, array);

                array.update(array.begin(), new tracked(1200));
                array.erase(array.begin());
                array.push_back(new tracked(1400));
                assert_values({ 1400 }, array);

                array.undo();
                assert_values({}, array);
                array.redo();
                assert_values({ 1400 }, array);
                Assert::IsFalse(array.redo());
            }
            Assert::AreEqual(0, tracked::live);
            Assert::AreEqual(tracked::created, tracked::deleted);
        }

        TEST_METHOD(pointer_vector_clean_up_on_history_operations)
        {
            tracked::reset_counters();
            {
                undo_redo_pointer_vector<tracked> array;
                array.push_back(new tracked(100));
                array.push_back(new tracked(200));
                array.push_back(new tracked(300));
                array.erase(array.begin());                   // 100 is held by the history.
                array.update(array.begin(), new tracked(2000)); // 200 is held by the history.
                Assert::IsTrue(array.undo());                 // 2000 is held by the history.
                Assert::AreEqual(4, tracked::live);           // 200 and 300 in the collection; 100 and 2000 in the history.

                array.clear_history();                        // 100 and 2000 are deleted; 200 and 300 stay.
                Assert::AreEqual(2, tracked::live);
                assert_values({ 200, 300 }, array);

                array.reset();
                Assert::AreEqual(0, tracked::live);
            }
            Assert::AreEqual(tracked::created, tracked::deleted);
        }

        TEST_METHOD(move)
        {
            tracked::reset_counters();
            {
                undo_redo_pointer_vector<tracked> source;
                source.push_back(new tracked(100));
                source.push_back(new tracked(200));
                source.erase(source.begin()); // 100 is held by the history.

                auto target = std::move(source);
                Assert::IsTrue(source.empty());
                Assert::IsFalse(source.can_undo());
                assert_values({ 200 }, target);
                Assert::IsTrue(target.undo());
                assert_values({ 100, 200 }, target);

                undo_redo_pointer_vector<tracked> other;
                other.push_back(new tracked(900));
                other = std::move(target); // 900 is deleted.
                Assert::AreEqual(1, tracked::deleted);
                assert_values({ 100, 200 }, other);
                Assert::IsTrue(target.empty());

                source.push_back(new tracked(300)); // A moved-from collection is still usable.
                assert_values({ 300 }, source);
            }
            Assert::AreEqual(0, tracked::live);
            Assert::AreEqual(tracked::created, tracked::deleted);
        }

        // ---- Exception safety (allocation failure injection) ----
        //
        // Each test makes the 1st, 2nd, 3rd ... allocation of an operation fail until it succeeds,
        // and checks that every failure leaves the collection, the history and the ownership consistent.

        TEST_METHOD(push_back_failure)
        {
            tracked::reset_counters();
            {
                undo_redo_pointer_vector<tracked> array;
                array.push_back(new tracked(100));
                array.push_back(new tracked(200));
                array.push_back(new tracked(300));
                Assert::IsTrue(array.undo()); // 300 is held by the redo history.

                int failures = 0;
                for (int succeeding = 0; ; succeeding++) {
                    auto element = new tracked(400);
                    try {
                        allocation_failure failure(succeeding);
                        array.push_back(element);
                    } catch (const std::bad_alloc&) {
                        failures++;
                        // Unchanged, and the given element has been deleted.
                        assert_values({ 100, 200 }, array);
                        Assert::IsTrue(array.can_redo());
                        Assert::AreEqual(3, tracked::live);
                        continue;
                    }
                    break;
                }
                Assert::IsTrue(failures > 0);
                assert_values({ 100, 200, 400 }, array);
                Assert::IsFalse(array.can_redo());
                Assert::AreEqual(3, tracked::live); // 300 was deleted with the redo history.

                Assert::IsTrue(array.undo());
                assert_values({ 100, 200 }, array);
            }
            Assert::AreEqual(0, tracked::live);
            Assert::AreEqual(tracked::created, tracked::deleted);
        }

        TEST_METHOD(insert_update_erase_failure)
        {
            tracked::reset_counters();
            {
                undo_redo_pointer_vector<tracked> array;
                array.push_back(new tracked(100));
                array.push_back(new tracked(200));

                int failures = 0;
                for (int succeeding = 0; ; succeeding++) {
                    auto element = new tracked(150);
                    try {
                        allocation_failure failure(succeeding);
                        array.insert(std::next(array.begin()), element);
                    } catch (const std::bad_alloc&) {
                        failures++;
                        assert_values({ 100, 200 }, array);
                        Assert::AreEqual(2, tracked::live);
                        continue;
                    }
                    break;
                }
                Assert::IsTrue(failures > 0);
                assert_values({ 100, 150, 200 }, array);

                failures = 0;
                for (int succeeding = 0; ; succeeding++) {
                    auto element = new tracked(1000);
                    try {
                        allocation_failure failure(succeeding);
                        array.update(array.begin(), element);
                    } catch (const std::bad_alloc&) {
                        failures++;
                        assert_values({ 100, 150, 200 }, array);
                        Assert::AreEqual(3, tracked::live);
                        continue;
                    }
                    break;
                }
                Assert::IsTrue(failures > 0);
                assert_values({ 1000, 150, 200 }, array);
                Assert::AreEqual(4, tracked::live); // 100 is held by the history.

                failures = 0;
                for (int succeeding = 0; ; succeeding++) {
                    try {
                        allocation_failure failure(succeeding);
                        array.erase(std::next(array.begin()));
                    } catch (const std::bad_alloc&) {
                        failures++;
                        // The element stays in the collection and is not deleted.
                        assert_values({ 1000, 150, 200 }, array);
                        Assert::AreEqual(4, tracked::live);
                        continue;
                    }
                    break;
                }
                Assert::IsTrue(failures > 0);
                assert_values({ 1000, 200 }, array);

                // The history is intact.
                Assert::IsTrue(array.undo());
                assert_values({ 1000, 150, 200 }, array);
                Assert::IsTrue(array.undo());
                assert_values({ 100, 150, 200 }, array);
                Assert::IsTrue(array.undo());
                assert_values({ 100, 200 }, array);
            }
            Assert::AreEqual(0, tracked::live);
            Assert::AreEqual(tracked::created, tracked::deleted);
        }

        TEST_METHOD(failure_in_transaction)
        {
            tracked::reset_counters();
            {
                undo_redo_pointer_vector<tracked> array;
                array.push_back(new tracked(100));

                {
                    undo_redo_pointer_vector<tracked>::transaction transaction(array);
                    array.push_back(new tracked(200));

                    auto element = new tracked(300);
                    bool failed = false;
                    try {
                        allocation_failure failure(0);
                        array.push_back(element);
                    } catch (const std::bad_alloc&) {
                        failed = true;
                    }
                    Assert::IsTrue(failed);
                    assert_values({ 100, 200 }, array);
                    Assert::AreEqual(2, tracked::live);

                    array.push_back(new tracked(400));
                }
                assert_values({ 100, 200, 400 }, array);

                // The transaction is still one undo step without the failed push_back.
                Assert::IsTrue(array.undo());
                assert_values({ 100 }, array);
                Assert::IsTrue(array.redo());
                assert_values({ 100, 200, 400 }, array);
            }
            Assert::AreEqual(0, tracked::live);
            Assert::AreEqual(tracked::created, tracked::deleted);
        }

        TEST_METHOD(vector_undo_redo_does_not_allocate)
        {
            tracked::reset_counters();
            {
                undo_redo_pointer_vector<tracked> array;
                for (int value = 1; value <= 8; value++)
                    array.push_back(new tracked(value));
                array.update(array.begin(), new tracked(10));
                array.clear(); // One undo step that removes 8 elements.

                // The next allocation would throw; undo / redo must succeed without allocating.
                {
                    allocation_failure failure(0);
                    while (array.undo())
                        ;
                    Assert::IsTrue(array.empty());
                    while (array.redo())
                        ;
                    Assert::IsTrue(array.empty());
                    while (array.undo())
                        ;
                }
                assert_values({}, array);
                Assert::IsTrue(array.redo());
                Assert::IsTrue(array.redo());
                assert_values({ 1, 2 }, array);
                Assert::AreEqual(9, tracked::live);
            }
            Assert::AreEqual(0, tracked::live);
            Assert::AreEqual(tracked::created, tracked::deleted);
        }

        TEST_METHOD(list_undo_failure_leaves_step_unchanged)
        {
            undo_redo_collection<int, std::list<int>> array;
            array.push_back(100);
            array.push_back(200);
            array.erase(array.begin());

            // Undoing the erase inserts a node, which allocates.
            bool failed = false;
            try {
                allocation_failure failure(0);
                array.undo();
            } catch (const std::bad_alloc&) {
                failed = true;
            }
            Assert::IsTrue(failed);
            assert_values({ 200 }, array);
            Assert::IsTrue(array.can_undo());

            Assert::IsTrue(array.undo());
            assert_values({ 100, 200 }, array);
        }

        // ---- append (all or nothing) ----

        TEST_METHOD(append)
        {
            undo_redo_vector<int> array;
            array.push_back(100);
            array.append({});
            Assert::IsFalse(array.can_redo());
            array.append({ 200, 300, 400 });
            assert_values({ 100, 200, 300, 400 }, array);

            // One undo step.
            Assert::IsTrue(array.undo());
            assert_values({ 100 }, array);
            Assert::IsTrue(array.redo());
            assert_values({ 100, 200, 300, 400 }, array);
        }

        TEST_METHOD(append_in_transaction)
        {
            undo_redo_vector<int> array;
            {
                undo_redo_vector<int>::transaction transaction(array);
                array.push_back(100);
                array.append({ 200, 300 });
                array.push_back(400);
            }
            assert_values({ 100, 200, 300, 400 }, array);
            Assert::IsTrue(array.undo());
            assert_values({}, array);
            Assert::IsFalse(array.can_undo());
        }

        TEST_METHOD(append_failure)
        {
            tracked::reset_counters();
            {
                undo_redo_pointer_vector<tracked> array;
                array.push_back(new tracked(100));
                array.push_back(new tracked(200));
                Assert::IsTrue(array.undo()); // 200 is held by the redo history.

                int failures = 0;
                for (int succeeding = 0; ; succeeding++) {
                    // The vector is made outside the failure scope; its allocation is not what is tested.
                    std::vector<tracked*> elements = { new tracked(1), new tracked(2), new tracked(3) };
                    try {
                        allocation_failure failure(succeeding);
                        array.append(std::move(elements));
                    } catch (const std::bad_alloc&) {
                        failures++;
                        // Nothing appended, the history is intact, and the 3 elements have been deleted.
                        assert_values({ 100 }, array);
                        Assert::IsTrue(array.can_redo());
                        Assert::AreEqual(2, tracked::live);
                        continue;
                    }
                    break;
                }
                Assert::IsTrue(failures > 0);
                assert_values({ 1, 2, 3 }, values_of_tail(array, 3));
                Assert::AreEqual<size_t>(4, array.size());
                Assert::IsFalse(array.can_redo());
                Assert::AreEqual(4, tracked::live); // 200 was deleted with the redo history.

                Assert::IsTrue(array.undo());
                assert_values({ 100 }, array);
            }
            Assert::AreEqual(0, tracked::live);
            Assert::AreEqual(tracked::created, tracked::deleted);
        }

        TEST_METHOD(append_failure_with_list_rolls_back)
        {
            // With std::list, each inserted node is allocated while the elements are applied,
            // so a failure there must undo the nodes inserted so far.
            undo_redo_collection<int, std::list<int>> array;
            array.push_back(100);

            int failures = 0;
            for (int succeeding = 0; ; succeeding++) {
                try {
                    allocation_failure failure(succeeding);
                    array.append({ 1, 2, 3, 4, 5 });
                } catch (const std::bad_alloc&) {
                    failures++;
                    assert_values({ 100 }, array);
                    continue;
                }
                break;
            }
            Assert::IsTrue(failures > 5); // Some failures happened while inserting the nodes.
            assert_values({ 100, 1, 2, 3, 4, 5 }, array);
            Assert::IsTrue(array.undo());
            assert_values({ 100 }, array);
        }

        // ---- Performance (N1) ----

        TEST_METHOD(many_modifications_are_not_quadratic)
        {
            constexpr int count = 50000;
            const auto start = std::chrono::steady_clock::now();

            undo_redo_vector<int> array;
            for (int value = 0; value < count; value++)
                array.push_back(value);
            array.clear(); // One transaction with `count` steps.
            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(count, array.size());

            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
            Logger::WriteMessage((L"many_modifications_are_not_quadratic: " + std::to_wstring(elapsed.count()) + L" ms").c_str());
            Assert::IsTrue(elapsed < std::chrono::seconds(5), L"Took too long; the history may be growing quadratically.");
        }

    private:
        static std::vector<int> values_of_tail(const undo_redo_pointer_vector<tracked>& array, size_t count)
        {
            std::vector<int> values;
            for (size_t index = array.size() - count; index < array.size(); index++)
                values.push_back(array[index]->get());
            return values;
        }

        static void assert_values_of_tracked(const std::vector<int>& expected, const undo_redo_vector<tracked*>& array)
        {
            Assert::AreEqual<size_t>(expected.size(), array.size());
            for (size_t index = 0; index < expected.size(); index++)
                Assert::AreEqual<int>(expected[index], array[index]->get());
        }
    };

    TEST_CLASS(synchronized_undo_redo_vector_test)
    {
    public:
        TEST_METHOD(basic_operations)
        {
            synchronized_undo_redo_vector<int> array;
            array.push_back(100);
            array.push_back(300);
            Assert::IsTrue(array.insert(1, 200));
            Assert::IsFalse(array.insert(4, 999));
            Assert::IsTrue(array.update(0, 1000));
            Assert::IsFalse(array.update(3, 999));
            Assert::IsTrue(array.erase(2));
            Assert::IsFalse(array.erase(2));

            assert_values({ 1000, 200 }, array.snapshot());
            Assert::AreEqual(1000, array.get(0).value());
            Assert::IsFalse(array.get(2).has_value());
            Assert::AreEqual(1000, array.front().value());
            Assert::AreEqual(200, array.back().value());
            Assert::AreEqual<size_t>(2, array.size());

            Assert::IsTrue(array.undo());
            assert_values({ 1000, 200, 300 }, array.snapshot());
            Assert::IsTrue(array.redo());
            assert_values({ 1000, 200 }, array.snapshot());

            array.clear_history();
            Assert::IsFalse(array.can_undo());
            Assert::IsFalse(array.can_redo());

            array.reset();
            Assert::IsTrue(array.empty());
            Assert::IsFalse(array.front().has_value());
        }

        TEST_METHOD(modify_is_one_undo_step)
        {
            synchronized_undo_redo_vector<int> array;
            array.push_back(100);

            const auto size = array.modify([](undo_redo_vector<int>& collection) {
                collection.push_back(200);
                collection.push_back(300);
                return collection.size();
            });
            Assert::AreEqual<size_t>(3, size);

            const auto sum = array.read([](const undo_redo_vector<int>& collection) {
                int total = 0;
                for (auto value : collection)
                    total += value;
                return total;
            });
            Assert::AreEqual(600, sum);

            Assert::IsTrue(array.undo());
            assert_values({ 100 }, array.snapshot());
        }

        TEST_METHOD(concurrent_push_back)
        {
            constexpr int thread_count = 8;
            constexpr int count        = 1000;

            synchronized_undo_redo_vector<int> array;
            {
                std::vector<std::thread> threads;
                for (int thread = 0; thread < thread_count; thread++) {
                    threads.emplace_back([&array] {
                        for (int value = 0; value < count; value++) {
                            array.push_back(value);
                            (void)array.size();
                        }
                    });
                }
                for (auto& thread : threads)
                    thread.join();
            }
            Assert::AreEqual<size_t>(thread_count * count, array.size());

            for (int undone = 0; undone < thread_count * count; undone++)
                Assert::IsTrue(array.undo());
            Assert::IsTrue(array.empty());
        }
    };

    TEST_CLASS(observable_test)
    {
        // Records the hints it receives.
        class recorder : public Observer<int>
        {
        public:
            std::vector<int> hints;

            void Update(const int& hint) override
            {
                hints.push_back(hint);
            }
        };

    public:
        TEST_METHOD(notify)
        {
            Observable<int> observable;
            recorder        first, second;
            Assert::IsTrue(observable.AddObserver(first));
            Assert::IsTrue(observable.AddObserver(second));

            observable.NotifyObservers(1);
            observable.NotifyObservers(2);
            assert_values({ 1, 2 }, first.hints);
            assert_values({ 1, 2 }, second.hints);
        }

        TEST_METHOD(add_same_observer_twice)
        {
            Observable<int> observable;
            recorder        observer;
            Assert::IsTrue (observable.AddObserver(observer));
            Assert::IsFalse(observable.AddObserver(observer)); // H2: already registered; ignored.

            observable.NotifyObservers(1);
            assert_values({ 1 }, observer.hints); // Notified only once.
        }

        TEST_METHOD(remove_observer)
        {
            Observable<int> observable;
            recorder        first, second, third;
            observable.AddObserver(first);
            observable.AddObserver(second);
            observable.AddObserver(third);

            Assert::IsTrue (observable.RemoveObserver(second));
            Assert::IsFalse(observable.RemoveObserver(second)); // L5: really removed the first time.

            observable.NotifyObservers(1);
            assert_values({ 1 }, first.hints);
            assert_values({},    second.hints);
            assert_values({ 1 }, third.hints);

            // It can be registered again after being removed.
            Assert::IsTrue(observable.AddObserver(second));
            observable.NotifyObservers(2);
            assert_values({ 2 }, second.hints);
        }
    };
}
