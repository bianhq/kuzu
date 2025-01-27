#pragma once

#include "src/binder/query/include/normalized_single_query.h"
#include "src/catalog/include/catalog.h"
#include "src/common/include/join_type.h"
#include "src/planner/include/join_order_enumerator_context.h"
#include "src/storage/store/include/nodes_statistics_and_deleted_ids.h"

using namespace kuzu::catalog;

namespace kuzu {
namespace planner {

class QueryPlanner;
class JoinOrderEnumeratorContext;

/**
 * JoinOrderEnumerator is currently responsible for
 *      join order enumeration
 *      filter push down
 */
class JoinOrderEnumerator {
    friend class ASPOptimizer;

public:
    JoinOrderEnumerator(const Catalog& catalog, const NodesStatisticsAndDeletedIDs& nodesStatistics,
        const RelsStatistics& relsStatistics, QueryPlanner* queryPlanner)
        : catalog{catalog}, nodesStatistics{nodesStatistics}, relsStatistics{relsStatistics},
          queryPlanner{queryPlanner}, context{make_unique<JoinOrderEnumeratorContext>()} {};

    vector<unique_ptr<LogicalPlan>> enumerate(
        const QueryGraphCollection& queryGraphCollection, expression_vector& predicates);

    inline void resetState() { context->resetState(); }

    unique_ptr<JoinOrderEnumeratorContext> enterSubquery(LogicalPlan* outerPlan,
        expression_vector expressionsToScan, vector<shared_ptr<NodeExpression>> nodesToScanTwice);
    void exitSubquery(unique_ptr<JoinOrderEnumeratorContext> prevContext);

    static inline void planMarkJoin(const vector<shared_ptr<NodeExpression>>& joinNodes,
        shared_ptr<Expression> mark, LogicalPlan& probePlan, LogicalPlan& buildPlan) {
        planJoin(joinNodes, JoinType::MARK, mark, probePlan, buildPlan);
    }
    static inline void planInnerHashJoin(const vector<shared_ptr<NodeExpression>>& joinNodes,
        LogicalPlan& probePlan, LogicalPlan& buildPlan) {
        planJoin(joinNodes, JoinType::INNER, nullptr /* mark */, probePlan, buildPlan);
    }
    static inline void planLeftHashJoin(const vector<shared_ptr<NodeExpression>>& joinNodes,
        LogicalPlan& probePlan, LogicalPlan& buildPlan) {
        planJoin(joinNodes, JoinType::LEFT, nullptr /* mark */, probePlan, buildPlan);
    }
    static inline void planCrossProduct(LogicalPlan& probePlan, LogicalPlan& buildPlan) {
        appendCrossProduct(probePlan, buildPlan);
    }

private:
    vector<unique_ptr<LogicalPlan>> planCrossProduct(
        vector<unique_ptr<LogicalPlan>> leftPlans, vector<unique_ptr<LogicalPlan>> rightPlans);

    vector<unique_ptr<LogicalPlan>> enumerate(
        QueryGraph* queryGraph, expression_vector& predicates);

    void planOuterExpressionsScan(expression_vector& expressions);

    void planTableScan();

    void planNodeScan(uint32_t nodePos);
    // Filter push down for node table.
    void planFiltersForNode(expression_vector& predicates, NodeExpression& node, LogicalPlan& plan);
    // Property push down for node table.
    void planPropertyScansForNode(NodeExpression& node, LogicalPlan& plan);

    void planRelScan(uint32_t relPos);
    inline void planRelExtendFiltersAndProperties(shared_ptr<RelExpression>& rel,
        RelDirection direction, expression_vector& predicates, LogicalPlan& plan) {
        appendExtend(rel, direction, plan);
        planFiltersForRel(predicates, *rel, direction, plan);
        planPropertyScansForRel(*rel, direction, plan);
    }
    // Filter push down for rel table.
    void planFiltersForRel(expression_vector& predicates, RelExpression& rel,
        RelDirection direction, LogicalPlan& plan);
    // Property push down for rel table.
    void planPropertyScansForRel(RelExpression& rel, RelDirection direction, LogicalPlan& plan);

    void planLevel(uint32_t level);

    void planWCOJoin(uint32_t leftLevel, uint32_t rightLevel);
    void planWCOJoin(const SubqueryGraph& subgraph, vector<shared_ptr<RelExpression>> rels,
        const shared_ptr<NodeExpression>& intersectNode);

    void planInnerJoin(uint32_t leftLevel, uint32_t rightLevel);

    bool canApplyINLJoin(const SubqueryGraph& subgraph, const SubqueryGraph& otherSubgraph,
        const vector<shared_ptr<NodeExpression>>& joinNodes);
    void planInnerINLJoin(const SubqueryGraph& subgraph, const SubqueryGraph& otherSubgraph,
        const vector<shared_ptr<NodeExpression>>& joinNodes);
    void planInnerHashJoin(const SubqueryGraph& subgraph, const SubqueryGraph& otherSubgraph,
        vector<shared_ptr<NodeExpression>> joinNodes, bool flipPlan);
    // Filter push down for hash join.
    void planFiltersForHashJoin(expression_vector& predicates, LogicalPlan& plan);

    void appendFTableScan(
        LogicalPlan* outerPlan, expression_vector& expressionsToScan, LogicalPlan& plan);

    void appendScanNode(shared_ptr<NodeExpression>& node, LogicalPlan& plan);
    void appendIndexScanNode(shared_ptr<NodeExpression>& node,
        shared_ptr<Expression> indexExpression, LogicalPlan& plan);

    void appendExtend(shared_ptr<RelExpression>& rel, RelDirection direction, LogicalPlan& plan);

    static void planJoin(const vector<shared_ptr<NodeExpression>>& joinNodes, JoinType joinType,
        shared_ptr<Expression> mark, LogicalPlan& probePlan, LogicalPlan& buildPlan);
    static void appendHashJoin(const vector<shared_ptr<NodeExpression>>& joinNodes,
        JoinType joinType, bool isProbeAcc, LogicalPlan& probePlan, LogicalPlan& buildPlan);
    static void appendMarkJoin(const vector<shared_ptr<NodeExpression>>& joinNodes,
        const shared_ptr<Expression>& mark, bool isProbeAcc, LogicalPlan& probePlan,
        LogicalPlan& buildPlan);
    static void appendIntersect(const shared_ptr<NodeExpression>& intersectNode,
        vector<shared_ptr<NodeExpression>>& boundNodes, LogicalPlan& probePlan,
        vector<unique_ptr<LogicalPlan>>& buildPlans);
    static void appendCrossProduct(LogicalPlan& probePlan, LogicalPlan& buildPlan);

    expression_vector getPropertiesForVariable(Expression& expression, Expression& variable);
    uint64_t getExtensionRate(
        table_id_t boundTableID, table_id_t relTableID, RelDirection relDirection);

    static expression_vector getNewlyMatchedExpressions(const SubqueryGraph& prevSubgraph,
        const SubqueryGraph& newSubgraph, const expression_vector& expressions) {
        return getNewlyMatchedExpressions(
            vector<SubqueryGraph>{prevSubgraph}, newSubgraph, expressions);
    }
    static expression_vector getNewlyMatchedExpressions(const vector<SubqueryGraph>& prevSubgraphs,
        const SubqueryGraph& newSubgraph, const expression_vector& expressions);
    static bool isExpressionNewlyMatched(const vector<SubqueryGraph>& prevSubgraphs,
        const SubqueryGraph& newSubgraph, Expression& expression);

private:
    const catalog::Catalog& catalog;
    const storage::NodesStatisticsAndDeletedIDs& nodesStatistics;
    const storage::RelsStatistics& relsStatistics;
    QueryPlanner* queryPlanner;
    unique_ptr<JoinOrderEnumeratorContext> context;
};

} // namespace planner
} // namespace kuzu
