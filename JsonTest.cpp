#include "Json.hpp"
#include "DataTypes.hpp"
#include "gtest/gtest.h"

namespace 
{
	class MyJsonTest : public ::testing::Test { };

	TEST_F(MyJsonTest, TestJsonBasic)
	{
		Json::JsonContainer jsons;

		auto root = Json::root(jsons);
		auto cld  = root.arr("user");
		auto cld1  = root.arr("family");
		root << cld1;
		root << cld;


		for (int i=0;i<150;i++)
		{
			auto obj = root.obj();
			obj << root.ele("wife","gowri chettyannan");
			obj << root.ele("son","aadhi selvam");
			obj << root.ele("daughter","sindhu selvam");
			cld1 << obj;
		}

		for (int i=0;i<490;i++)
		{
			std::string u = "user";
			u.append(std::to_string(i));
			cld << cld.ele(u);
		}
		
		std::string str = root.json();
		//std::cout << str << std::endl;
		//std::cout << str.length() << "," << jsons.size() << std::endl;
		//for (auto a:jsons)
		//	std::cout << *a << std::endl;
		std::cout << root.json() << std::endl;
		Timestamp t, t1;
		t.set();
		int count = 10;
		for (int i=0;i<count;i++)
		{
			auto mirror = Json::root(jsons,str);
			//std::cout << mirror.json() << std::endl;
		}
		t1.set();

		std::cout << "elapsed time " << ((t1-t)/count) << std::endl; 

		std::string s="{\"Event\":\"USER_LOGIN\",\"year\":\"2017\",\"producttype\":\"EQUITY\",\"symbol\":\"IBM\",\"data\":\"CLOSEPX\"}";
		auto test = Json::root(jsons,s);
	}


}  // namespace
