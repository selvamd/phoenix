#include "GlobalUtils.hpp"
#include "jsonxx.hpp"
#include "DbtestDomainObjects.hh"
#include "Logger.hpp"
#include "gtest/gtest.h"

int main(int argc, char **argv)
{
    //setLogLevel
    std::string level("debug");
    if (argc == 2) level = argv[1];
    setLogLevel(level);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
