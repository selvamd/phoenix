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

#include "Containers.hpp"
#include "GlobalUtils.hpp"
#include "DomainDatabase.hh"
#include <atomic>
#include <climits>

//A listener that serializes db change logs and writes it to disk
//Packing Format = header[msgsize(2)+transid(4)] + [cTable,cField[255=>DELETE,254=>INSERT],iRowNum(4 byte),data]
//The implementation allows either read or write (trying to do both from same class even serially doesnot work. Not sure why).
//Please create seperate instances of reader and writer objects if we need to read and write. 
class DBFileSerializer : public DBChangeListener
{
	private:
		size_t  d_size;
		char    *d_buffer;
		size_t  d_maxsize;
		size_t  d_transid;

		fstream stream;
		streampos readpos;

		const size_t HEADERLEN = 6; // msgsize(2)+transid(4)

		//packs metadata information
		void packMetaData(uint8_t table, uint8_t field, uint32_t row)
		{
			d_buffer[d_size++] = table;
			d_buffer[d_size++] = field;  
		    d_buffer[d_size++] = (uint8_t)((row >> 24) & 0xff); 
		    d_buffer[d_size++] = (uint8_t)((row >> 16) & 0xff);
		    d_buffer[d_size++] = (uint8_t)((row >> 8) & 0xff); 
		    d_buffer[d_size++] = (uint8_t)(row & 0xff);
		}

		void publish()
		{
			d_buffer[0] = d_size/256;	
			d_buffer[1] = d_size%256;	
		    d_buffer[2] = (uint8_t)((d_transid >> 24) & 0xff); 
		    d_buffer[3] = (uint8_t)((d_transid >> 16) & 0xff);
		    d_buffer[4] = (uint8_t)((d_transid >> 8) & 0xff); 
		    d_buffer[5] = (uint8_t)(d_transid & 0xff);
			//write code to publish
			//stream.seekg (0, stream.end);
			stream.write(d_buffer, d_size);
			stream.flush();
			//streampos length = stream.tellg();
			//std::cout << "Publish function called with d_size = " << d_size << std::endl;
			reset();
		}

		//resets the buffer not the 
		//transactionid  or file handler
		void reset()
		{
			if (d_buffer == nullptr) return;
			memset(d_buffer,0,d_maxsize);
			d_size = HEADERLEN; 
		}

		void unpackRecord(DomainDB &db)
		{
			if (d_buffer == nullptr) return;
			int len = 256 * (uint8_t)d_buffer[0]+(uint8_t)d_buffer[1];
			int pos = HEADERLEN;
			while (pos < len - 1)
			{
				uint8_t table = d_buffer[pos++];
				uint8_t field = d_buffer[pos++];
				uint32_t  row = (uint8_t) d_buffer[pos++];
				row = (256 * row) + (uint8_t)d_buffer[pos++]; 
				row = (256 * row) + (uint8_t)d_buffer[pos++]; 
				row = (256 * row) + (uint8_t)d_buffer[pos++]; 
				//std::cout << "Unpack record = " << (int)table << "," << (int)field << "," << row << std::endl;
				//process as delete or update based on fieldId
				if (field == 255) db.delRow(table, row);
				else db.setRow(&d_buffer[pos], table, row, (field == 254)? -1:field);
				//only insert and update need to read further
				pos += db.fieldsize(table,(field == 254)? -1:field);
			}
		}

	public:

		DBFileSerializer(std::string file) : d_transid(0) 
		{
			d_buffer = nullptr;

			stream.open(file.c_str(), ios::in | ios::out | ios::binary | ios::app);

			//point to begin of file
			stream.seekg (0, stream.beg);
			readpos = stream.tellg();
		}

		~DBFileSerializer()
		{
			stream.close();
		}

		//required for packing
		void setBuffer(void * sBuf, size_t maxsize)
		{
			//set only once
			if (d_buffer != nullptr) return;
			d_buffer  = (char *) sBuf;
			d_maxsize = maxsize;
			reset();
		}

		//unpacking function for inserted rows
		void unpackLog(DomainDB &db)
		{
			while (!stream.eof())
			{
				reset();
				stream.read(d_buffer,2);
				int len = 256 * (uint8_t)d_buffer[0]+(uint8_t)d_buffer[1];
				stream.read(&d_buffer[2],len-2);
				//std::cout << "Unpack log with length = " << (int)d_buffer[0] << "," << (int)d_buffer[1] << "," << len << std::endl;
				unpackRecord(db);
			}
		}

		void nextTransactionNotice(uint32_t nexttransid)
		{
			if (d_size > HEADERLEN)
				publish();
			d_transid = nexttransid;
		} 

		void onRowAdd(uint8_t table, uint32_t row, DomainDB &db) 
		{
			if (d_buffer == nullptr) return;
			if (db.fieldsize(table,-1) + 6 + d_size >= d_maxsize) publish();
			packMetaData(table,254,row);//254 is the code for insert
			d_size += db.getRow(&d_buffer[d_size], table, row, -1);
		}

		void onRowDelete(uint8_t table, uint32_t row) 
		{
			if (d_buffer == nullptr) return;
			if (6 + d_size >= d_maxsize) publish();
			packMetaData(table,255,row);//254 is the code for insert
		}

		void onRowChange(uint8_t table, uint32_t row, uint8_t field, DomainDB &db) 
		{
			if (d_buffer == nullptr) return;
			if (db.fieldsize(table,field) + 6 + d_size >= d_maxsize) publish();
 			packMetaData(table,field,row); 
			d_size += db.getRow(&d_buffer[d_size], table, row, field);
		}
};

