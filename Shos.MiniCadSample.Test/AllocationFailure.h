#pragma once

// Allocation failure injection for tests.
//
// operator new of this test module is replaced (Shos.MiniCadSample.Test.cpp) so that, within an
// allocation_failure scope, the allocation after `succeeding` successful ones throws std::bad_alloc (only once).
// Outside such a scope it behaves like the default one.
//
// Only the allocations made by the code of this module are affected; objects allocated by the class-specific
// operator new of CObject, or inside the MFC DLL, are not.
// In debug builds, the container proxies of the MSVC STL (allocated even in noexcept constructors) are never failed.

namespace allocation_failure_injection {

extern thread_local int countdown; // Number of allocations that still succeed; -1 means "never fail".

} // namespace allocation_failure_injection

namespace ShosMiniCadSampleTest {

class allocation_failure
{
public:
    explicit allocation_failure(int succeeding)
    {
        allocation_failure_injection::countdown = succeeding;
    }

    ~allocation_failure()
    {
        allocation_failure_injection::countdown = -1;
    }

    allocation_failure(const allocation_failure&)            = delete;
    allocation_failure& operator=(const allocation_failure&) = delete;
};

} // namespace ShosMiniCadSampleTest
