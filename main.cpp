#include "stdafx.h"
#include "ConcurrentHashTable.h"

int main()
{
    ConcurrentHashTable<size_t, std::string> hash_table;

    for (size_t i = 0; i < 1000000; ++i)
    {
        std::stringstream val;
        val << "val " << i;
        std::string val_str = val.str();

        size_t size = hash_table.size();
        hash_table[i] = val_str;
    }

    return 0;
}
