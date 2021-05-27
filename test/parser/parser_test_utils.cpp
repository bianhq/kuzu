#include "test/parser/parser_test_utils.h"

bool ParserTestUtils::equals(const LoadCSVStatement& left, const LoadCSVStatement& right) {
    return left.statementType == right.statementType &&
           left.fieldTerminator == right.fieldTerminator &&
           equals(*left.inputExpression, *right.inputExpression);
}

bool ParserTestUtils::equals(const MatchStatement& left, const MatchStatement& right) {
    auto result = left.statementType == right.statementType &&
                  left.graphPattern.size() == right.graphPattern.size();
    if (left.whereClause) {
        result &= right.whereClause && equals(*left.whereClause, *right.whereClause);
    }
    if (!result) {
        return false;
    }
    for (auto i = 0u; i < left.graphPattern.size(); ++i) {
        if (!equals(*left.graphPattern[i], *right.graphPattern[i])) {
            return false;
        }
    }
    return true;
}

bool ParserTestUtils::equals(const WithStatement& left, const WithStatement& right) {
    auto result = left.containsStar == right.containsStar &&
                  left.expressions.size() == right.expressions.size();
    if (left.whereClause) {
        result &= right.whereClause && equals(*left.whereClause, *right.whereClause);
    }
    if (!result)
        return false;
    for (auto i = 0u; i < left.expressions.size(); ++i) {
        if (!equals(*left.expressions[i], *right.expressions[i])) {
            return false;
        }
    }
    return true;
}

bool ParserTestUtils::equals(const ReturnStatement& left, const ReturnStatement& right) {
    auto result = left.containsStar == right.containsStar &&
                  left.expressions.size() == right.expressions.size();
    if (!result)
        return false;
    for (auto i = 0u; i < left.expressions.size(); ++i) {
        if (!equals(*left.expressions[i], *right.expressions[i])) {
            return false;
        }
    }
    return true;
}

bool ParserTestUtils::equals(const ParsedExpression& left, const ParsedExpression& right) {
    auto result = left.type == right.type && left.text == right.text && left.alias == right.alias &&
                  left.children.size() == right.children.size();
    if (!result)
        return false;
    for (auto i = 0u; i < left.children.size(); ++i) {
        if (!equals(*left.children[i], *right.children[i])) {
            return false;
        }
    }
    return true;
}

bool ParserTestUtils::equals(const PatternElement& left, const PatternElement& right) {
    auto result = equals(*left.nodePattern, *right.nodePattern) &&
                  left.patternElementChains.size() == right.patternElementChains.size();
    if (!result)
        return false;
    for (auto i = 0u; i < left.patternElementChains.size(); ++i) {
        if (!equals(*left.patternElementChains[i], *right.patternElementChains[i])) {
            return false;
        }
    }
    return true;
}

bool ParserTestUtils::equals(const PatternElementChain& left, const PatternElementChain& right) {
    return equals(*left.nodePattern, *right.nodePattern) &&
           equals(*left.relPattern, *right.relPattern);
}

bool ParserTestUtils::equals(const RelPattern& left, const RelPattern& right) {
    return left.name == right.name && left.label == right.label &&
           left.arrowDirection == right.arrowDirection;
}

bool ParserTestUtils::equals(const NodePattern& left, const NodePattern& right) {
    return left.name == right.name && left.label == right.label;
}