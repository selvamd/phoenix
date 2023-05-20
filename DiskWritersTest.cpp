#include "DiskWriters.hpp"
#include "gtest/gtest.h"

namespace 
{
	class DiskWritersTest : public ::testing::Test { };

	TEST_F(DiskWritersTest, TestBlockWriter)
	{
		BlockWriter writer;
		writer.init("test",5);
		
		for (int i=0;i<5;i++)
			writer.set(i*10.0,i);
		auto l = writer.writeNew();
		std::cout << l << std::endl;

		for (int i=0;i<5;i++)
			writer.set(i*20.0,i);
		l = writer.writeNew();
		std::cout << l << std::endl;

		for (int i=0;i<5;i++)
			writer.set(i*30.0,i);
		l = writer.writeNew();
		std::cout << l << std::endl;

		auto d = writer.read(l-40);
		for (int i=0;i<5;i++)
			std::cout << d[i] << std::endl;
	}

	TEST_F(DiskWritersTest, TestBlockWriter2)
	{
		BlockWriter writer;
		writer.init("test2",5);
		writer.writeNew();
		writer.read(0);
		writer.set(writer.get(0)+10,0);
		writer.set(writer.get(1)+10,1);
		writer.set(writer.get(2)+10,2);
		writer.set(writer.get(3)+10,3);
		writer.set(writer.get(4)+10,4);
		for (int i=0;i<5;i++)
			std::cout << writer.get(i) << std::endl;
		writer.write(0);
	}

	TEST_F(DiskWritersTest, TestSharedMemoryMap)
	{
		SharedMemoryMap map("test_mmap", 100);
		auto c = (char *)map.init();
		memset(c,'1',100);
		for (int i=0;i<100;i++)
		{
			if (c[i]!='1')
				std::cout << c[i] << std::endl;
		}
	}
}  // namespace
