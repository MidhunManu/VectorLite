#include "exec/eval.hpp"
#include <optional>
#include <string>

namespace vectordb
{
    namespace
    {
        std::optional<int> compare(const Value& a, const Value& b)
        {
            if (a.is_null() || b.is_null() || a.type() != b.type()) return std::nullopt;

            auto three = [](const auto& x, const auto& y) -> int { return x < y ? -1 : (y < x ? 1 : 0); };

            switch (a.type())
            {
                case ValueType::Int32:   return three(std::get<int32_t>(a.data), std::get<int32_t>(b.data));
                case ValueType::Int64:   return three(std::get<int64_t>(a.data), std::get<int64_t>(b.data));
                case ValueType::Float32: return three(std::get<float>(a.data), std::get<float>(b.data));
                case ValueType::String:  return three(std::get<std::string>(a.data), std::get<std::string>(b.data));
                case ValueType::Bool:    return three(static_cast<int>(std::get<bool>(a.data)),
                                                      static_cast<int>(std::get<bool>(b.data)));
                case ValueType::Null:    break;
            }
            return std::nullopt;
        }

        bool eval_comparison(const BoundComparison& cmp, const Record& record)
        {
            std::optional<int> c = compare(record.at(cmp.column_index), cmp.value);
            if (!c.has_value()) return false;

            switch (cmp.op)
            {
                case ComparisonOp::Eq: return *c == 0;
                case ComparisonOp::Ne: return *c != 0;
                case ComparisonOp::Lt: return *c < 0;
                case ComparisonOp::Gt: return *c > 0;
                case ComparisonOp::Le: return *c <= 0;
                case ComparisonOp::Ge: return *c >= 0;
            }
            return false;
        }
    }

    bool evaluate_predicate(const BoundPredicate& predicate, const Record& record)
    {
        bool result = false;
        bool current = eval_comparison(predicate.comparisons[0], record);

        for (size_t i = 0; i < predicate.connectors.size(); ++i)
        {
            bool next = eval_comparison(predicate.comparisons[i + 1], record);
            if (predicate.connectors[i] == LogicalOp::And)
            {
                current = current && next;
            }
            else
            {
                result = result || current;
                current = next;
            }
        }
        return result || current;
    }
}
