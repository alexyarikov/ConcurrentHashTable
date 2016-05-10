#pragma once
#include "GetCPUTime.h"

// Concurrent hash table benchmark test class
template <typename TContainer>
class Benchmark
{
public:
    static void start(TContainer& container, const std::string& container_name)
    {
        container.clear();

        double time_start = getCPUTime();

        for (size_t i = 0; i < 50000; ++i)
        {
            std::stringstream val_str, val_str_upd;
            val_str << "val " << i;
            val_str_upd << "val_upd " << i;

            // check size method
            assert(container.size() == i);

            // check at method
            bool caught = false;
            try
            {
                std::string val = container.at(i);
            }
            catch (const std::out_of_range&)
            {
                caught = true;
            }
            assert(caught);

            // check insert
            container.insert(std::make_pair(i, val_str.str()));
            assert(container.size() == i + 1);

            // check update with operator []
            container[i] = val_str_upd.str();
            assert(container.size() == i + 1);

            // check at
            std::string val;
            try
            {
                val = container[i];
            }
            catch (const std::out_of_range&)
            {
                assert(false);
            }
            assert(val.compare(val_str_upd.str()) == 0);

            // check erase
            container[i + 1] = "dummy";
            container.erase(i + 1);
            assert(container.size() == i + 1);
        }

        double time_end = getCPUTime();
        printf("%s CPU time used:\t%lf\r\n", container_name.c_str(), time_end - time_start);
    }
};
