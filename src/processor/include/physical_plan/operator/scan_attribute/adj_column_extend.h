#pragma once

#include "src/processor/include/physical_plan/operator/filtering_operator.h"
#include "src/processor/include/physical_plan/operator/scan_attribute/scan_column.h"

namespace graphflow {
namespace processor {

class AdjColumnExtend : public ScanColumn, public FilteringOperator {

public:
    AdjColumnExtend(uint64_t dataChunkPos, uint64_t valueVectorPos, Column* column,
        label_t outNodeIDVectorLabel, unique_ptr<PhysicalOperator> prevOperator,
        ExecutionContext& context, uint32_t id);

    void reInitialize() override;

    void getNextTuples() override;

    unique_ptr<PhysicalOperator> clone() override {
        return make_unique<AdjColumnExtend>(dataChunkPos, valueVectorPos, column,
            outNodeIDVectorLabel, prevOperator->clone(), context, id);
    }

private:
    label_t outNodeIDVectorLabel;
};

} // namespace processor
} // namespace graphflow
