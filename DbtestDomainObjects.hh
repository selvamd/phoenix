#pragma once

#include <cstring>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <assert.h>

using namespace std;

#include "DataTypes.hpp"
#include "DomainDatabase.hh"

//No spacing between macro var-args for enum values
DEFINE_ENUM_TYPES(DomainObjects_t,STOCK,ORDER)
DEFINE_ENUM_TYPES(OrdType_t,MARKET,LIMIT,STOP,STOPLIMIT)

class Stock : public DomainObjectBase<Stock>
{
    public:
    
        friend DomainTable<Stock>;
        static int TableID() { return (int)DomainObjects_t::STOCK; }
        static string TableName() { return "Stock"; }
        Stock(int i = -1, void * sBuff = NULL) : d_row(i), d_dbid(0) { clone(sBuff); }
    
        //Use DECLARE_INDEX if the data member is used to construct an index 
        DECLARE_INDEX(FixedString<12>, Ticker,0)
        DECLARE_INDEX(int, AdvBucket,1)
        DECLARE_MEMBER(int,HasDarkQuotes,2)
        DECLARE_MEMBER(int64_t,  test64 , 3)
    
        static int maxFields()    { return 4; }
        #define SET(FieldIndex) DECLARE_END(FieldIndex)
        SET4()
        #undef SET
                    
        const int d_row;
        const uint32_t d_dbid;
    
    private:

        //Called during table construction to create indexes 
        static void createIndices(DomainTable<Stock> &table)
        {
            table.addIndex("PrimaryKey",stk_primarykey);
            table.addIndex("AdvIndex",stk_adv_compare);
            table.addIndex("TickerAdvIndex",stk_ticker_adv_compare);
        };

        //Compare function used for an index
        static bool stk_primarykey(const Stock * a,  const Stock * b)
        {
            int i = a->getTicker().compareForIndex(b->getTicker());
            return (i == 0 )? a->d_row < b->d_row : i < 0;
        }
        
        //Compare function used for an index
        static bool stk_adv_compare(const Stock * a,  const Stock * b)
        {
                if ( a->getAdvBucket() != b->getAdvBucket() )
                    if ( min(a->getAdvBucket(),b->getAdvBucket()) >= 0 ) 
                        return ( a->getAdvBucket() < b->getAdvBucket() );
                return ( a->d_row < b->d_row);
        }

        //Compare function used for an index
        static bool stk_ticker_adv_compare(const Stock * a,  const Stock * b)
        {
            if ( a->getAdvBucket() != b->getAdvBucket() )
                if ( min(a->getAdvBucket(),b->getAdvBucket()) >= 0 )
                    return ( a->getAdvBucket() < b->getAdvBucket() );

            int i = a->getTicker().compareForIndex(b->getTicker());
            if ( i != 0 ) return (i > 0);
            
            return a->d_row < b->d_row;
        }
};

class Order : public DomainObjectBase<Order>
{
    public:
        friend DomainTable<Order>;
        static int TableID() { return (int)DomainObjects_t::ORDER; }
        static string TableName() { return "Order"; }
        Order(int i = -1, void * sBuff = NULL) : d_row(i), d_dbid(0) { clone(sBuff); }
    
        //Use DECLARE_INDEX if the data member is used to construct an index 
        DECLARE_INDEX(int,   SessionID,     0)
        DECLARE_INDEX(int64_t,  ClOrdID ,      1)
        DECLARE_INDEX(int,   SymbolID ,     2)
        DECLARE_INDEX(int,   Side,          3)
        DECLARE_INDEX(int,   Price ,        4)
        DECLARE_INDEX(Timestamp,  RankTime , 5)

        DECLARE_MEMBER(int,  Quantity ,     6)
        DECLARE_MEMBER(char, TimeInForce ,  7)
        DECLARE_MEMBER(EnumData<OrdType_t>, OrdType , 8)
        DECLARE_MEMBER(char, ExecInst ,     9)
        DECLARE_MEMBER(char, MaxFloor ,     10)
        DECLARE_MEMBER(int,  FirmID ,       11)
        DECLARE_MEMBER(int,  UserID ,       12)
        DECLARE_MEMBER(int64_t,  test64 , 13)
    
        //Define end of record
        static int maxFields()    { return 14; }
        #define SET(FieldIndex) DECLARE_END(FieldIndex)
        SET14()
        #undef SET

        const int d_row;
        const uint32_t d_dbid;
    
    private:

        //Called during table construction to create indexes 
        static void createIndices(DomainTable<Order> &table)
        {
            table.addIndex("PrimaryKey",ord_primarykey);
            table.addIndex("BookIndex",ord_book_index);
        };

        //Compare function used for an index
        static bool ord_primarykey(const Order * a,  const Order * b)
        {
            if ( a->getSessionID() != b->getSessionID() )
                    if ( min(a->getSessionID(),b->getSessionID()) >= 0 ) 
                        return a->getSessionID() < b->getSessionID();
            
            if ( a->getClOrdID() != b->getClOrdID() )
                    if ( min(a->getClOrdID(),b->getClOrdID()) >= 0 ) 
                        return a->getClOrdID() < b->getClOrdID();
            
            return a->d_row < b->d_row;
        }
    
        //Compare function used for an index
        static bool ord_book_index(const Order * a,  const Order * b)
        {
            if (a->getSymbolID() != b->getSymbolID())
                if ( min(a->getSymbolID(),b->getSymbolID()) >= 0 ) 
                     return a->getSymbolID() < b->getSymbolID();
            
            if (a->getSide() != b->getSide())
                if ( min(a->getSide(),b->getSide()) >= 0 ) 
                    return a->getSide() < b->getSide();
            
            if (a->getPrice() != b->getPrice())
            {
                if ( min(a->getPrice(),b->getPrice()) >= 0 ) 
                {
                    if ( a->getSide() == '1' )
                        return a->getPrice() < b->getPrice();
                    else
                        return a->getPrice() > b->getPrice();
                }
            }

            int i = a->getRankTime().compareForIndex(b->getRankTime());
            if ( i != 0 ) return (i > 0);

            return a->d_row < b->d_row;
        }
};

