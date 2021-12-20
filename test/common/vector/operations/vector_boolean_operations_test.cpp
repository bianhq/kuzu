#include <unordered_set>

#include "gtest/gtest.h"
#include "test/common/include/vector/operations/vector_operations_test_helper.h"

#include "src/common/include/data_chunk/data_chunk.h"
#include "src/common/include/vector/operations/vector_boolean_operations.h"

using namespace graphflow::common;
using namespace graphflow::testing;
using namespace std;

/**
 * Populates the 2 vectors with boolean for numTuples number of values.
 *            lVector              rVector
 *          -----------          ----------
 *  0            T                    T
 *  1            F                    T
 *  2            T                    F
 *  3            F                    F
 *              ...                  ...     (Pattern continues)
 * */
static void setValuesInVectors(const shared_ptr<ValueVector>& lVector,
    const shared_ptr<ValueVector>& rVector, uint32_t numTuples) {
    auto lVectorData = (bool*)lVector->values;
    auto rVectorData = (bool*)rVector->values;
    for (int i = 0; i < numTuples; i++) {
        lVectorData[i] = i % 2 == 0;
        rVectorData[i] = (i >> 1) % 2 == 0;
    }
}

/**
 * Sets isNull bits for the 2 vectors for numTuples number of values. For lVector, alternating
 * groups of 4 are set as either notNull or Null. For rVector, alternating groups are of size 8.
 * If vectors are populated with setValuesInVectors() before, the vectors will be as follows ((N)
 * denotes that the values is marked Null.)
 *            lVector              rVector
 *          -----------          ----------
 *  0            T                    T
 *  1            F                    T
 *  2            T                    F
 *  3            F                    F
 *  4            T (N)                T
 *  5            F (N)                T
 *  6            T (N)                F
 *  7            F (N)                F
 *  8            T                    T (N)
 *  9            F                    T (N)
 *  10           T                    F (N)
 *  11           F                    F (N)
 *  12           T (N)                T (N)
 *  13           F (N)                T (N)
 *  14           T (N)                F (N)
 *  15           F (N)                F (N)
 *             ...                  ...     (Pattern continues)
 * */
static void setNullsInVectors(const shared_ptr<ValueVector>& lVector,
    const shared_ptr<ValueVector>& rVector, uint32_t numTuples) {
    for (int i = 0; i < numTuples; ++i) {
        lVector->setNull(i, ((i >> 2) % 2) == 1);
    }

    for (int i = 0; i < numTuples; ++i) {
        rVector->setNull(i, ((i >> 3) % 2) == 1);
    }
}

class BoolOperandsInSameDataChunkTest : public OperandsInSameDataChunk, public Test {

public:
    DataType getDataTypeOfOperands() override { return BOOL; }
    DataType getDataTypeOfResultVector() override { return BOOL; }

    void SetUp() override {
        initDataChunk();
        setValuesInVectors(vector1, vector2, NUM_TUPLES);
    }
};

class BoolOperandsInDifferentDataChunksTest : public OperandsInDifferentDataChunks, public Test {

public:
    DataType getDataTypeOfOperands() override { return BOOL; }
    DataType getDataTypeOfResultVector() override { return BOOL; }

    void SetUp() override {
        initDataChunk();
        setValuesInVectors(vector1, vector2, NUM_TUPLES);
    }
};

static void checkResultVectorNoNulls(const shared_ptr<ValueVector>& result,
    const function<bool(uint32_t)>& idxOfTrueValueFunc, uint32_t numTuples) {
    auto resultData = (bool*)result->values;
    for (int i = 0; i < numTuples; i++) {
        if (idxOfTrueValueFunc(i)) {
            ASSERT_TRUE(resultData[i]);
        } else {
            ASSERT_FALSE(resultData[i]);
        }
        ASSERT_FALSE(result->isNull(i));
    }
    ASSERT_TRUE(result->hasNoNullsGuarantee());
}

TEST_F(BoolOperandsInSameDataChunkTest, BoolUnaryAndBinaryAllUnflatNoNulls) {

    VectorBooleanOperations::And(*vector1, *vector2, *result);
    // Value at positions {0, 4, 8, 16 ...} are expected to be true
    checkResultVectorNoNulls(
        result, [](uint32_t i) { return i % 4 == 0; }, NUM_TUPLES);

    VectorBooleanOperations::Or(*vector1, *vector2, *result);
    // Value at all positions except {3, 7, 11, 15 ...} are expected to be true
    checkResultVectorNoNulls(
        result, [](uint32_t i) { return i % 4 != 3; }, NUM_TUPLES);

    VectorBooleanOperations::Xor(*vector1, *vector2, *result);
    // Value at positions {1, 5, 8 ...} and {2, 6, 8 ...} are expected to be true
    checkResultVectorNoNulls(
        result, [](uint32_t i) { return i % 4 == 1 || i % 4 == 2; }, NUM_TUPLES);

    VectorBooleanOperations::Not(*vector1, *result);
    // Value at positions {1, 3, 5, 7 ...} are expected to be true
    checkResultVectorNoNulls(
        result, [](uint32_t i) { return i % 2 == 1; }, NUM_TUPLES);
}

// This function works only on the resultant vector that is outcome of the vectors that has been
// populated by the scheme specified above. That is, it assumes that the True/False/Null pattern
// repeat after every 16 elements.
static void checkResultVectorWithNulls(const shared_ptr<ValueVector>& result,
    const unordered_set<uint32_t>& idxOfTrueValuePer16Elements,
    const unordered_set<uint32_t>& idxOfFalseValuePer16Elements, uint32_t numTuples) {
    auto resultData = (bool*)result->values;
    for (int i = 0; i < numTuples; i++) {
        if (idxOfTrueValuePer16Elements.contains(i % 16)) {
            ASSERT_TRUE(resultData[i]);
            ASSERT_FALSE(result->isNull(i));
        } else if (idxOfFalseValuePer16Elements.contains(i % 16)) {
            ASSERT_FALSE(resultData[i]);
            ASSERT_FALSE(result->isNull(i));
        } else {
            ASSERT_TRUE(result->isNull(i));
        }
    }
    ASSERT_FALSE(result->hasNoNullsGuarantee());
}

TEST_F(BoolOperandsInSameDataChunkTest, BoolUnaryAndBinaryAllUnflatWithNulls) {
    setNullsInVectors(vector1, vector2, NUM_TUPLES);

    VectorBooleanOperations::And(*vector1, *vector2, *result);
    checkResultVectorWithNulls(result, {0}, {1, 2, 3, 6, 7, 9, 11}, NUM_TUPLES);

    VectorBooleanOperations::Or(*vector1, *vector2, *result);
    checkResultVectorWithNulls(result, {0, 1, 2, 4, 5, 8, 10}, {3}, NUM_TUPLES);

    VectorBooleanOperations::Xor(*vector1, *vector2, *result);
    checkResultVectorWithNulls(result, {1, 2}, {0, 3}, NUM_TUPLES);

    result->setNullMask(vector1->getNullMask());
    VectorBooleanOperations::Not(*vector1, *result);
    checkResultVectorWithNulls(result, {1, 3, 9, 11}, {0, 2, 8, 10}, NUM_TUPLES);
}

TEST_F(BoolOperandsInDifferentDataChunksTest, BoolBinaryOneFlatOneUnflatNoNulls) {
    // Flatten dataChunkWithVector1, which holds vector1
    dataChunkWithVector1->state->currIdx = 0;

    VectorBooleanOperations::And(*vector1, *vector2, *result);
    // Value at positions {0, 1, 4, 5, 8, 9 ...} are expected to be true
    checkResultVectorNoNulls(
        result, [](uint32_t i) { return (i >> 1) % 2 == 0; }, NUM_TUPLES);

    VectorBooleanOperations::Or(*vector1, *vector2, *result);
    // All values are expected to be true.
    checkResultVectorNoNulls(
        result, [](uint32_t i) { return true; }, NUM_TUPLES);

    VectorBooleanOperations::Xor(*vector1, *vector2, *result);
    // Value at positions {2, 3, 6, 7 ...} are expected to be true
    checkResultVectorNoNulls(
        result, [](uint32_t i) { return (i >> 1) % 2 == 1; }, NUM_TUPLES);
}

TEST_F(BoolOperandsInDifferentDataChunksTest, BoolBinaryOneFlatOneUnflatWithNulls) {
    setNullsInVectors(vector1, vector2, NUM_TUPLES);

    // CASE 1: Flat value is TRUE
    dataChunkWithVector1->state->currIdx = 0;

    VectorBooleanOperations::And(*vector1, *vector2, *result);
    checkResultVectorWithNulls(result, {0, 1, 4, 5}, {2, 3, 6, 7}, NUM_TUPLES);

    VectorBooleanOperations::Or(*vector1, *vector2, *result);
    checkResultVectorWithNulls(
        result, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}, {}, NUM_TUPLES);

    VectorBooleanOperations::Xor(*vector1, *vector2, *result);
    checkResultVectorWithNulls(result, {2, 3, 6, 7}, {0, 1, 4, 5}, NUM_TUPLES);

    // CASE 2: Flat pos has isNull = TRUE
    dataChunkWithVector1->state->currIdx = 4;

    VectorBooleanOperations::And(*vector1, *vector2, *result);
    checkResultVectorWithNulls(result, {}, {2, 3, 6, 7}, NUM_TUPLES);

    VectorBooleanOperations::Or(*vector1, *vector2, *result);
    checkResultVectorWithNulls(result, {0, 1, 4, 5}, {}, NUM_TUPLES);

    VectorBooleanOperations::Xor(*vector1, *vector2, *result);
    checkResultVectorWithNulls(result, {}, {}, NUM_TUPLES);
}

static void checkSelectedPos(uint32_t actualNumSelectedPos, const sel_t* actualSelectedPos,
    const function<bool(uint32_t)>& isSelectedPos, uint32_t numTuples) {
    vector<uint32_t> expectedSelectedPos;
    for (auto i = 0; i < numTuples; i++) {
        if (isSelectedPos(i)) {
            expectedSelectedPos.emplace_back(i);
        }
    }
    EXPECT_EQ(actualNumSelectedPos, expectedSelectedPos.size());
    for (auto i = 0; i < actualNumSelectedPos; i++) {
        EXPECT_EQ(actualSelectedPos[i], expectedSelectedPos[i]);
    }
}

TEST_F(BoolOperandsInSameDataChunkTest, BoolUnaryAndBinarySelectAllUnflatNoNulls) {
    auto selectedPosBuffer = result->state->selectedPositionsBuffer.get();
    auto numSelectedPos = 0u;

    numSelectedPos = VectorBooleanOperations::AndSelect(*vector1, *vector2, selectedPosBuffer);
    // selected positions are multiples of 4 {0, 4, 8, 16 ...}
    checkSelectedPos(
        numSelectedPos, selectedPosBuffer, [](uint32_t i) { return i % 4 == 0; }, NUM_TUPLES);

    numSelectedPos = VectorBooleanOperations::OrSelect(*vector1, *vector2, selectedPosBuffer);
    // selected positions are follow that pattern {0, 1, 2, 4, 5, 6 ...}
    checkSelectedPos(
        numSelectedPos, selectedPosBuffer, [](uint32_t i) { return i % 4 != 3; }, NUM_TUPLES);

    numSelectedPos = VectorBooleanOperations::XorSelect(*vector1, *vector2, selectedPosBuffer);
    // selected positions follow the pattern {1, 2, 5, 6, 9, 10 ...}
    checkSelectedPos(
        numSelectedPos, selectedPosBuffer, [](uint32_t i) { return i % 4 == 1 || i % 4 == 2; },
        NUM_TUPLES);

    numSelectedPos = VectorBooleanOperations::NotSelect(*vector1, selectedPosBuffer);
    // selected positions are all odd positions {1, 3, 5 ...}
    checkSelectedPos(
        numSelectedPos, selectedPosBuffer, [](uint32_t i) { return i % 2 == 1; }, NUM_TUPLES);
}

TEST_F(BoolOperandsInSameDataChunkTest, BoolUnaryAndBinarySelectAllUnflatWithNulls) {
    setNullsInVectors(vector1, vector2, NUM_TUPLES);

    auto selectedPosBuffer = result->state->selectedPositionsBuffer.get();
    auto numSelectedPos = 0u;

    numSelectedPos = VectorBooleanOperations::AndSelect(*vector1, *vector2, selectedPosBuffer);
    // selected positions are multiples of 16 {0, 16, 32, 48 ...}
    checkSelectedPos(
        numSelectedPos, selectedPosBuffer, [](uint32_t i) { return i % 16 == 0; }, NUM_TUPLES);

    numSelectedPos = VectorBooleanOperations::OrSelect(*vector1, *vector2, selectedPosBuffer);
    // selected positions are union of {0, 8, 16 ...}, {2, 10, 18 ...}, {1, 17, 33 ...},
    // {4, 20, 36 ...} and {5, 21, 37 ...}
    checkSelectedPos(
        numSelectedPos, selectedPosBuffer,
        [](uint32_t i) {
            return i % 8 == 0 || i % 8 == 2 || i % 16 == 1 || i % 16 == 4 || i % 16 == 5;
        },
        NUM_TUPLES);

    numSelectedPos = VectorBooleanOperations::XorSelect(*vector1, *vector2, selectedPosBuffer);
    // selected positions are union of {1, 17, 33 ...} and {2, 18, 34 ...}
    checkSelectedPos(
        numSelectedPos, selectedPosBuffer, [](uint32_t i) { return i % 16 == 1 || i % 16 == 2; },
        NUM_TUPLES);

    numSelectedPos = VectorBooleanOperations::NotSelect(*vector1, selectedPosBuffer);
    // selected positions are union of {1, 9, 17 ...} and {3, 11, 19 ...}
    checkSelectedPos(
        numSelectedPos, selectedPosBuffer, [](uint32_t i) { return i % 8 == 1 || i % 8 == 3; },
        NUM_TUPLES);
}

TEST_F(BoolOperandsInDifferentDataChunksTest, BoolBinarySelectOneFlatOneUnflatNoNulls) {
    dataChunkWithVector1->state->currIdx = 0;

    auto selectedPosBuffer = result->state->selectedPositionsBuffer.get();
    auto numSelectedPos = 0u;

    numSelectedPos = VectorBooleanOperations::AndSelect(*vector1, *vector2, selectedPosBuffer);
    // selected positions are union of multiples of 4 and {1, 5, 9 ...}
    checkSelectedPos(
        numSelectedPos, selectedPosBuffer, [](uint32_t i) { return i % 4 == 0 || i % 4 == 1; },
        NUM_TUPLES);

    numSelectedPos = VectorBooleanOperations::OrSelect(*vector1, *vector2, selectedPosBuffer);
    // all positions are selected.
    checkSelectedPos(
        numSelectedPos, selectedPosBuffer, [](uint32_t i) { return true; }, NUM_TUPLES);

    numSelectedPos = VectorBooleanOperations::XorSelect(*vector1, *vector2, selectedPosBuffer);
    // selected positions are union of {2, 6, 10 ...} and {3, 7, 1 ...}
    checkSelectedPos(
        numSelectedPos, selectedPosBuffer, [](uint32_t i) { return i % 4 == 2 || i % 4 == 3; },
        NUM_TUPLES);
}

TEST_F(BoolOperandsInDifferentDataChunksTest, BoolBinarySelectOneFlatOneUnflatWithNulls) {
    setNullsInVectors(vector1, vector2, NUM_TUPLES);
    auto selectedPosBuffer = result->state->selectedPositionsBuffer.get();
    auto numSelectedPos = 0u;

    // CASE 1: Flat value is TRUE
    dataChunkWithVector1->state->currIdx = 0;

    numSelectedPos = VectorBooleanOperations::AndSelect(*vector1, *vector2, selectedPosBuffer);
    // selected positions are union of multiples of 16, {1, 17, 33 ...}, {4, 20, 36 ...} and
    // {5, 21, 37}
    checkSelectedPos(
        numSelectedPos, selectedPosBuffer,
        [](uint32_t i) { return i % 16 == 0 || i % 16 == 1 || i % 16 == 4 || i % 16 == 5; },
        NUM_TUPLES);

    numSelectedPos = VectorBooleanOperations::OrSelect(*vector1, *vector2, selectedPosBuffer);
    // all positions are selected.
    checkSelectedPos(
        numSelectedPos, selectedPosBuffer, [](uint32_t i) { return true; }, NUM_TUPLES);

    numSelectedPos = VectorBooleanOperations::XorSelect(*vector1, *vector2, selectedPosBuffer);
    // selected positions are union of {2, 18, 34 ...}, {3, 19, 35 ...}, {6, 22, 38} and
    // {7, 23, 39 ...}
    checkSelectedPos(
        numSelectedPos, selectedPosBuffer,
        [](uint32_t i) { return i % 16 == 2 || i % 16 == 3 || i % 16 == 6 || i % 16 == 7; },
        NUM_TUPLES);

    // CASE 2: Flat pos has isNull = TRUE
    dataChunkWithVector1->state->currIdx = 4;

    numSelectedPos = VectorBooleanOperations::AndSelect(*vector1, *vector2, selectedPosBuffer);
    // no positions are selected.
    checkSelectedPos(
        numSelectedPos, selectedPosBuffer, [](uint32_t i) { return false; }, NUM_TUPLES);

    numSelectedPos = VectorBooleanOperations::OrSelect(*vector1, *vector2, selectedPosBuffer);
    // selected positions are union of multiples of 16, {1, 17, 33 ...}, {4, 20, 36 ...} and
    // {5, 21, 37}
    checkSelectedPos(
        numSelectedPos, selectedPosBuffer,
        [](uint32_t i) { return i % 16 == 0 || i % 16 == 1 || i % 16 == 4 || i % 16 == 5; },
        NUM_TUPLES);

    numSelectedPos = VectorBooleanOperations::XorSelect(*vector1, *vector2, selectedPosBuffer);
    // no poisitons are selected.
    checkSelectedPos(
        numSelectedPos, selectedPosBuffer, [](uint32_t i) { return false; }, NUM_TUPLES);
}
