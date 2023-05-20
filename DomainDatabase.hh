#pragma once

#include <cstring>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <sstream>
#include <assert.h>
#include <iostream>
#include <fstream>
using namespace std;
#include "macros.hh"
#include "Containers.hpp"
#include "GlobalUtils.hpp"
#include <atomic>
#include <climits>

//Notes:
// 1. Class model of this framework mimics the organisation of a Database. 
//	  -	Database contains one or more tables 
//    - Tables contains several data rows accessed like array using fastindex
//	  - Relationships can be defined between tables using fastindex fields.
//	  - Tables can define indices for ordered access.
//    
// 2. Table data records support introspection. (ie) get(string fldname) and set(string fldname, string value)
//
// 3. Tables are initialized with fixed size contiguous memory buffer (shared or local) to manage object allocations. 
//	  - Records are allocated and managed on that buffer. Table.addObject allocates new object on the buffer 
//      and table.removeObject deletes object and releases the memory to free pool for reuse.
//
// 4. Entity tables: Special tables referencing primary entity in an application. The entity keys are used to 
//    define relationships and index data in other tables and therefore deserves special handling.  
//    - RemoveObject doesnot return the memory to free pool. Non-reuse policy preserves referential integrity in relation Tables.  
//    - First field of Entity table MUST be designated for permanent key (can be any datatype - string or numeric)
//    - Field and table Naming conventions are used to implicitly indicate entity-relationships.
//			- Entity Table names : Use entity name with suffix 'Lookup'. (eg) UserLookup, SymbolLookup etc
//    		- FK fields : Uses qualifier + entity name + 'Index'. (eg) SenderUserIndex fkeys into UserLookup
//          - FK fields in relation tables are nullable (Value -1 indicates null reference)  
//    - load and store functions to disk require a deterministic order for preserving referential integrity during the process. 
//		Consequently using circular references between entities is not allowed. Prefered order is as follows:
//			(a) Independent Entity (b) Dependent Entity (c) Relationship tables. 
//
// 5. Index provides a unique ordering to iterate over the data. Data objects in the tables can be iterated 
//    multiple ways by pre-defining multiple indices based on compare functions.
//    - No external logic required to manage the index. When setter functions are called on data objects, 
//      re-indexing is done transparently.
//	  - Ensures indices are automatically kept in sync during add/modify/remove record operations. 
//    - Within an index, partial iteration can be acheieved by using start and end key as boundary objects for find calls
//    - Index called "PrimaryKey" is mandatory for all the tables and will facilitate lookup by unique primary key.
//    - Calling setter on an index field while continuing to iterate is unsafe. Reindexing can result in gaps/duplication 
//      of traversal due to tree re-balancing.
//
// 6. Change logs (also created during set operations transparently) are tagged with transactionid and copied to 
//    log collector (DomainLogger) for external consumption.
// 	  - Consumers can use this to rebuild a database mirror (replication)
//    - Consumers can also run non-realtime tasks NotificationService, QueryService, DiskRecorderService, UIViewerService etc

//Forward declaration of all the classes
class DomainTableBase;
class DomainLogger;
class DomainDB;

template <typename Element, size_t Size>  class SPSCQueue;
template <typename DO> class DomainTable;
template <int maxsize> class FixedString;
template <typename DO, size_t maxsize> class FixedArray;
template <typename DO> class DomainObjectBase;

#define MAX_FIELD_SIZE 255
#define MAX_TABLE_SIZE 255
#define MAX_COLS_SIZE 64

DEFINE_ENUM_TYPES(Boolean_t,TRUE,FALSE)

/*
 * Iteration using partial keys can be achieved by setting
 * the key object with partial key field values, and setting defaults 
 * non-specified keys. The compare functions will have to treat any comparison 
 * with default values as wildcard match. 
 * Please refer to DBTest_ut for partial iterator and partial reverse
 * iterator examples.
 */
class InitFunctions
{
    public:
        /*
         * Functions to set default values for Table key values
         */
        void initDefault(Timestamp& val)
        {
            val = 0;
        }
        
        template<int i>
        void initDefault(FixedString<i>& val)
        {
            char str[2];
            str[0] = (char)0xFF;
            str[1] = 0;
            val = str;
        }

        template <typename DO, size_t size>
        void initDefault(FixedArray<DO,size>& val)
        {
            val.init();
        }
    
        template<typename enumVal>
        void initDefault(EnumData<enumVal>& val)
        {
            val.init();
        }
        
        void initDefault(double& val)       { val = -1.0; };
        //void initDefault(long& val)      	{ val = -1; };
        void initDefault(int64_t& val)      	{ val = -1; };
        void initDefault(int32_t& val)      	{ val = -1; };
        void initDefault(int16_t& val)      	{ val = -1; };
        void initDefault(char& val)      	    { val = -1; };
};

//Logging Class that holds all replaced values so it can be used for rollback
//If underlying buffer is full, logging will stop (with rc = false)
class DomainLogger
{
	friend DomainDB;

	private:
		uint8_t  d_chkpt;
		size_t   d_size;
		char    *d_buffer;
		size_t  d_maxsize;

		//checkpt,instance,table,field[255=>DELETE],row;
		std::vector<std::tuple<uint8_t,uint8_t,uint8_t,uint8_t,uint32_t>> d_fields;
		std::vector<std::tuple<uint8_t,uint8_t,uint8_t,uint8_t,uint32_t>> d_newrows;

		size_t freesize() { return d_maxsize - d_size; }
	
	public:
		DomainLogger() { 
			d_buffer = nullptr;
			std::cout << "New logger created" << std::endl; 
		}
		
		void setBuffer(void * sBuf, int size)
		{
			//set only once
			if (d_buffer != nullptr) return;
			d_buffer  = (char *) sBuf;
			d_maxsize = size;
			reset();
		}

		void reset()
		{
			memset(d_buffer,0,d_maxsize);
			d_fields.clear();
			d_newrows.clear();
			d_size = 0;
			d_chkpt = 0;
		}

		uint8_t addcheckpoint() { return ++d_chkpt; }
		uint8_t checkpoint() { return d_chkpt; }

		template <typename DO>
		bool insert(DO *rec)
		{
			if (d_buffer == nullptr) return false;
			d_newrows.push_back(std::make_tuple(d_chkpt, (uint8_t)rec->d_dbid, 
				(uint8_t)DO::TableID(), 255,(uint32_t)rec->d_row));
			return true;
		}

		template <typename DO>
		bool log(DO *rec, int32_t fieldIndex = -1)
		{
			if (d_buffer == nullptr) return false;
			if (fieldIndex > 255 || fieldIndex < -1)
				return false;
			if (fieldIndex == -1) fieldIndex = 255;
			d_fields.push_back(std::make_tuple(d_chkpt, (uint8_t)rec->d_dbid, 
				(uint8_t)DO::TableID(), (uint8_t)fieldIndex, (uint32_t)rec->d_row));
			if (fieldIndex == 255 && sizeof(DO) < freesize())
				d_size += rec->buffer(&d_buffer[d_size]);
			else if (freesize() > MAX_FIELD_SIZE)
				d_size += rec->copy(fieldIndex, &d_buffer[d_size], false);  			
			else return false;
			return true;
		}
};

//Abstract base class (interface) to declare non-template generic functions 
class DomainTableBase
{
	public:
		//returns the size of data copied, field = -1 implies full record 
		virtual size_t fieldsize(int field) = 0;
		virtual bool removeObject(int id) = 0;
		virtual int bufcopy(char * sBuf, bool bInOut,int row, int field) = 0;
		virtual bool obj2str(const int &index, std::string &data) = 0;
		virtual bool str2obj(const std::string &data) = 0;
		virtual int size(const std::string &indexName = "") const = 0;
		virtual ~DomainTableBase() {};
};

template <typename DO>
class DomainTable :  DomainTableBase
{
	friend DomainObjectBase<DO>;
	friend DO;
	friend DomainDB;

	public:
		typedef bool(*compare_func)(const DO * lhs, const DO * rhs);
		typedef typename std::set<DO *, compare_func> 	Index;
		typedef typename Index::iterator 				IndexIterator;
		typedef typename std::map<string,Index *> 		IndexContainer;
		typedef typename IndexContainer::iterator 		IndexContainerIterator;
		
	private:
		const uint16_t              d_instanceid;
		DO *  						d_buffer;
		deque<int>                  d_buffer_free;
		int 						d_buffer_free_size;
		int 						d_buffer_loop;
		std::map<int,uint64_t> 		d_log_map; //lastbit marks obj deletion
		int 						d_buffer_max;
		std::map<string,Index *> 	d_indices;
		int 						d_size;


		DomainTable<DO>(uint16_t instanceid) :  d_instanceid (instanceid)
		{
			d_buffer = NULL;
			d_buffer_max = 0;
			d_buffer_free_size = 0;
            d_buffer_loop = 0;
            d_size = 0;
			
			DO::createIndices(*this);
			if ( getIndex("PrimaryKey") == NULL )
				cout << "ERROR: No Primary key defined for the table " << endl;
		}

		~DomainTable<DO>()
		{
			for ( IndexContainerIterator iter = d_indices.begin(); iter != d_indices.end(); ++iter)
			{
				delete iter->second;
				//d_indices.erase(iter);
			}
		}

		//can be done only once
		void setBuffer(void * sBuf, int size)
		{
			if (d_buffer != NULL) return;
			d_buffer = (DO *) sBuf;
			d_buffer_max = size / sizeof(DO);
			memset(d_buffer, 0 , size);
            for (int i=0; i < d_buffer_max; i++)
                d_buffer_free.push_back(i);
            d_buffer_free_size = d_buffer_max;    
            d_buffer_loop = 0;
            d_size = 0;
			
		}
		
		/*
		void notifyObjectModified(DO *obj, int fieldIndex) 
		{
			int id = getObjectID(obj);
			if ( id == -1) return;
			std::map<int,uint64_t>::iterator it = d_log_map.find(id);
			uint64_t modBits = (it == d_log_map.end())? 0: it->second;
			modBits |= ((uint64_t)1 << fieldIndex);
			if (it != d_log_map.end()) d_log_map.erase(it);	
			d_log_map.insert(make_pair(id,modBits));
		};*/
		
		void addIndex(string index, bool(*c)(const DO * lhs, const DO * rhs)) 
		{
			d_indices.insert(make_pair(index,new Index(c)));		
		};

		void deleteObjFromIndices(DO * obj) 
		{
			if (getObjectID(obj) == -1) return;
			for ( IndexContainerIterator iter = d_indices.begin(); iter != d_indices.end(); ++iter)
				iter->second->erase(obj);
		};

		void addObjToIndices(DO * obj) 
		{
			if (getObjectID(obj) == -1) return;
			for ( IndexContainerIterator iter = d_indices.begin(); iter != d_indices.end(); ++iter)
				iter->second->insert(obj);
		};
		
		Index * getIndex(const string& name) 
		{
			IndexContainerIterator iter = d_indices.find(name);
			return (iter == d_indices.end())? NULL: iter->second;
		};

	public:

		int capacity() const {
			return d_buffer_max;
		}

		int size(const std::string &indexName = "") const {
            if (indexName == "")
                return d_size;

            auto iter = d_indices.find(indexName);
            if (iter == d_indices.end()) return 0;
            return iter->second->size();
		}

		size_t fieldsize(int field)
		{
			DO key;
			return (field < 0)? sizeof(DO)-8:key.size(field);
		}

		int bufcopy(char * sBuf, bool bInOut, int row, int field)
		{
			//std::cout << "Bufcopy = " << bInOut << "," << row << "," << field << std::endl; 

			auto obj = getObject(row);
			//field == -1 implies full copy
			if (field < 0)
			{
				if (bInOut)
				{
					DO key(row);
					key.clone(sBuf);
					//std::cout << key << std::endl;
					copyObject(&key);
				}
				else
				{ 
					if (obj == nullptr) return 0;
					memcpy(sBuf, obj, sizeof(DO) - 8);
				}
				return sizeof(DO) - 8;
			}

			if (obj == nullptr) return 0;
			int copysize = obj->copy(field, sBuf, bInOut);
			if (bInOut)
			{
				deleteObjFromIndices(obj);
				addObjToIndices(obj);
			}
			return copysize;
		}

		bool obj2str(const int &index, std::string &data) 
		{	
			auto obj = getObject(index);
			if (obj == nullptr) return false;
			std::stringstream ss;
			ss << (*obj);
			data = ss.str();
			return true;
		}

		bool str2obj(const std::string &data) 
		{
			std::vector<std::string> vec;
            tokenize(data, ',', vec);
			std::string type = DO::TableName();
			if (type == vec[0].substr(1+vec[0].find("=")).c_str())
			{
	            DO key(atoi(vec[1].substr(1+vec[1].find("=")).c_str()));
	            for (size_t i=2;i<vec.size();i++)
	                key.set(vec[i].substr(0,vec[i].find("=")), deserialize(vec[i].substr(1+vec[i].find("="))));
				return (copyObject(&key) != nullptr);
			}
			return false;
		}
    
        DO * copyObject(DO * obj)
        {
            if ( d_buffer_free.size() == 0 )
                return NULL; //Buffer full
            auto itr = d_buffer_free.begin();
            if (obj->d_row >= 0 )
            {
            	while (obj->d_row != *itr && itr != d_buffer_free.end()) ++itr;
            	if (itr == d_buffer_free.end())
            		return nullptr;
            }

            int index = *itr;
            d_buffer_free.erase(itr);
            d_buffer_free_size--;
            if ( d_buffer_free_size == 0 )
            {
                d_buffer_free_size = d_buffer_free.size();
                d_buffer_loop++;
            }
            int row = d_buffer_max * d_buffer_loop + index;
            //Copy operation from obj makes sure to exclude the last 8 bytes meant 
            //for d_row and dbid. But may be due to byte alignment, we see d_row  
            //still getting updated. Setting it explicitly for now using const cast
            DO * newobj = new (&d_buffer[index]) DO(row, (void *)obj);
            *(const_cast< int * >(&d_buffer[index].d_row))  = row;
            *(const_cast< uint32_t * >(&d_buffer[index].d_dbid)) = d_instanceid;
			d_size++;
            addObjToIndices(newobj);
            newobj->notifyFieldUpdate(-2);
            return newobj;
        };
    
		DO * createObject(int id = -1)
		{
            if ( d_buffer_free.size() == 0 )
                return NULL; //Buffer full
            auto itr = d_buffer_free.begin();
            if (id >= 0)
            {
            	while (id != *itr ) ++itr;
            	if (itr == d_buffer_free.end())
            		return nullptr;
            }
            int index = *itr;
            //std::cout << DO::TableName() << ":" << d_buffer_free.size() << std::endl;
            d_buffer_free.erase(itr);
            d_buffer_free_size--;
            if ( d_buffer_free_size == 0 )
            {
                d_buffer_free_size = d_buffer_free.size();
                d_buffer_loop++;
            }
			d_size++;
			addObjToIndices(new (&d_buffer[index]) DO(d_buffer_max * d_buffer_loop + index));
            *(const_cast< uint32_t * >(&d_buffer[index].d_dbid)) = d_instanceid;
            d_buffer[index].notifyFieldUpdate(-2);
			return &d_buffer[index];
		};
		
        void reset()
        {
            if (size() == 0 ) return;
            int idmax = (d_buffer_loop+1)*d_buffer_max;
            for (int i = 0; i < idmax; i++)
                removeObject(i);
            d_buffer_free.clear();
            for (int i=0; i < d_buffer_max; i++)
                d_buffer_free.push_back(i);
            d_buffer_free_size = d_buffer_max;
            d_buffer_loop = 0;
            d_size = 0;
        }

		bool removeObject(int id) 
		{
            if (size()==0) return false;
			DO * obj = getObject(id);
			if ( obj == NULL ) return false;

			//Donot reuse index for entity tables
			if (!DO::IsEntity())
				d_buffer_free.push_back(id%d_buffer_max); 

			deleteObjFromIndices(obj);
			obj->notifyFieldUpdate(-1);
			obj->~DO();
			memset(&d_buffer[id%d_buffer_max], 0xFF, sizeof(DO));
			d_size--;
			return true;
		};
		
		bool checkIndex()
		{
			bool status = true;
			for ( IndexContainerIterator iter = d_indices.begin(); iter != d_indices.end(); ++iter)
			{
				if (iter->second->size() != (size_t)d_size)
				{
					std::cout << "Index Error, Table " << DO::TableName()  
							  << " size : " << d_size << ",Index " << iter->first 
							  << " size : " << iter->second->size() << std::endl;  
					status = false;
				} 
			}
			return status;
		}

		DO * getObject(int id) 
		{
			if (d_size == 0) return NULL;
            int maxid = (d_buffer_loop+1)*d_buffer_max;
			return ( id < 0 || id > maxid || d_buffer[id%d_buffer_max].d_row != id )? NULL:&d_buffer[id%d_buffer_max];
		};
		
		int getObjectID(const DO * obj) 
		{
			//make sure memory offset and rowID match
			if ( obj == NULL) return -1;
			if (d_size == 0)  return -1;
			if ((long long)obj < (long long)&d_buffer[0] || (long long)obj > (long long)&d_buffer[d_buffer_max-1])
				return -1;
			int index = ((long long)obj - (long long)&d_buffer[0]) / sizeof(DO);
			return ( index < 0 || index > d_buffer_max || index != ((int)obj->d_row%d_buffer_max))? -1:(int)obj->d_row;
		};


        DO * findByUniqueKey(const string name, DO * key)
		{
            DomainTable<DO>::IndexIterator itrB, itrE;

			Index * index = getIndex(name);
            if (index == nullptr) return nullptr;
			*(const_cast<int *>(&key->d_row)) = -1;
            itrB = index->lower_bound(key);
            *(const_cast<int *>(&key->d_row)) = INT_MAX;
            itrE = index->upper_bound(key);
            *(const_cast<int *>(&key->d_row)) = -1;
            return ( itrB == itrE )? nullptr:*itrB;
		}

        DO * findByPrimaryKey(DO * key)
		{
			return findByUniqueKey("PrimaryKey", key);
		}
		
		bool begin(IndexIterator& iter, const string name, DO * obj = NULL)
		{
			Index * index = getIndex(name);
			if ( index == NULL ) return false;
			if (obj != NULL) *(const_cast< int * >(&obj->d_row)) = -1;
			iter = (obj==NULL)? index->begin():index->lower_bound(obj);
			return iter != index->end();
		};
		 
		bool end(IndexIterator& iter, const string name, DO * obj = NULL)
		{
			Index * index = getIndex(name);
			if ( index == NULL ) return false;
			if (obj != NULL) *(const_cast< int * >(&obj->d_row)) = INT_MAX;
			iter = (obj==NULL)? index->end():index->upper_bound(obj);
			return true;
		};

};

class DBChangeListener
{
	public:
		virtual void onRowAdd(uint8_t tableid, uint32_t row, DomainDB &db) = 0;
		virtual void onRowDelete(uint8_t tableid, uint32_t row) = 0;
		virtual void onRowChange(uint8_t tableid, uint32_t row, uint8_t field, DomainDB &db) = 0;
		virtual void nextTransactionNotice(uint32_t transid) = 0;
};

class DomainDB
{
	private:
		uint32_t d_DBID;
		uint64_t d_trans;
		DomainTableBase * d_tables[MAX_TABLE_SIZE];
		DomainLogger d_logger;
		std::vector<DBChangeListener *> d_listeners;
		
    public:

		DomainDB(const DomainDB &) = delete; // Copy constructor.  
		DomainDB& operator= (const DomainDB &) = delete; //copy assign
		
		DomainDB(uint32_t id) : d_DBID(id), d_trans(0), d_logger() {
			for (int i=0;i<MAX_TABLE_SIZE;i++)
				d_tables[i] = nullptr;
		}
		
		~DomainDB()
		{
			for (int i=0;i<MAX_TABLE_SIZE;i++)
				if (d_tables[i] != NULL) delete d_tables[i];
		}

		static DomainDB& instance(uint32_t dbid)
		{
            DomainDB * db = nullptr;
			static std::map<uint32_t,DomainDB *> dbMap;
            auto it = dbMap.find(dbid);
            if (it == dbMap.end())
                dbMap.insert(make_pair(dbid,db = new DomainDB(dbid)));
            else db = it->second;
			return *db;
		}

		//Size is passed only during initialization
        DomainLogger& getLogger(void * sBuf = NULL, int size = 0) 
		{ 
			if ( size > 0 ) 
				instance(d_DBID).d_logger.setBuffer(sBuf,size);
			return instance(d_DBID).d_logger;
		}

		void addDBChangeListener(DBChangeListener * listener)
		{
			if (listener == nullptr) return;
			d_listeners.push_back(listener);
		}

        bool getRow(const int &tableID, int32_t &row, std::string &data)
		{
			if (d_tables[tableID] == nullptr) return false;
			return d_tables[tableID]->obj2str(row, data);
		}	

		//copies obj into buffer and returns bytes copied
		int getRow(char * sBuf, int tableID, int row, int field = -1)
		{
			if (d_tables[tableID] == nullptr) return false;
			return d_tables[tableID]->bufcopy(sBuf, false, row, field);
		}

		//sets obj from buffer and returns status
		bool setRow(char * sBuf, int tableID, int row, int field = -1)
		{
			if (d_tables[tableID] == nullptr) return false;
			return d_tables[tableID]->bufcopy(sBuf, true, row, field) > 0;
		}

		bool delRow(int tableID, int row)
		{
			if (d_tables[tableID] == nullptr) return false;
			return d_tables[tableID]->removeObject(row);
		}

        bool setRow(const std::string &data)
		{
			for (int i=0;i<MAX_TABLE_SIZE;i++)
			{
				if (d_tables[i] == NULL) continue;
				if (d_tables[i]->str2obj(data)) return true;
			}
			return false;
		}	

		size_t fieldsize(int tableID, int field = -1)
		{
			if (d_tables[tableID] == nullptr) return false;
			return d_tables[tableID]->fieldsize(field);
		}

        void commit()
		{
			//transaction had nothing to log so return immediately
			if (d_logger.d_fields.size() == 0 && d_logger.d_newrows.size() == 0)
				return;

			d_trans++;
			//Notify all the listeners
			for (auto &listener: d_listeners)
			{
				for (auto &rec : d_logger.d_fields)
					if (std::get<3>(rec) == 255)
						listener->onRowDelete(std::get<2>(rec), std::get<4>(rec));	

				for (auto &rec : d_logger.d_newrows)
					listener->onRowAdd(std::get<2>(rec), std::get<4>(rec), *this);	

				for (auto &rec : d_logger.d_fields)
					if (std::get<3>(rec) != 255)
						listener->onRowChange(std::get<2>(rec), std::get<4>(rec), std::get<3>(rec), *this);	
				listener->nextTransactionNotice(d_trans);
			}
			instance(d_DBID).d_logger.reset();
		}	

        /*
         *  Database template functions:
         */
        //Only appllies for lookup tables, assumes first field is the perm key
        template <typename DO>
        std::string permkey(const int32_t index)
        {
            if (!DO::IsEntity()) return "";
            auto obj = lookupByIndex<DO>(index);
            return (obj == nullptr)? "":obj->getValue0();
        }

        //Only appllies for lookup tables, assumes first field is the key
        template <typename DO>
        DO * lookup(const std::string & permkey)
        {
            if (!DO::IsEntity()) return nullptr;
            DO key;
            key.set(key.getName0(),permkey);
            return lookup(key);
        }
    
        //Only appllies for lookup tables, assumes first field is the permanent key
        template <typename DO>
        int32_t fastindex(const std::string & permkey)
        {
            auto obj = lookup<DO>(permkey);
            return (obj == nullptr)? -1:obj->d_row;
        }

        template <typename DO>
        DO * lookupOrCreate(DO &keyobj, const std::string & index_name = "PrimaryKey")
        {
            DomainTable<DO> &table = DomainDB::instance(d_DBID).getTable<DO>();
            DO * obj = table.findByUniqueKey(index_name, &keyobj);
            return (obj==nullptr) ?  table.copyObject(&keyobj) : obj;
        }

        template <typename DO>
        DO * lookup(DO &keyobj, const std::string & index_name = "PrimaryKey")
        {
            DomainTable<DO> &table = DomainDB::instance(d_DBID).getTable<DO>();
            return table.findByUniqueKey(index_name, &keyobj);
        }

        template <typename DO>
        DO * copy(DO *keyobj)
        {
            DomainTable<DO> &table = DomainDB::instance(d_DBID).getTable<DO>();
            return table.copyObject(keyobj);
        }

        template <typename DO>
        DO * create(int32_t fastindex = -1)
        {
            DomainTable<DO> &table = DomainDB::instance(d_DBID).getTable<DO>();
            return table.createObject(fastindex);
        }

        template <typename DO>
        DO * lookupByIndex(const int index)
        {
            DomainTable<DO> &table = DomainDB::instance(d_DBID).getTable<DO>();
            return table.getObject(index);
        }

        template <typename DO>
        int32_t lookupIndex(DO * keyobj) const
        {
            DomainTable<DO> &table = DomainDB::instance(d_DBID).getTable<DO>();
            DO * obj = table.findByPrimaryKey(keyobj);
            return (obj == nullptr) ? -1 : obj->d_row;
        }

        template <typename DO>
        bool deleteByIndex(const int index) const
        {
            DomainTable<DO> &table = DomainDB::instance(d_DBID).getTable<DO>();
            return table.removeObject(index);
        }

        template <typename DO>
        int32_t deleteAll(const std::string & index_name = "PrimaryKey",
                              DO * start_key = NULL,
                              DO * end_key = NULL)
        {
        	typename DomainTable<DO>::IndexIterator start_itr, end_itr;
        	auto rc = iterator<DO>(start_itr,end_itr, index_name, start_key, end_key);
        	if (rc < 0) return rc;

        	//delete in 2 steps
            std::vector<int32_t> vec;
			for (;start_itr != end_itr; ++start_itr) 
				vec.push_back((*start_itr)->d_row);
			for (auto idx: vec) deleteByIndex<DO>(idx);

            return vec.size();
        }

        template <typename DO>
        int32_t getTableSize() const
        {
            DomainTable<DO> &table = DomainDB::instance(d_DBID).getTable<DO>();
            return table.capacity();
        }

        template <typename DO>
		bool checkIndex() const
		{
			DomainTable<DO> &table = DomainDB::instance(d_DBID).getTable<DO>();
			return table.checkIndex();
		}

        template <typename DO>
        int32_t getRecordCount(const std::string indexName = "") const
        {
            DomainTable<DO> &table = DomainDB::instance(d_DBID).getTable<DO>();
            return table.size(indexName);
        }

        template <typename DO>
        void resetTable()
        {
            DomainTable<DO> &table = DomainDB::instance(d_DBID).getTable<DO>();
            table.reset();
        }

        template <typename DO>
        void store(const std::string & filename, const std::string & index_name = "PrimaryKey")
        {
        	//std::cout << "Storing file " << filename << std::endl;
            if (getRecordCount<DO>() == 0) return;
            std::ofstream outfile(filename, std::ofstream::out | std::ofstream::app);
            if (DO::TableName().find("Lookup") == std::string::npos)
            {
                typename DomainTable<DO>::IndexIterator startItr, endItr;
                auto rc = iterator<DO>(startItr,endItr,index_name);
                if ( rc < 0 ) return;
                for (; startItr != endItr; ++startItr)
                    outfile << *(*startItr) << std::endl;
            }
            else
            {
                for (int i = 0; i < getTableSize<DO>(); i++)
                {
                    auto rec = lookupByIndex<DO>(i);
                    if (rec == nullptr) continue;
                    outfile << *rec << std::endl;
                }
            }
            outfile.close();
        }

        void loadAll(const std::string & filename)
        {
            std::string line;
            std::ifstream input(filename);
            if (!input.good()) return;
            while( std::getline(input, line))
	        	for (int i=0;i<MAX_TABLE_SIZE;i++)
					if (d_tables[i] != NULL) 
						d_tables[i]->str2obj(line);	
            input.close();
        }

        template <typename DO>
        void load(const std::string & filename)
        {
            std::vector<std::string> vec;
            std::ifstream input(filename);
            std::string line;
            std::string type = "DomainObject=";
            type.append(DO::TableName());
            if (!input.good()) return;
            while( std::getline(input, line))
            {
                tokenize(line, ',', vec);
                if (vec[0] != type) continue;
                DO key(atoi(vec[1].substr(1+vec[1].find("=")).c_str()));
                for (unsigned long i=2;i<vec.size();i++)
                    key.set(vec[i].substr(0,vec[i].find("=")), deserialize(vec[i].substr(1+vec[i].find("="))));

                copy<DO>(&key);
            }
            input.close();
        }

        template <typename DO>
        int32_t iterator(typename DomainTable<DO>::IndexIterator &start_itr,
                              typename DomainTable<DO>::IndexIterator &end_itr,
                              const std::string & index_name = "PrimaryKey",
                              DO * start_key = NULL,
                              DO * end_key = NULL)
        {
            DomainTable<DO> &table = DomainDB::instance(d_DBID).getTable<DO>();
            auto tblindex = table.getIndex(index_name);
            if (tblindex == nullptr) return -1;
            if (getRecordCount<DO>() <= 0)
            {
                start_itr = tblindex->end();
                end_itr = tblindex->end();
                return 0;
            }

            if (!table.begin(start_itr, index_name, start_key) || !table.end(end_itr, index_name, end_key))
            {
                start_itr = tblindex->end();
                end_itr = tblindex->end();
                return -1;
            }
            return 0;
        }

        //Returns begin iterator so as to use it for breaking reverse iteration loop
        //Example while (itrN != itr && itrN != itrB) itrN--;
        template <typename DO>
        int32_t riterator(typename DomainTable<DO>::IndexIterator &start_itr,
                              typename DomainTable<DO>::IndexIterator &end_itr,
                              typename DomainTable<DO>::IndexIterator &begin_itr,
                              const std::string & index_name = "PrimaryKey",
                              DO * start_key = NULL,
                              DO * end_key = NULL)
        {
            DomainTable<DO> &table = DomainDB::instance(d_DBID).getTable<DO>();
            auto tblindex = table.getIndex(index_name);
            if (tblindex == nullptr) return -1;
            begin_itr = tblindex->begin();
            if (getRecordCount<DO>() <= 0)
            {
                start_itr = tblindex->end();
                end_itr = tblindex->end();
                return 0;
            }
            if (!table.begin(start_itr, index_name, start_key) || !table.end(end_itr, index_name, end_key))
            {
                start_itr = tblindex->end();
                end_itr = tblindex->end();
                return -1;
            }

            return 0;
        }

        template <typename DO> DomainTable<DO> & getTable(void * sBuf = NULL, int size = 0)
        {
			if (d_tables[DO::TableID()] == NULL)
				d_tables[DO::TableID()] = new DomainTable<DO>(d_DBID);

			DomainTable<DO> * table = static_cast< DomainTable<DO> * > (d_tables[DO::TableID()]);
			if ( size > 0 )
            {
                if ( sBuf == nullptr )
                    sBuf = malloc(size);
                table->setBuffer(sBuf, size);
            }
			return *table;		
        }
}; 


template <typename DO>
class DomainObjectBase
{
	friend DomainTable<DO>;
	friend DomainLogger;

	protected:

		//fieldIndex -1 is delete and -2 is insert 
		void notifyFieldUpdate(int fieldIndex) 
		{
			auto &logger = DomainDB::instance(((DO *)this)->d_dbid).getLogger();
			if (fieldIndex == -2)
				logger.insert((DO *)this);
			else
				logger.log((DO *)this,fieldIndex);
		};
	
		void deleteFromIndices() 
		{
			DomainDB::instance(((DO *)this)->d_dbid).template getTable<DO>().deleteObjFromIndices((DO *)this);
		};
		
		void addToIndices() 
		{
			DomainDB::instance(((DO *)this)->d_dbid).template getTable<DO>().addObjToIndices((DO *)this);
		};

		template<typename t>
		void setString(EnumData<t>& temp, string& value)
		{
			temp = value;
		}

		template<typename t, size_t i>
		void setString(FixedArray<t,i>& temp, string& value)
		{
			std::vector<std::string> vec;
            tokenize(value,';',vec);
            size_t index = 0;
            for (auto& str:vec)
            {
                t d = (t)strtod(str.c_str(),nullptr);
                temp.set(d, index++);
            }
		}

		template<int i>
		void setString(FixedString<i>& temp, string& value)
		{
			temp = value;
		}

		void setString(Timestamp& temp, string& value)
		{
			temp = value;
		}

		void setString(double& temp, string& value)
		{
			temp = strtod(value.c_str(),nullptr);
		}

		//void setString(long& temp, string& value)	
		//{ 
		//	temp = stol(value);
		//}

		void setString(int32_t& temp, string& value)	
		{ 
			temp = stoi(value);
		}

		void setString(int16_t& temp, string& value)	
		{ 
			temp = stoi(value);
		}

		void setString(uint16_t& temp, string& value)	
		{ 
			temp = stoi(value);
		}

		void setString(int64_t& temp, string& value)	
		{ 
			temp = stol(value);
		}

		void setString(char& temp, string& value)		
		{
			temp = value[0];
		}

		int32_t buffer(void * sBuf)
		{
			if (sBuf != NULL)
				memcpy(sBuf, (void *)this, sizeof(DO) - 8);
			return sizeof(DO) - 8;
		}
        
		void clone(void * sBuf)
		{
			if (sBuf != NULL)
				memcpy(this, sBuf, sizeof(DO) - 8);
		}

    public:
		static bool IsEntity()
		{
			return (DO::TableName().find("Lookup") != std::string::npos);
		}

		string permkey()
		{
            if (!DO::IsEntity()) return "";
            return ((DO *)this)->getValue0();
		}    
		
        /////////////////////  MACRO FUNCTIONS FOR INTROSPECTION //////////////////////////////////////
        // Uses the SET macro to define the code and call SETN to repeat for each member recursively //
        /////////////////////  MACRO FUNCTIONS FOR INTROSPECTION //////////////////////////////////////
    
        string get(string name)
        {
            #define SET(FieldIndex) if (((DO *)this)->getName##FieldIndex() == name ) return ((DO *)this)->getValue##FieldIndex();
            SET0()
            #undef SET
            return "";
        }

        void set(string name, string value)
        {
            #define SET(FieldIndex) if (((DO *)this)->setValue##FieldIndex(name,value)) return;
            SET0()
            #undef SET
        }

        string getName(int iFieldIdx)
        {
            if ( iFieldIdx < 0 || iFieldIdx >= DO::maxFields()) return "";
            switch (iFieldIdx) 
            {
                #define SET(FieldIndex) case FieldIndex : return ((DO *)this)->getName##FieldIndex();
                SET0()
                #undef SET
            }
            return "";
        }

        string getValue(int iFieldIdx)
        {
            if ( iFieldIdx < 0 || iFieldIdx >= DO::maxFields()) return "";
            switch (iFieldIdx) 
            {
                #define SET(FieldIndex) case FieldIndex : return ((DO *)this)->getValue##FieldIndex();
                SET0()
                #undef SET
            }
            return "";
        }

        friend std::ostream & operator<<(std::ostream & out, DO& msg)
        {
            out << "DomainObject=" << DO::TableName();
            out << ",fastindex=" << msg.d_row;
            #define SET(FieldIndex) if (FieldIndex < DO::maxFields()) out << "," << msg.getName##FieldIndex() << "=" << serialize(msg.getValue##FieldIndex()); else return out;
            SET0()
            #undef SET
            return out;
        }
   
    private:
        size_t size(int iFieldIdx)
        {
            if ( iFieldIdx < 0 || iFieldIdx > DO::maxFields()) return 0;
            switch (iFieldIdx) 
            {
                #define SET(FieldIndex) case FieldIndex : return ((DO *)this)->size##FieldIndex();
                SET0()
                #undef SET
            }
            return 0;
        }

    	//returns the size of the data copied
        int copy(int iFieldIdx, char * sBuf, bool bInOut)
        {
            if ( iFieldIdx < 0 || iFieldIdx > DO::maxFields()) return 0;
            switch (iFieldIdx) 
            {
                #define SET(FieldIndex) case FieldIndex : return ((DO *)this)->copy##FieldIndex(sBuf,bInOut);
                SET0()
                #undef SET
            }
            return 0;
        }
};

