#include "Containers.hpp"
#include "GlobalUtils.hpp"
#include "DbtestDomainObjects.hh"
#include "Logger.hpp"
#include "gtest/gtest.h"

namespace 
{
	class DBTest : public ::testing::Test 
	{
		#define STOCKSIZE 100
		char sBuf[STOCKSIZE*sizeof(Stock)];
		char sLogBuf[8192];
		protected: 
			DBTest() { 
				DomainDB::instance(1).getTable<Stock>(sBuf,STOCKSIZE*sizeof(Stock));
			}

			virtual ~DBTest() { }

			virtual void SetUp() 
			{
				DomainTable<Stock> &stockDB = DomainDB::instance(1).getTable<Stock>();
				DomainDB::instance(1).getLogger(sLogBuf,8192);

				if (stockDB.size() > 0 ) return;
				for (int i=0;i<50;i++)
				{
					Stock *stk = stockDB.createObject();
					if (stk != NULL) 
					{
						stk->setTicker("AAPL" + to_string(i));
						stk->setAdvBucket(i%2);
					}
				}
			}

			virtual void TearDown() { 
				DomainDB::instance(1).resetTable<Stock>();
				DomainDB::instance(1).resetTable<Order>();
			}
	};
    
	void commit_log_test(uint64_t iExpected) 
	{
		LOG_DEBUG << iExpected << LOG_END;
		DomainDB::instance(1).commit();
		//RingBufferSPMC<LogRecord>& buffer = DomainDB::instance(1).getBuffer();
		//uint64_t iReq = 0, iRes = 0;
		//LogRecord rec;	
		//while (buffer.read(rec,iReq,iRes))
		//{
		//	iReq = iRes	+ 1;
		//}
		//ASSERT_EQ (iExpected, iReq);
	}
	
	TEST_F(DBTest, IterateBetweenStartAndEndByIndex)
	{
		DomainTable<Stock> &stockDB = DomainDB::instance(1).getTable<Stock>();
		ASSERT_EQ(50,stockDB.size());
		commit_log_test(100);

		Stock from, to; 
		from.setAdvBucket(1);
		to.setAdvBucket(1);
			
		int count = 0;	
		DomainTable<Stock>::IndexIterator itrS, itrE;
		if (  stockDB.begin(itrS,"AdvIndex",&from) && stockDB.end(itrE,"AdvIndex",&to))
		{	
			for ( ;itrS != itrE;itrS++)
			{
				Stock *stk = *itrS;	
				count += stk->getAdvBucket();	
			}	
		}
        
        //there are 10 objs between 20-30
        //and only 5 with adv = 1
		ASSERT_EQ (25, count);
		commit_log_test(100);
	}

	TEST_F(DBTest, SearchByPk)
	{
        Stock key;                
        key.setTicker("AAPL0");
        auto stock = DomainDB::instance(1).lookup<Stock>(key);
        ASSERT_EQ(stock->d_row, 0);
    }

	TEST_F(DBTest, IterateBetweenStartAndEndByPartialIndex)
	{
        DomainTable<Stock> &stockDB = DomainDB::instance(1).getTable<Stock>();
		ASSERT_EQ(50,stockDB.size());

        Stock from, to;
        //Select any object with AdvBucket=1 and ticker < "AAPL4"
		from.setAdvBucket(1);
        from.defTicker();
		to.setAdvBucket(1);
        to.setTicker("AAPL4");
			
		int count = 0;	
		DomainTable<Stock>::IndexIterator itrS, itrE;
		if (  stockDB.begin(itrS,"TickerAdvIndex",&from) && stockDB.end(itrE,"TickerAdvIndex",&to))
		{
            for ( ;itrS != itrE;itrS++)
			{
				Stock *stk = *itrS;
				count += stk->getAdvBucket();	
			}
		}
        ASSERT_EQ (8, count);
	}

	TEST_F(DBTest, ReverseIterateBetweenStartAndEndByPartialIndex)
	{
        DomainTable<Stock> &stockDB = DomainDB::instance(1).getTable<Stock>();
		ASSERT_EQ(50,stockDB.size());

        Stock from, to;
		from.setAdvBucket(1);
        from.defTicker();
		to.setAdvBucket(1);
        to.setTicker("AAPL4");
			
		int count = 0;	
		DomainTable<Stock>::IndexIterator itrS, itrE;
		if (  stockDB.begin(itrS,"TickerAdvIndex",&from) && stockDB.end(itrE,"TickerAdvIndex",&to))
		{
            while (itrS != itrE)
            {
                Stock *stk = *(--itrE);
				count += stk->getAdvBucket();	
            }
		}
        ASSERT_EQ (8, count);
	}

	TEST_F(DBTest, AccessByKeyAndUpdateIndexField) 
	{
		DomainTable<Stock> &stockDB = DomainDB::instance(1).getTable<Stock>();
		Stock key; 
		key.setTicker("AAPL20");
		
		Stock * obj = stockDB.findByPrimaryKey(&key);
		if (obj == NULL) cout << "Null returned" << endl;
		ASSERT_EQ (obj->get("Ticker"), "AAPL20");
		
		//verify it moves to last place on AdvIndex
		obj->set("AdvBucket","100");
		commit_log_test(101);
		
		int count = 0;	
		DomainTable<Stock>::IndexIterator itrS, itrE;
		if (  stockDB.begin(itrS,"AdvIndex") && stockDB.end(itrE,"AdvIndex"))
		{
			for ( ;itrS != itrE;itrS++)
			{
				if ((*itrS)->get("Ticker") == "AAPL20") break;
				else count++;	
			}
		}
		ASSERT_EQ (49, count);
		commit_log_test(101);
	}
	
	TEST_F(DBTest, TestDeleteAndAdd) 
	{
		DomainTable<Stock> &stockDB = DomainDB::instance(1).getTable<Stock>();
		for (int i=0;i<50;i++)
		{
			ASSERT_EQ (true,stockDB.removeObject(i));
		}	
		ASSERT_EQ(0,stockDB.size());
		commit_log_test(151);

		for (int i=0;i<10;i++)
		{
			Stock *stk = stockDB.createObject();
			if (stk != NULL) 
			{
				stk->setTicker("AAPL" + to_string(i));
				stk->setAdvBucket(i%2);
			}
		}
        
		ASSERT_EQ(10,stockDB.size());
		for (int i=40;i<50;i++)
		{
			Stock *stk = stockDB.createObject();
			if (stk != NULL) 
			{
				stk->setTicker("AAPL" + to_string(i));
				stk->setAdvBucket(i%2);
			}
		}
        
		ASSERT_EQ(20,stockDB.size());
		commit_log_test(191);

	}

	TEST_F(DBTest, TestEnumCreation)
    {
        Order ord;
        ord.setOrdType(OrdType_t::STOPLIMIT);
        std::cout << ord << std::endl;
        ord.setOrdType(OrdType_t::STOP);
        std::cout << ord << std::endl;
    }

}  // namespace
