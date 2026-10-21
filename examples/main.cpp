#include <iostream>
#include <string>

#include "query/query_error.hpp"
#include "vectordb/database.hpp"

static std::string fmt(const vectordb::Value& v)
{
    using vectordb::ValueType;
    switch (v.type())
    {
        case ValueType::Null:    return "NULL";
        case ValueType::Int32:   return std::to_string(std::get<int32_t>(v.data));
        case ValueType::Int64:   return std::to_string(std::get<int64_t>(v.data));
        case ValueType::Float32: return std::to_string(std::get<float>(v.data));
        case ValueType::String:  return std::get<std::string>(v.data);
        case ValueType::Bool:    return std::get<bool>(v.data) ? "true" : "false";
    }
    return "?";
}

int main(int argc, char const* argv[])
{
    if (argc < 2)
    {
        std::cerr << "usage: " << argv[0] << " <database-file>\n";
        return 1;
    }

    try
    {
        vectordb::Database db = vectordb::Database::open(argv[1]);

        std::string line;
        std::cout << "vdb> " << std::flush;
        while (std::getline(std::cin, line))
        {
            while (!line.empty() && (std::isspace(static_cast<unsigned char>(line.back())) || line.back() == ';'))
                line.pop_back(); // the lexer has no ';' token
            if (line == ".quit") break;

            if (!line.empty())
            {
                try
                {
                    vectordb::ResultSet r = db.execute(line);
                    if (!r.columns.empty())
                    {
                        for (size_t i = 0; i < r.columns.size(); ++i)
                            std::cout << (i ? " | " : "") << r.columns[i];
                        std::cout << "\n";
                        for (const auto& row : r.rows)
                        {
                            for (size_t i = 0; i < row.size(); ++i)
                                std::cout << (i ? " | " : "") << fmt(row[i]);
                            std::cout << "\n";
                        }
                        std::cout << "(" << r.rows.size() << " rows)\n";
                    }
                    else
                    {
                        std::cout << r.affected_rows << " row(s) affected\n";
                    }
                }
                catch (const vectordb::QueryError& e)
                {
                    std::cout << "query error at offset " << e.offset() << ": " << e.what() << "\n";
                }
                catch (const std::exception& e)
                {
                    std::cout << "error: " << e.what() << "\n";
                }
            }
            std::cout << "vdb> " << std::flush;
        }

        db.close();
    }
    catch (const std::exception& e)
    {
        std::cerr << "fatal: " << e.what() << "\n";
        return 1;
    }
    return 0;
}