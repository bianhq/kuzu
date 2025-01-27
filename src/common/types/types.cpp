#include "include/types.h"

#include <stdexcept>

#include "include/types_include.h"
#include "include/value.h"

#include "src/common/include/exception.h"

namespace kuzu {
namespace common {

DataType::DataType(const DataType& other) : typeID{other.typeID} {
    if (other.childType) {
        childType = other.childType->copy();
    }
}

DataType& DataType::operator=(const DataType& other) {
    typeID = other.typeID;
    if (other.childType) {
        childType = other.childType->copy();
    }
    return *this;
}

bool DataType::operator==(const DataType& other) const {
    if (typeID != other.typeID) {
        return false;
    }
    if (typeID == LIST && *childType != *other.childType) {
        return false;
    }
    return true;
}

unique_ptr<DataType> DataType::copy() {
    if (childType) {
        return make_unique<DataType>(typeID, childType->copy());
    } else {
        return make_unique<DataType>(typeID);
    }
}

DataType Types::dataTypeFromString(const string& dataTypeString) {
    DataType dataType;
    if (dataTypeString.ends_with("[]")) {
        dataType.typeID = LIST;
        dataType.childType = make_unique<DataType>(
            dataTypeFromString(dataTypeString.substr(0, dataTypeString.size() - 2)));
        return dataType;
    } else {
        dataType.typeID = dataTypeIDFromString(dataTypeString);
    }
    return dataType;
}

DataTypeID Types::dataTypeIDFromString(const std::string& dataTypeIDString) {
    if ("NODE_ID" == dataTypeIDString) {
        return NODE_ID;
    } else if ("INT64" == dataTypeIDString) {
        return INT64;
    } else if ("DOUBLE" == dataTypeIDString) {
        return DOUBLE;
    } else if ("BOOLEAN" == dataTypeIDString) {
        return BOOL;
    } else if ("STRING" == dataTypeIDString) {
        return STRING;
    } else if ("DATE" == dataTypeIDString) {
        return DATE;
    } else if ("TIMESTAMP" == dataTypeIDString) {
        return TIMESTAMP;
    } else if ("INTERVAL" == dataTypeIDString) {
        return INTERVAL;
    } else {
        throw Exception("Cannot parse dataTypeID: " + dataTypeIDString);
    }
}

string Types::dataTypeToString(const DataType& dataType) {
    if (dataType.typeID == LIST) {
        assert(dataType.childType);
        auto result = dataTypeToString(*dataType.childType) + "[]";
        return result;
    } else {
        return dataTypeToString(dataType.typeID);
    }
}

string Types::dataTypeToString(DataTypeID dataTypeID) {
    switch (dataTypeID) {
    case ANY:
        return "ANY";
    case NODE:
        return "NODE";
    case REL:
        return "REL";
    case NODE_ID:
        return "NODE_ID";
    case BOOL:
        return "BOOL";
    case INT64:
        return "INT64";
    case DOUBLE:
        return "DOUBLE";
    case DATE:
        return "DATE";
    case TIMESTAMP:
        return "TIMESTAMP";
    case INTERVAL:
        return "INTERVAL";
    case STRING:
        return "STRING";
    case UNSTRUCTURED:
        return "UNSTRUCTURED";
    case LIST:
        return "LIST";
    default:
        assert(false);
    }
}

string Types::dataTypesToString(const vector<DataType>& dataTypes) {
    vector<DataTypeID> dataTypeIDs;
    for (auto& dataType : dataTypes) {
        dataTypeIDs.push_back(dataType.typeID);
    }
    return dataTypesToString(dataTypeIDs);
}

string Types::dataTypesToString(const vector<DataTypeID>& dataTypeIDs) {
    if (dataTypeIDs.empty()) {
        return string("");
    }
    string result = "(" + Types::dataTypeToString(dataTypeIDs[0]);
    for (auto i = 1u; i < dataTypeIDs.size(); ++i) {
        result += "," + Types::dataTypeToString(dataTypeIDs[i]);
    }
    result += ")";
    return result;
}

const uint32_t Types::getDataTypeSize(DataTypeID dataTypeID) {
    switch (dataTypeID) {
    case NODE_ID:
        return sizeof(nodeID_t);
    case BOOL:
        return sizeof(uint8_t);
    case INT64:
        return sizeof(int64_t);
    case DOUBLE:
        return sizeof(double_t);
    case DATE:
        return sizeof(date_t);
    case TIMESTAMP:
        return sizeof(timestamp_t);
    case INTERVAL:
        return sizeof(interval_t);
    case STRING:
        return sizeof(ku_string_t);
    case UNSTRUCTURED:
        return sizeof(Value);
    case LIST:
        return sizeof(ku_list_t);
    default:
        throw Exception(
            "Cannot infer the size of dataTypeID: " + dataTypeToString(dataTypeID) + ".");
    }
}

RelDirection operator!(RelDirection& direction) {
    return (FWD == direction) ? BWD : FWD;
}

string getRelDirectionAsString(RelDirection direction) {
    return (FWD == direction) ? "forward" : "backward";
}

} // namespace common
} // namespace kuzu
