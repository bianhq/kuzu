#include "include/plan_mapper.h"

#include "src/planner/logical_plan/logical_operator/include/logical_limit.h"
#include "src/processor/operator/include/limit.h"

namespace kuzu {
namespace processor {

unique_ptr<PhysicalOperator> PlanMapper::mapLogicalLimitToPhysical(
    LogicalOperator* logicalOperator, MapperContext& mapperContext) {
    auto& logicalLimit = (const LogicalLimit&)*logicalOperator;
    auto prevOperator = mapLogicalOperatorToPhysical(logicalOperator->getChild(0), mapperContext);
    auto dataChunkToSelectPos = logicalLimit.getGroupPosToSelect();
    return make_unique<Limit>(logicalLimit.getLimitNumber(), make_shared<atomic_uint64_t>(0),
        dataChunkToSelectPos, logicalLimit.getGroupsPosToLimit(), move(prevOperator),
        getOperatorID(), logicalLimit.getExpressionsForPrinting());
}

} // namespace processor
} // namespace kuzu
