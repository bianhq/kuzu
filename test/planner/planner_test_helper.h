#include "gtest/gtest.h"
#include "test/mock/mock_catalog.h"

#include "src/binder/include/binder.h"
#include "src/parser/include/parser.h"
#include "src/planner/include/planner.h"
#include "src/planner/logical_plan/include/logical_plan_util.h"
#include "src/storage/store/include/nodes_statistics_and_deleted_ids.h"

using ::testing::NiceMock;
using ::testing::Test;

using namespace graphflow::planner;
using namespace graphflow::storage;

class PlannerTest : public Test {

public:
    void SetUp() override {
        catalog.setUp();
        vector<unique_ptr<NodeStatisticsAndDeletedIDs>> nodeStatisticsAndDeletedIDs;
        nodeStatisticsAndDeletedIDs.push_back(
            make_unique<NodeStatisticsAndDeletedIDs>(PERSON_TABLE_ID, NUM_PERSON_NODES));
        nodeStatisticsAndDeletedIDs.push_back(make_unique<NodeStatisticsAndDeletedIDs>(
            ORGANISATION_TABLE_ID, NUM_ORGANISATION_NODES));
        mockNodeStatisticsAndDeletedIDs =
            make_unique<NodesStatisticsAndDeletedIDs>(nodeStatisticsAndDeletedIDs);
        auto knowsRelStatistics = make_unique<RelStatistics>(
            14 /* numRels */, vector<unordered_map<table_id_t, uint64_t>>{
                                  unordered_map<table_id_t, uint64_t>{{0, 14}, {1, 0}},
                                  unordered_map<table_id_t, uint64_t>{
                                      {0, 14}, {1, 0}}} /* numRelsPerDirectionBoundTable */);
        auto studyAtRelStatistics = make_unique<RelStatistics>(
            3 /* numRels */, vector<unordered_map<table_id_t, uint64_t>>{
                                 unordered_map<table_id_t, uint64_t>{{0, 3}, {1, 0}},
                                 unordered_map<table_id_t, uint64_t>{
                                     {0, 0}, {1, 3}}} /* numRelsPerDirectionBoundTable */);
        auto workAtRelStatistics = make_unique<RelStatistics>(
            3 /* numRels */, vector<unordered_map<table_id_t, uint64_t>>{
                                 unordered_map<table_id_t, uint64_t>{{0, 3}, {1, 0}},
                                 unordered_map<table_id_t, uint64_t>{
                                     {0, 0}, {1, 3}}} /* numRelsPerDirectionBoundTable */);
        auto meetsRelStatistics = make_unique<RelStatistics>(
            7 /* numRels */, vector<unordered_map<table_id_t, uint64_t>>{
                                 unordered_map<table_id_t, uint64_t>{{0, 7}, {1, 0}},
                                 unordered_map<table_id_t, uint64_t>{
                                     {0, 7}, {1, 0}}} /* numRelsPerDirectionBoundTable */);
        vector<unique_ptr<RelStatistics>> relStatistics;
        relStatistics.push_back(move(knowsRelStatistics));
        relStatistics.push_back(move(studyAtRelStatistics));
        relStatistics.push_back(move(workAtRelStatistics));
        relStatistics.push_back(move(meetsRelStatistics));
        mockRelStatistic = make_unique<RelsStatistics>(move(relStatistics));
    }

    unique_ptr<LogicalPlan> getBestPlan(const string& query) {
        auto statement = Parser::parseQuery(query);
        auto parsedQuery = (RegularQuery*)statement.get();
        auto boundQuery = Binder(catalog).bind(*parsedQuery);
        return Planner::getBestPlan(
            catalog, *mockNodeStatisticsAndDeletedIDs, *mockRelStatistic, *boundQuery);
    }

    bool containSubstr(const string& str, const string& substr) {
        return str.find(substr) != string::npos;
    }

private:
    NiceMock<TinySnbCatalog> catalog;
    unique_ptr<NodesStatisticsAndDeletedIDs> mockNodeStatisticsAndDeletedIDs;
    unique_ptr<RelsStatistics> mockRelStatistic;
};
