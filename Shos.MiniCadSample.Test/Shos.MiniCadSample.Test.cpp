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
	};
}
