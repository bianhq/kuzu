#pragma once

#include "src/common/include/vector/node_vector.h"
#include "src/processor/include/operator/physical/operator.h"
#include "src/processor/include/task_system/morsel.h"
#include "src/storage/include/structures/column.h"

using namespace graphflow::storage;

namespace graphflow {
namespace processor {

class ColumnReader : public Operator {

public:
    ColumnReader(const uint64_t& dataChunkPos, const uint64_t& valueVectorPos, BaseColumn* column,
        unique_ptr<Operator> prevOperator);

    ~ColumnReader() { column->reclaim(handle); }

    void getNextTuples() override;

protected:
    uint64_t dataChunkPos;
    uint64_t valueVectorPos;
    shared_ptr<DataChunk> inDataChunk;
    shared_ptr<NodeIDVector> inNodeIDVector;
    shared_ptr<ValueVector> outValueVector;

    BaseColumn* column;
    unique_ptr<ColumnOrListsHandle> handle;
};

} // namespace processor
} // namespace graphflow