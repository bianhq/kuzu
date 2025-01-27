#include <string>

#include "include/main_test_helper.h"

using namespace kuzu::testing;

class CSVOutputTest : public ApiTest {};

TEST_F(CSVOutputTest, BasicCSVTest) {
    string newline = "\n";
    string basicOutput = R"(Carol,1,5.000000,1940-06-22,1911-08-20 02:32:21,CsWork)" + newline +
                         R"(Dan,2,4.800000,1950-07-23,2031-11-30 12:25:30,DEsWork)" + newline +
                         R"(Elizabeth,1,4.700000,1980-10-26,1976-12-23 11:21:42,DEsWork)" + newline;
    auto query = "MATCH (a:person)-[:workAt]->(o:organisation) RETURN a.fName, a.gender,"
                 "a.eyeSight, a.birthdate, a.registerTime, o.name";
    auto result = conn->query(query);
    result->writeToCSV("output_CSV_BASIC.csv");
    ifstream f("output_CSV_BASIC.csv");
    ostringstream ss;
    ss << f.rdbuf();
    string fileString = ss.str();
    ASSERT_STREQ(fileString.c_str(), basicOutput.c_str());
}

TEST_F(CSVOutputTest, ListCSVTest) {
    string newline = "\n";
    string listOutput =
        R"([""Aida""],"[10,5]","[""Alice"",""Alice""]")" + newline +
        R"([""Bobby""],"[12,8]","[""Bob"",""Bob""]")" + newline +
        R"("[""Carmen"",""Fred""]","[4,5]","[""Carol"",""Carol""]")" + newline +
        R"("[""Wolfeschlegelstein"",""Daniel""]","[1,9]","[""Dan"",""Dan""]")" + newline +
        R"([""Ein""],[2],"[""Elizabeth"",""Elizabeth""]")" + newline +
        R"([""Fesdwe""],"[3,4,5,6,7]","[""Farooq"",""Farooq""]")" + newline +
        R"([""Grad""],[1],"[""Greg"",""Greg""]")" + newline +
        R"("[""Ad"",""De"",""Hi"",""Kye"",""Orlan""]","[10,11,12,3,4,5,6,7]","[""Hubert Blaine Wolfeschlegelsteinhausenbergerdorff"",""Hubert Blaine Wolfeschlegelsteinhausenbergerdorff""]")" +
        newline;
    auto query = "MATCH (a:person) RETURN a.usedNames, a.workedHours, [a.fName, a.fName]";
    auto result = conn->query(query);
    result->writeToCSV("output_CSV_LIST.csv");
    ifstream f("output_CSV_LIST.csv");
    ostringstream ss;
    ss << f.rdbuf();
    string fileString = ss.str();
    ASSERT_STREQ(fileString.c_str(), listOutput.c_str());
}
