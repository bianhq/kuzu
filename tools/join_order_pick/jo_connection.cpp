#include "include/jo_connection.h"

#include "src/binder/include/binder.h"
#include "src/parser/include/parser.h"
#include "src/planner/include/planner.h"
#include "src/planner/logical_plan/include/logical_plan_util.h"
#include "src/processor/mapper/include/plan_mapper.h"

namespace kuzu {
namespace main {

unique_ptr<QueryResult> JOConnection::query(const string& query, const string& encodedJoin) {
    assert(!query.empty());
    lock_t lck{mtx};
    auto preparedStatement = make_unique<PreparedStatement>();
    try {
        auto statement = Parser::parseQuery(query);
        assert(statement->getStatementType() == StatementType::QUERY);
        auto parsedQuery = (RegularQuery*)statement.get();
        auto boundQuery = Binder(*database->catalog).bind(*parsedQuery);
        auto& nodesStatisticsAndDeletedIDs =
            database->storageManager->getNodesStore().getNodesStatisticsAndDeletedIDs();
        auto& relsStatistics = database->storageManager->getRelsStore().getRelsStatistics();
        unique_ptr<LogicalPlan> logicalPlan;
        if (encodedJoin.empty()) {
            logicalPlan = Planner::getBestPlan(
                *database->catalog, nodesStatisticsAndDeletedIDs, relsStatistics, *boundQuery);
        } else {
            for (auto& plan : Planner::getAllPlans(*database->catalog, nodesStatisticsAndDeletedIDs,
                     relsStatistics, *boundQuery)) {
                if (LogicalPlanUtil::encodeJoin(*plan) == encodedJoin) {
                    logicalPlan = move(plan);
                }
            }
            if (logicalPlan == nullptr) {
                throw ConnectionException("Cannot find a plan matching " + encodedJoin);
            }
        }
        preparedStatement->createResultHeader(logicalPlan->getExpressionsToCollect());
        preparedStatement->logicalPlan = std::move(logicalPlan);
        return executeAndAutoCommitIfNecessaryNoLock(preparedStatement.get());
    } catch (Exception& exception) {
        string errMsg = exception.what();
        return queryResultWithError(errMsg);
    }
}

} // namespace main
} // namespace kuzu
