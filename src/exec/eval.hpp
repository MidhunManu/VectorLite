#pragma once
#include "query/planner/logical_plan.hpp"
#include "storage/record.hpp"

namespace vectordb
{
    bool evaluate_predicate(const BoundPredicate& predicate, const Record& record);
}
