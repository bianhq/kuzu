#include <string>

#include "include/gtest/gtest.h"

#include "src/common/types/include/types_include.h"

using namespace kuzu::common;
using namespace std;

TEST(DateTests, IsLeapYearTest) {
    EXPECT_TRUE(Date::IsLeapYear(2000));
    EXPECT_FALSE(Date::IsLeapYear(2001));
}

TEST(DateTests, FromDateConvertGivesSame) {
    int32_t year, month, day;
    Date::Convert(Date::FromDate(1909, 8, 28), year, month, day);
    EXPECT_EQ(1909, year);
    EXPECT_EQ(8, month);
    EXPECT_EQ(28, day);
}
