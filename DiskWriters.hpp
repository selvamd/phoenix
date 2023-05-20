#pragma once

#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <iostream>
#include <string>
#include <cstdint>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <fcntl.h>        
#include <iostream>
#include <algorithm>

//Shared memory mapped file that multiple processes can share
class SharedMemoryMap
{
    public:

         SharedMemoryMap(const std::string& shm_id, uint32_t size_bytes):
             shm_id_(shm_id), size_(size_bytes), shm_fd_(0), data_(nullptr)
         {
              //The Shared Memory key must begin with a forward slash ('/') and 
              //have no other forward slashes
              std::replace( shm_id_.begin(), shm_id_.end(), '/', '_');
              shm_id_.insert(0, 1, '/');
         }

         //Block all moves and copies for this object
         SharedMemoryMap(const SharedMemoryMap &) = delete; // Copy constructor.  
         SharedMemoryMap(const SharedMemoryMap &&) = delete; // move constructor
         SharedMemoryMap& operator= (const SharedMemoryMap &) = delete; //copy assign
         SharedMemoryMap& operator=(SharedMemoryMap && other) = delete; //Move assignment

        ~SharedMemoryMap()
        {
            if (data_) munmap(data_, size_);
            if (shm_fd_ != 0) close(shm_fd_);
        }

        void* init()
        {
            //Perform cleanup
            if (size_ == 0) return nullptr;
            if (data_) munmap(data_, size_);
            if (shm_fd_ != 0) close(shm_fd_);
            shm_fd_ = 0;
            data_ = nullptr;
            
            //init shared memory
            int flags = O_CREAT | O_RDWR;
            int permissions = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
            shm_fd_ = shm_open(shm_id_.c_str(), flags, permissions); 
            if (shm_fd_ <= 0) return data_;
            
            //init memory map
            struct stat map_stat;
            fstat(shm_fd_, &map_stat);

            if (map_stat.st_size < size_ &&
                ftruncate(shm_fd_, size_) < 0)
                    return data_;

            
            flags = MAP_SHARED;
            int protection = PROT_READ | PROT_WRITE;

            data_ = mmap(0, size_, protection, flags, shm_fd_, 0);   
            if (data_ == MAP_FAILED) data_ = nullptr;
            
            return data_;
        }

    private:

        std::string shm_id_;
        uint32_t size_;
        int shm_fd_ = 0;
        void*  data_;
};


//Used for storing time series data
//Writes a series of fixed size double arrays to disk
//Supports random access to read back the same from disk
class BlockWriter
{
    private:
        std::fstream file; 
        int32_t buffersize;
        double * buffer;

    public:

         BlockWriter() {}

         void init(std::string fileName, int32_t arraysize)
         {
            //Open normally first
            file.open(fileName.c_str(), std::ios::out | std::ios::in | std::ios::binary);
            //create if it doesnot exist
            if (!file) file.open(fileName.c_str(), std::ios::out | std::ios::in | std::ios::binary | std::ios::trunc);
            buffersize = arraysize * sizeof(double);
            buffer = (double *) malloc(buffersize);
            memset(buffer,0,buffersize);
         }

         BlockWriter(const BlockWriter &) = delete; // Copy constructor.  
         BlockWriter(const BlockWriter &&) = delete; // move constructor
         BlockWriter& operator= (const BlockWriter &) = delete; //copy assign
         BlockWriter& operator=(BlockWriter&& other) = delete; //Move assignment
         virtual ~BlockWriter() { 
            file.flush();
            file.close(); 
            delete buffer;
            //std::cout << "Blockwriters destroyed" << std::endl; 
         } 

         bool set(const double &d, int32_t idx)
         {
            //std::cout << "value " << d << " for " << idx << std::endl;
            if (idx < 0 || idx >= (int32_t)(buffersize/sizeof(double))) 
                return false;
            buffer[idx] = d;
            return true;
         }

         double get(int32_t idx) { return buffer[idx]; }
         int size() { return buffersize/sizeof(double); }

         //returns next offset available
         long writeNew()
         {
            file.seekg( 0, std::ios::beg );
            auto fsize = file.tellg();
            file.seekg( 0, std::ios::end );
            fsize = file.tellg() - fsize;
            memset(buffer,0,buffersize);
            write(fsize);
            return fsize;
         }

         void write(const long offset)
         {
            file.seekp(offset, std::ios::beg);
            file.write((char *) buffer, buffersize);
            if (file.fail())
                std::cout << "error: in flush " << std::endl;
            file.flush();
            memset(buffer,0,buffersize);
         }

         const double * read(const long offset)
         {
            //file.seekg(std::ios::beg);
            memset(buffer,0,buffersize);
            file.seekg(offset, std::ios::beg);
            file.read((char *) buffer, buffersize);

           if (!file)
              std::cout << "error: only " << file.gcount() << " could be read" << std::endl;

            return buffer;
         }
};




