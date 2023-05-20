#include "Containers.hpp"
#include "GlobalUtils.hpp"
#include "DbtestDomainObjects.hh"
#include "DomainDBListeners.hh"
#include "Logger.hpp"
#include "gtest/gtest.h"

namespace 
{
	class DBMirrorTest : public ::testing::Test 
	{
		#define STOCKSIZE 100
		char sBuf[STOCKSIZE*sizeof(Stock)];
		char sLoggerBuf[8192];
		char sBufMirr[STOCKSIZE*sizeof(Stock)];

		protected: 
			DBMirrorTest() {
				//Parent DB 
				DomainDB::instance(1).getTable<Stock>(sBuf,STOCKSIZE*sizeof(Stock));
				DomainDB::instance(1).getLogger(sLoggerBuf,8192);
				//Mirror DB
				DomainDB::instance(2).getTable<Stock>(sBufMirr,STOCKSIZE*sizeof(Stock));
			}

			virtual ~DBMirrorTest() { }

			virtual void SetUp() 
			{
				DomainDB::instance(1).resetTable<Stock>();
				DomainDB::instance(2).resetTable<Stock>();
				DomainDB::instance(1).getLogger().reset();
			}

			virtual void TearDown() { 
				DomainDB::instance(1).resetTable<Stock>();
				DomainDB::instance(2).resetTable<Stock>();
				DomainDB::instance(1).getLogger().reset();
			}
	};
    
	
	TEST_F(DBMirrorTest, TestMirror)
	{
		char sSerialBuf[1500];
		system("rm test.db");
		DBFileSerializer dbwriter("test.db");
		dbwriter.setBuffer(sSerialBuf,1500);
		DomainDB::instance(1).addDBChangeListener(&dbwriter);

		DomainTable<Stock> &stockDB = DomainDB::instance(1).getTable<Stock>();
		if (stockDB.size() > 0 ) return;
		for (int i=0;i<10;i++)
		{
			Stock *stk = stockDB.createObject();
			if (stk != NULL) 
			{
				stk->setTicker("AAPL" + to_string(i));
				stk->setAdvBucket(i%2);
			}
		}
		DomainDB::instance(1).commit();
		//std::cout << DomainDB::instance(1).fieldsize(0) << std::endl;
	}

	TEST_F(DBMirrorTest, TestRebuild)
	{
		char sSerialBuf[1500];
		DBFileSerializer dbwriter("test.db");///Users/selvamdoraisamy/Desktop/personal/phoenix_v2/
		dbwriter.setBuffer(sSerialBuf,1500);
		dbwriter.unpackLog(DomainDB::instance(2));

        DomainTable<Stock> &stockDB = DomainDB::instance(2).getTable<Stock>();
		ASSERT_EQ(10,stockDB.size());
		for (int i=0;i<10;i++)
		{
			auto rec = DomainDB::instance(2).lookupByIndex<Stock>(i);
			if (rec == nullptr) continue;
			std::cout << rec->getTicker().c_str() << std::endl;
		}
	}

}  // namespace
