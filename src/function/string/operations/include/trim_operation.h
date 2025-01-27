#pragma once

#include <cassert>
#include <cstring>

#include "ltrim_operation.h"
#include "rtrim_operation.h"

#include "src/common/types/include/ku_string.h"

using namespace std;
using namespace kuzu::common;

namespace kuzu {
namespace function {
namespace operation {

struct Trim : BaseStrOperation {
public:
    static inline void operation(
        ku_string_t& input, ku_string_t& result, ValueVector& resultValueVector) {
        BaseStrOperation::operation(input, result, resultValueVector, trim);
    }

private:
    static uint32_t trim(char* data, uint32_t len) {
        return Rtrim::rtrim(data, Ltrim::ltrim(data, len));
    }
};

} // namespace operation
} // namespace function
} // namespace kuzu
