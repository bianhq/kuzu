#pragma once

#include "src/processor/include/physical_plan/operator/morsel.h"
#include "src/processor/include/physical_plan/operator/physical_operator.h"

namespace graphflow {
namespace processor {

class ScanNodeID : public PhysicalOperator {

public:
    ScanNodeID(const DataPos& outDataPos, const shared_ptr<MorselsDesc>& morsel,
        ExecutionContext& context, uint32_t id)
        : PhysicalOperator{SCAN, context, id}, outDataPos{outDataPos}, morsel{morsel} {}

    ScanNodeID(const DataPos& outDataPos, const shared_ptr<MorselsDesc>& morsel,
        unique_ptr<PhysicalOperator> prevOperator, ExecutionContext& context, uint32_t id)
        : PhysicalOperator{move(prevOperator), SCAN, context, id},
          outDataPos{outDataPos}, morsel{morsel} {}

    void initResultSet(const shared_ptr<ResultSet>& resultSet) override;

    bool getNextTuples() override;

    unique_ptr<PhysicalOperator> clone() override {
        return prevOperator ?
                   make_unique<ScanNodeID>(outDataPos, morsel, prevOperator->clone(), context, id) :
                   make_unique<ScanNodeID>(outDataPos, morsel, context, id);
    }

private:
    DataPos outDataPos;
    shared_ptr<MorselsDesc> morsel;

    shared_ptr<DataChunk> outDataChunk;
    shared_ptr<ValueVector> outValueVector;
};

} // namespace processor
} // namespace graphflow