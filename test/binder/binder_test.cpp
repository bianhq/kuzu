#include "gtest/gtest.h"
#include "test/mock/mock_catalog.h"

#include "src/binder/include/binder.h"
#include "src/common/include/configs.h"
#include "src/parser/include/parser.h"

using namespace kuzu::parser;
using namespace kuzu::binder;

using ::testing::Test;

class BinderTest : public Test {

public:
    void SetUp() override { catalog.setUp(); }

protected:
    NiceMock<TinySnbCatalog> catalog;
};

TEST_F(BinderTest, VarLenExtendMaxDepthTest) {
    // If the upper bound of the varLenEtend is larger than VAR_LENGTH_EXTEND_MAX_DEPTH, the
    // upper bound will be set to VAR_LENGTH_EXTEND_MAX_DEPTH.
    auto input = "MATCH (a:person)-[:knows*2..32]->(b:person) return count(*)";
    auto boundRegularQuery =
        Binder(catalog).bind(*reinterpret_cast<RegularQuery*>(Parser::parseQuery(input).get()));
    auto normalizedQueryPart =
        ((BoundRegularQuery*)boundRegularQuery.get())->getSingleQuery(0)->getQueryPart(0);
    for (auto i = 0u; i < normalizedQueryPart->getNumReadingClause(); i++) {
        ASSERT_EQ(normalizedQueryPart->getReadingClause(i)->getClauseType(), ClauseType::MATCH);
        auto boundMatchClause = (BoundMatchClause*)normalizedQueryPart->getReadingClause(i);
        auto queryRel =
            boundMatchClause->getQueryGraphCollection()->getQueryGraph(0)->getQueryRel(0);
        ASSERT_EQ(queryRel->getLowerBound(), 2);
        ASSERT_EQ(queryRel->getUpperBound(), VAR_LENGTH_EXTEND_MAX_DEPTH);
    }
}
