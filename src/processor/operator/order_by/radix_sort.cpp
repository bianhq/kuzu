#include "include/radix_sort.h"

#include <algorithm>

#include "src/function/comparison/operations/include/comparison_operations.h"

using namespace kuzu::function::operation;

namespace kuzu {
namespace processor {

void RadixSort::sortSingleKeyBlock(const DataBlock& keyBlock) {
    auto numBytesSorted = 0ul;
    auto numTuplesInKeyBlock = keyBlock.numTuples;
    queue<TieRange> ties;
    // We need to sort the whole keyBlock for the first radix sort, so just mark all tuples as a
    // tie.
    ties.push(TieRange{0, numTuplesInKeyBlock - 1});
    for (auto i = 0u; i < stringAndUnstructuredKeyColInfo.size(); i++) {
        const auto numBytesToSort = stringAndUnstructuredKeyColInfo[i].colOffsetInEncodedKeyBlock -
                                    numBytesSorted +
                                    stringAndUnstructuredKeyColInfo[i].getEncodingSize();
        const auto numOfTies = ties.size();
        for (auto j = 0u; j < numOfTies; j++) {
            auto keyBlockTie = ties.front();
            ties.pop();
            radixSort(keyBlock.getData() + keyBlockTie.startingTupleIdx * numBytesPerTuple,
                keyBlockTie.getNumTuples(), numBytesSorted, numBytesToSort);

            auto newTiesInKeyBlock =
                findTies(keyBlock.getData() + keyBlockTie.startingTupleIdx * numBytesPerTuple +
                             numBytesSorted,
                    keyBlockTie.getNumTuples(), numBytesToSort, keyBlockTie.startingTupleIdx);
            for (auto& newTieInKeyBlock : newTiesInKeyBlock) {
                solveStringAndUnstructuredTies(newTieInKeyBlock,
                    keyBlock.getData() + newTieInKeyBlock.startingTupleIdx * numBytesPerTuple, ties,
                    stringAndUnstructuredKeyColInfo[i]);
            }
        }
        if (ties.empty()) {
            return;
        }
        numBytesSorted += numBytesToSort;
    }

    if (numBytesSorted < numBytesPerTuple) {
        while (!ties.empty()) {
            auto tie = ties.front();
            ties.pop();
            radixSort(keyBlock.getData() + tie.startingTupleIdx * numBytesPerTuple,
                tie.getNumTuples(), numBytesSorted, numBytesToRadixSort - numBytesSorted);
        }
    }
}

void RadixSort::radixSort(uint8_t* keyBlockPtr, uint32_t numTuplesToSort, uint32_t numBytesSorted,
    uint32_t numBytesToSort) {
    // We use radixSortLSD which sorts from the least significant byte to the most significant byte.
    auto tmpKeyBlockPtr = tmpSortingResultBlock->getData();
    keyBlockPtr += numBytesSorted;
    tmpKeyBlockPtr += numBytesSorted;
    constexpr uint16_t countingArraySize = 256;
    uint32_t count[countingArraySize];
    auto isInTmpBlock = false;
    for (auto curByteIdx = 1ul; curByteIdx <= numBytesToSort; curByteIdx++) {
        memset(count, 0, countingArraySize * sizeof(uint32_t));
        auto sourcePtr = isInTmpBlock ? tmpKeyBlockPtr : keyBlockPtr;
        auto targetPtr = isInTmpBlock ? keyBlockPtr : tmpKeyBlockPtr;
        auto curByteOffset = numBytesToSort - curByteIdx;
        auto sortBytePtr = sourcePtr + curByteOffset;
        // counting sort
        for (auto j = 0ul; j < numTuplesToSort; j++) {
            count[*sortBytePtr]++;
            sortBytePtr += numBytesPerTuple;
        }
        auto maxCounter = count[0];
        for (auto val = 1ul; val < countingArraySize; val++) {
            maxCounter = max(count[val], maxCounter);
            count[val] = count[val] + count[val - 1];
        }
        // If all bytes have the same value (tie), continue on the next byte.
        if (maxCounter == numTuplesToSort) {
            continue;
        }
        // Reorder the data based on the count array.
        auto sourceTuplePtr = sourcePtr + (numTuplesToSort - 1) * numBytesPerTuple;
        for (auto j = 0ul; j < numTuplesToSort; j++) {
            auto targetTupleNum = --count[*(sourceTuplePtr + curByteOffset)];
            memcpy(targetPtr + targetTupleNum * numBytesPerTuple, sourceTuplePtr, numBytesPerTuple);
            sourceTuplePtr -= numBytesPerTuple;
        }
        isInTmpBlock = !isInTmpBlock;
    }
    // If the data is in the tmp block, copy the data from tmp block back.
    if (isInTmpBlock) {
        memcpy(keyBlockPtr, tmpKeyBlockPtr, numTuplesToSort * numBytesPerTuple);
    }
}

vector<TieRange> RadixSort::findTies(uint8_t* keyBlockPtr, uint32_t numTuplesToFindTies,
    uint32_t numBytesToSort, uint32_t baseTupleIdx) {
    vector<TieRange> newTiesInKeyBlock;
    auto iTuplePtr = keyBlockPtr;
    for (auto i = 0u; i < numTuplesToFindTies - 1; i++) {
        auto j = i + 1;
        auto jTuplePtr = iTuplePtr + numBytesPerTuple;
        for (; j < numTuplesToFindTies; j++) {
            if (memcmp(iTuplePtr, jTuplePtr, numBytesToSort) != 0) {
                break;
            }
            jTuplePtr += numBytesPerTuple;
        }
        j--;
        if (i != j) {
            newTiesInKeyBlock.emplace_back(TieRange(i + baseTupleIdx, j + baseTupleIdx));
        }
        iTuplePtr = jTuplePtr;
        i = j;
    }
    return newTiesInKeyBlock;
}

void RadixSort::fillTmpTuplePtrSortingBlock(TieRange& keyBlockTie, uint8_t* keyBlockPtr) {
    auto tmpTuplePtrSortingBlockPtr = (uint8_t**)tmpTuplePtrSortingBlock->getData();
    for (auto i = 0ul; i < keyBlockTie.getNumTuples(); i++) {
        tmpTuplePtrSortingBlockPtr[i] = keyBlockPtr;
        keyBlockPtr += numBytesPerTuple;
    }
}

void RadixSort::reOrderKeyBlock(TieRange& keyBlockTie, uint8_t* keyBlockPtr) {
    auto tmpTuplePtrSortingBlockPtr = (uint8_t**)tmpTuplePtrSortingBlock->getData();
    auto tmpKeyBlockPtr = tmpSortingResultBlock->getData();
    for (auto i = 0ul; i < keyBlockTie.getNumTuples(); i++) {
        memcpy(tmpKeyBlockPtr, tmpTuplePtrSortingBlockPtr[i], numBytesPerTuple);
        tmpKeyBlockPtr += numBytesPerTuple;
    }
    memcpy(keyBlockPtr, tmpSortingResultBlock->getData(),
        keyBlockTie.getNumTuples() * numBytesPerTuple);
}

template<typename TYPE>
void RadixSort::findStringAndUnstructuredTies(TieRange& keyBlockTie, uint8_t* keyBlockPtr,
    queue<TieRange>& ties, StringAndUnstructuredKeyColInfo& keyColInfo) {
    auto iTuplePtr = keyBlockPtr;
    for (auto i = keyBlockTie.startingTupleIdx; i < keyBlockTie.endingTupleIdx; i++) {
        bool isIValNull = OrderByKeyEncoder::isNullVal(
            iTuplePtr + keyColInfo.colOffsetInEncodedKeyBlock, keyColInfo.isAscOrder);
        // This variable will only be used when the current column is a string column. Otherwise,
        // we just set this variable to false.
        bool isIStringLong =
            keyColInfo.isStrCol &&
            OrderByKeyEncoder::isLongStr(
                iTuplePtr + keyColInfo.colOffsetInEncodedKeyBlock, keyColInfo.isAscOrder);
        TYPE iValue =
            isIValNull ?
                TYPE() :
                factorizedTable.getData<TYPE>(
                    OrderByKeyEncoder::getEncodedFTBlockIdx(iTuplePtr + numBytesToRadixSort),
                    OrderByKeyEncoder::getEncodedFTBlockOffset(iTuplePtr + numBytesToRadixSort),
                    keyColInfo.colOffsetInFT);
        auto j = i + 1;
        auto jTuplePtr = iTuplePtr + numBytesPerTuple;
        for (; j <= keyBlockTie.endingTupleIdx; j++) {
            auto jTupleInfoPtr = jTuplePtr + numBytesToRadixSort;
            bool isJValNull = OrderByKeyEncoder::isNullVal(
                jTuplePtr + keyColInfo.colOffsetInEncodedKeyBlock, keyColInfo.isAscOrder);
            if (isIValNull && isJValNull) {
                // If the left value and the right value are nulls, we can just continue on
                // the next tuple.
                jTupleInfoPtr += numBytesPerTuple;
                continue;
            } else if (isIValNull || isJValNull) {
                // If only one value is null, we can just conclude that those two values are
                // not equal.
                break;
            }
            if constexpr (is_same<TYPE, ku_string_t>::value) {
                // We do an optimization here to minimize the number of times that we fetch
                // tuples from factorizedTable. If both left and right string are short, they
                // must equal to each other (since they have the same prefix). If one string is
                // short and the other string is long, then they must not equal to each other.
                bool isJStringLong = OrderByKeyEncoder::isLongStr(
                    jTuplePtr + keyColInfo.colOffsetInEncodedKeyBlock, keyColInfo.isAscOrder);
                if (!isIStringLong && !isJStringLong) {
                    jTupleInfoPtr += numBytesPerTuple;
                    continue;
                } else if (isIStringLong != isJStringLong) {
                    break;
                }
            }

            uint8_t result;
            NotEquals::operation<TYPE, TYPE>(iValue,
                factorizedTable.getData<TYPE>(
                    OrderByKeyEncoder::getEncodedFTBlockIdx(jTupleInfoPtr),
                    OrderByKeyEncoder::getEncodedFTBlockOffset(jTupleInfoPtr),
                    keyColInfo.colOffsetInFT),
                result);
            if (result) {
                break;
            }
            jTuplePtr += numBytesPerTuple;
        }
        j--;
        if (i != j) {
            ties.push(TieRange(i, j));
        }
        i = j;
        iTuplePtr = jTuplePtr;
    }
}

void RadixSort::solveStringAndUnstructuredTies(TieRange& keyBlockTie, uint8_t* keyBlockPtr,
    queue<TieRange>& ties, StringAndUnstructuredKeyColInfo& keyColInfo) {
    fillTmpTuplePtrSortingBlock(keyBlockTie, keyBlockPtr);
    auto tmpTuplePtrSortingBlockPtr = (uint8_t**)tmpTuplePtrSortingBlock->getData();
    sort(tmpTuplePtrSortingBlockPtr, tmpTuplePtrSortingBlockPtr + keyBlockTie.getNumTuples(),
        [this, keyColInfo](const uint8_t* leftPtr, const uint8_t* rightPtr) -> bool {
            // Handle null value comparison.
            if (OrderByKeyEncoder::isNullVal(
                    rightPtr + keyColInfo.colOffsetInEncodedKeyBlock, keyColInfo.isAscOrder)) {
                return keyColInfo.isAscOrder;
            } else if (OrderByKeyEncoder::isNullVal(leftPtr + keyColInfo.colOffsetInEncodedKeyBlock,
                           keyColInfo.isAscOrder)) {
                return !keyColInfo.isAscOrder;
            }

            if (keyColInfo.isStrCol) {
                // We only need to fetch the actual strings from the
                // factorizedTable when both left and right strings are long string.
                auto isLeftLongStr = OrderByKeyEncoder::isLongStr(
                    leftPtr + keyColInfo.colOffsetInEncodedKeyBlock, keyColInfo.isAscOrder);
                auto isRightLongStr = OrderByKeyEncoder::isLongStr(
                    rightPtr + keyColInfo.colOffsetInEncodedKeyBlock, keyColInfo.isAscOrder);
                if (!isLeftLongStr && !isRightLongStr) {
                    // If left and right are both short string and have the same prefix, we can't
                    // conclude that the left string is smaller than the right string.
                    return false;
                } else if (isLeftLongStr && !isRightLongStr) {
                    // If left string is a long string and right string is a short string, we can
                    // conclude that the left string must be greater than the right string.
                    return !keyColInfo.isAscOrder;
                } else if (isRightLongStr && !isLeftLongStr) {
                    // If right string is a long string and left string is a short string, we can
                    // conclude that the right string must be greater than the left string.
                    return keyColInfo.isAscOrder;
                }
            }

            auto leftTupleInfoPtr = leftPtr + numBytesToRadixSort;
            auto rightTupleInfoPtr = rightPtr + numBytesToRadixSort;
            const auto leftBlockIdx = OrderByKeyEncoder::getEncodedFTBlockIdx(leftTupleInfoPtr);
            const auto leftBlockOffset =
                OrderByKeyEncoder::getEncodedFTBlockOffset(leftTupleInfoPtr);
            const auto rightBlockIdx = OrderByKeyEncoder::getEncodedFTBlockIdx(rightTupleInfoPtr);
            const auto rightBlockOffset =
                OrderByKeyEncoder::getEncodedFTBlockOffset(rightTupleInfoPtr);

            if (keyColInfo.isStrCol) {
                auto result = (keyColInfo.isAscOrder ==
                               (factorizedTable.getData<ku_string_t>(
                                    leftBlockIdx, leftBlockOffset, keyColInfo.colOffsetInFT) <
                                   factorizedTable.getData<ku_string_t>(
                                       rightBlockIdx, rightBlockOffset, keyColInfo.colOffsetInFT)));
                return result;
            } else {
                // The comparison function does the type checking for the unstructured values. If
                // there is a type mismatch, the comparison function will throw an exception. Note:
                // we may loose precision if we compare DOUBLE and INT64 For example: DOUBLE: a =
                // 2^57, INT64: b = 2^57 + 3. Although a < b, the LessThan function may still output
                // false.
                uint8_t result;
                LessThan::operation<Value, Value>(factorizedTable.getData<Value>(leftBlockIdx,
                                                      leftBlockOffset, keyColInfo.colOffsetInFT),
                    factorizedTable.getData<Value>(
                        rightBlockIdx, rightBlockOffset, keyColInfo.colOffsetInFT),
                    result);
                return keyColInfo.isAscOrder == result;
            }
        });
    reOrderKeyBlock(keyBlockTie, keyBlockPtr);
    if (keyColInfo.isStrCol) {
        findStringAndUnstructuredTies<ku_string_t>(keyBlockTie, keyBlockPtr, ties, keyColInfo);
    } else {
        findStringAndUnstructuredTies<Value>(keyBlockTie, keyBlockPtr, ties, keyColInfo);
    }
}

} // namespace processor
} // namespace kuzu
