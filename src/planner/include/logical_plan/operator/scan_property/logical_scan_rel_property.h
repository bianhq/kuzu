#pragma once

#include "src/planner/include/logical_plan/operator/logical_operator.h"

namespace graphflow {
namespace planner {

class LogicalScanRelProperty : public LogicalOperator {

public:
    LogicalScanRelProperty(string boundNodeID, label_t boundNodeLabel, string nbrNodeID,
        label_t relLabel, Direction direction, string propertyVariableName, uint32_t propertyKey,
        bool isColumn, shared_ptr<LogicalOperator> prevOperator)
        : LogicalOperator{prevOperator}, boundNodeID{move(boundNodeID)},
          boundNodeLabel{boundNodeLabel}, nbrNodeID{move(nbrNodeID)}, relLabel{relLabel},
          direction{direction}, propertyVariableName(move(propertyVariableName)),
          propertyKey{propertyKey}, isColumn{isColumn} {}

    LogicalOperatorType getLogicalOperatorType() const override {
        return LogicalOperatorType::LOGICAL_SCAN_REL_PROPERTY;
    }

    string getOperatorInformation() const override { return propertyVariableName; }

public:
    string boundNodeID;
    label_t boundNodeLabel;
    string nbrNodeID;
    label_t relLabel;
    Direction direction;
    string propertyVariableName;
    uint32_t propertyKey;
    bool isColumn;
};

} // namespace planner
} // namespace graphflow