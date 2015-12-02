#include "stdafx.h"
#include "ConcurrentHashTable.h"

int main(int argc, char* argv[])
{
	ConcurrentHashTable<std::string, std::string> hash_table;

	std::string key1("key1"), val1("val1");
	hash_table.insert(&key1, &val1);

	return 0;
}
