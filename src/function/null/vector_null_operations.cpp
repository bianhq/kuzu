#include "include/vector_null_operations.h"

#include "operations/include/null_operations.h"

namespace kuzu {
namespace function {

scalar_exec_func VectorNullOperations::bindExecFunction(
    ExpressionType expressionType, const expression_vector& children) {
    assert(children.size() == 1);
    return bindUnaryExecFunction(expressionType, children);
}

scalar_select_func VectorNullOperations::bindSelectFunction(
    ExpressionType expressionType, const expression_vector& children) {
    assert(children.size() == 1);
    return bindUnarySelectFunction(expressionType, children);
}

scalar_exec_func VectorNullOperations::bindUnaryExecFunction(
    ExpressionType expressionType, const expression_vector& children) {
    assert(children.size() == 1);
    switch (expressionType) {
    case IS_NULL: {
        return UnaryNullExecFunction<operation::IsNull>;
    }
    case IS_NOT_NULL: {
        return UnaryNullExecFunction<operation::IsNotNull>;
    }
    default:
        assert(false);
    }
}

scalar_select_func VectorNullOperations::bindUnarySelectFunction(
    ExpressionType expressionType, const expression_vector& children) {
    assert(children.size() == 1);
    switch (expressionType) {
    case IS_NULL: {
        return UnaryNullSelectFunction<operation::IsNull>;
    }
    case IS_NOT_NULL: {
        return UnaryNullSelectFunction<operation::IsNotNull>;
    }
    default:
        assert(false);
    }
}

} // namespace function
} // namespace kuzu
