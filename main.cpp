#include <iostream>

using namespace std;

int main()
{
    ConcurrentHashTable<std::string, std::string> hash_table;

    for (size_t i = 0; i < 1000000; ++i)
    {
        std::ostringstream key, val;
        key << "key " << i;
        val << "val " << i;

        std::string key_str = key.str();
        std::string val_str = val.str();

        hash_table[key_str];
    }

    return 0;
}
