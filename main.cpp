#include "stdafx.h"
#include "ConcurrentHashTable.h"

int main()
{
    // test my hash table
    typedef ConcurrentHashTable<size_t, std::string> MyHashTableType;
    MyHashTableType my_hash_table(31, 1);
    Benchmark<MyHashTableType>::start(my_hash_table, "My hash table");

    // test STL hash table
    typedef std::unordered_map<size_t, std::string> STLHashTableType;
    STLHashTableType stl_hash_table;
    Benchmark<STLHashTableType>::start(stl_hash_table, "STL hash table");

    printf("Press any key to exit\r\n");
    _getch();
    return 0;
}
