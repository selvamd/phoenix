#include <cassert>
#include <sstream>
#include <string>
#include <iostream>
#include <fstream>
#include <string>
#include "jsonxx.hpp"
#include "gtest/gtest.h"

#include "TcpUtils.hpp"
#include "ux_selector.hh"
#include "SocketStreamerBase.hh"
#include "ReliableMulticastChannel.hpp"
#include "ThreadPool.hpp"
#include "gtest/gtest.h"
#include <string>

namespace 
{
	class ThreadPoolTest : public ::testing::Test { };

	TEST_F(ThreadPoolTest, ThreadPoolTest1)
	{
        ThreadPool pool(4);
        auto result = pool.enqueue([](int answer) { return answer; }, 42);
        std::cout << result.get() << std::endl;
	}

}  // namespace


