#include <cstdio>

#include "../include/vectordb/database.hpp"

int main(int argc, char const *argv[])
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: %s <database-file>\n", argv[0]);
        return 1;
    }

    try
    {
        vectordb::Database db = vectordb::Database::open(argv[1]);
        db.close();
        std::printf("opened database: %s\n", argv[1]);
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }

    return 0;
}
