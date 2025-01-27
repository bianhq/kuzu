#include "gtest/gtest.h"
#include "test/common/include/vector/operations/vector_operations_test_helper.h"

#include "src/function/include/binary_operation_executor.h"
#include "src/function/string/operations/include/concat_operation.h"

using namespace kuzu::function;
using namespace kuzu::testing;

class StringArithmeticOperandsInSameDataChunkTest : public OperandsInSameDataChunk, public Test {

public:
    DataTypeID getDataTypeOfOperands() override { return STRING; }
    DataTypeID getDataTypeOfResultVector() override { return STRING; }

    void SetUp() override { initDataChunk(); }
};

TEST_F(StringArithmeticOperandsInSameDataChunkTest, StringTest) {
    auto lVector = vector1;
    auto rVector = vector2;
    auto resultData = (ku_string_t*)result->values;
    // Fill values before the comparison.
    for (int i = 0; i < NUM_TUPLES; i++) {
        lVector->addString(i, to_string(i));
        rVector->addString(i, to_string(110 - i));
    }

    BinaryOperationExecutor::executeSwitch<ku_string_t, ku_string_t, ku_string_t, operation::Concat,
        BinaryStringAndListOperationWrapper>(*lVector, *rVector, *result);
    for (int i = 0; i < NUM_TUPLES; i++) {
        ASSERT_EQ(resultData[i].getAsString(), to_string(i) + to_string(110 - i));
    }
}

TEST_F(StringArithmeticOperandsInSameDataChunkTest, BigStringTest) {
    auto lVector = vector1;
    auto rVector = vector2;
    auto resultData = (ku_string_t*)result->values;
    // Fill values before the comparison.
    for (int i = 0; i < NUM_TUPLES; i++) {
        lVector->addString(i, to_string(i) + "abcdefabcdefqwert");
        rVector->addString(i, to_string(110 - i) + "abcdefabcdefqwert");
    }

    BinaryOperationExecutor::executeSwitch<ku_string_t, ku_string_t, ku_string_t, operation::Concat,
        BinaryStringAndListOperationWrapper>(*lVector, *rVector, *result);
    for (int i = 0; i < NUM_TUPLES; i++) {
        ASSERT_EQ(resultData[i].getAsString(),
            to_string(i) + "abcdefabcdefqwert" + to_string(110 - i) + "abcdefabcdefqwert");
    }
}
