#pragma once

#include "src/binder/expression/include/expression.h"
#include "src/expression_evaluator/include/base_evaluator.h"
#include "src/processor/include/execution_context.h"
#include "src/processor/mapper/include/mapper_context.h"
#include "src/processor/result/include/result_set.h"
#include "src/processor/result/include/result_set_descriptor.h"

using namespace kuzu::binder;
using namespace kuzu::evaluator;

namespace kuzu {
namespace processor {

class PlanMapper;

class ExpressionMapper {

public:
    unique_ptr<BaseExpressionEvaluator> mapExpression(
        const shared_ptr<Expression>& expression, const MapperContext& mapperContext);

private:
    unique_ptr<BaseExpressionEvaluator> mapLiteralExpression(
        const shared_ptr<Expression>& expression);

    unique_ptr<BaseExpressionEvaluator> mapParameterExpression(
        const shared_ptr<Expression>& expression);

    unique_ptr<BaseExpressionEvaluator> mapReferenceExpression(
        const shared_ptr<Expression>& expression, const MapperContext& mapperContext);

    unique_ptr<BaseExpressionEvaluator> mapFunctionExpression(
        const shared_ptr<Expression>& expression, const MapperContext& mapperContext);
};

} // namespace processor
} // namespace kuzu
