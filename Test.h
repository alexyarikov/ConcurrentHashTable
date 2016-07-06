#pragma once
#include "stdafx.h"

#define CONTAINER_SIZE 50000

// Concurrent hash table test class
class Test
{
public:
    static void start();

private:
    static void test_insert();
    static void test_update();
    static void test_erase();
    static void test_clear();
    static void test_rehash();
    static void test_multithreaded();

    static void thread_func(ConcurrentHashTable<uint16_t, std::string>& ht, std::atomic_bool& work_flag, std::mutex& print_mutex);
    static void print_msg(const std::string& msg, std::mutex& print_mutex);
};

void Test::start()
{
    test_insert();
    test_update();
    test_erase();
    test_clear();
    test_rehash();
    test_multithreaded();
}

void Test::test_insert()
{
    std::cout << "insert test:\t\t";

    ConcurrentHashTable<uint16_t, std::string> ht;
    ht.insert(0, "val0");
    bool res = (ht[0] == "val0");
    res = res && (ht.size() == 1);

    std::cout << (res ? "passed" : "failed") << std::endl;
}

void Test::test_update()
{
    std::cout << "update test:\t\t";

    ConcurrentHashTable<uint16_t, std::string> ht;
    ht.insert(1000, "val1000");
    size_t size = ht.size();
    ht[1000] = "val1000_upd";
    bool res = (ht[1000] == "val1000_upd");
    res = res && (size == ht.size());

    std::cout << (res ? "passed" : "failed") << std::endl;
}

void Test::test_erase()
{
    std::cout << "erase test:\t\t";

    ConcurrentHashTable<uint16_t, std::string> ht;
    ht.insert(1000, "val1000");
    ht.insert(1001, "val1001");
    ht.erase(1001);
    ht.erase(1000);
    bool res = (!ht.contains(1000));
    res = res && (!ht.contains(1001));

    std::cout << (res ? "passed" : "failed") << std::endl;
}

void Test::test_clear()
{
    std::cout << "clear test:\t\t";

    ConcurrentHashTable<uint16_t, std::string> ht;
    ht.insert(0, "val0");
    ht.clear();
    bool res  = (ht.size() == 0);

    std::cout << (res ? "passed" : "failed") << std::endl;
}

void Test::test_rehash()
{
    std::cout << "rehash test:\t\t";

    ConcurrentHashTable<uint16_t, std::string> ht(7, 0.5, 2.0);
    bool res = (ht.capacity() == 7);
    ht.insert(0, "0");
    ht.insert(1, "1");
    ht.insert(2, "2");
    ht.insert(3, "3");
    ht.insert(4, "4");
    res = res && (ht.capacity() == 14);
    res = res && (ht.size() == 5);
    res = res && (ht[0] == "0");
    res = res && (ht[1] == "1");
    res = res && (ht[2] == "2");
    res = res && (ht[3] == "3");
    res = res && (ht[4] == "4");

    std::cout << (res ? "passed" : "failed") << std::endl;
}

void Test::test_multithreaded()
{
    ConcurrentHashTable<uint16_t, std::string> ht;

    std::cout << "multi thread test:" << std::endl;

    // fill container
    for (uint16_t i = 0; i < CONTAINER_SIZE; ++i)
    {
        std::stringstream val_str;
        val_str << "val " << i;
        ht.insert(i, val_str.str());
    }

    std::cout << "starting threads, press any key to stop..." << std::endl;

    // create and start threads
    std::atomic_bool work_flag = true;
    std::mutex print_mutex;

    std::vector<std::thread> threads;

    for (unsigned int i = 0; i < std::thread::hardware_concurrency(); ++i)
        threads.emplace_back(thread_func, std::ref(ht), std::ref(work_flag), std::ref(print_mutex));

    // waiting for any key to press
    _getch();

    // stop threads
    work_flag = false;
    for (auto& thread : threads)
        thread.join();
}

void Test::thread_func(ConcurrentHashTable<uint16_t, std::string>& ht, std::atomic_bool& work_flag, std::mutex& print_mutex)
{
    std::srand(unsigned int(std::time(0)));

    print_msg("starting thread", print_mutex);

    while (work_flag)
    {
        uint16_t key = std::rand() % (CONTAINER_SIZE * 2);

        switch (std::rand() % 2)
        {
            case 0:
            {
                if (ht.contains(key))
                {
                    std::string val = ht[key];
                    ht[key] = val + "_upd";
                }
                else
                {
                    std::stringstream val;
                    val << key;
                    ht.insert(key, val.str());
                }
                break;
            }
            case 1:
            {
                ht.erase(key);
                break;
            }
        }

        //std::this_thread::yield();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    print_msg("thread stopped", print_mutex);
}

void Test::print_msg(const std::string& msg, std::mutex& print_mutex)
{
    std::lock_guard<std::mutex> lock(print_mutex);
    std::cout << std::this_thread::get_id() << ": " << msg << std::endl;
}
