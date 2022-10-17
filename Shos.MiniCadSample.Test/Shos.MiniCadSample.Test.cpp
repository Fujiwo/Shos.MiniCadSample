#include "pch.h"
#include "CppUnitTest.h"
#include "../Shos.MiniCadSample/undo_redo_vector.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ShosMiniCadSampleTest
{
	TEST_CLASS(undo_redo_vector_test)
	{
	public:
		TEST_METHOD(construct)
		{
			undo_redo_vector<int> array;
			Assert::AreEqual<size_t>(array.size(), 0UL);
		}

		TEST_METHOD(push_back)
		{
			undo_redo_vector<int> array;
			array.push_back(100);

			Assert::AreEqual<size_t>(array.size(), 1UL);
			Assert::AreEqual<size_t>(array[0], 100);

			array.push_back(300);

			Assert::AreEqual<size_t>(array.size(), 2UL);
			Assert::AreEqual<size_t>(array[0], 100);
			Assert::AreEqual<size_t>(array[1], 300);
		}
	};
}
