#ifndef _CONTAINERS_HPP_
#define _CONTAINERS_HPP_

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
using namespace std;
#include "macros.hh"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <algorithm>
#include <utility>
#include <stdint.h>
#include <inttypes.h>
#include <algorithm>

////////////////////////////////// RingBufferSPMC /////////////////////////////////////////////////////
template <typename DO>
class RingBufferSPMC
{
	public:
		RingBufferSPMC() :  _capacity(0) {}   
		virtual ~RingBufferSPMC() { _count->~atomic<uint64_t>(); }

		//can be done only once
		void setBuffer(void * sBuf, int size)
		{
			if ( _capacity > 0) return;
			_array = (DO *) sBuf;
			_capacity = ((size-8)/sizeof(DO));
			memset(_array, 0, size);
			_count = new (&_array[_capacity]) std::atomic<uint64_t>(0) ;
		}
		
		uint64_t end() { 
			return _count->load(std::memory_order_acquire); 
		};
		
		//write will not wait or get blocked	
		void write(const DO& rec)
		{ 
			uint64_t index = end();
			_array[index%_capacity] = rec;
			_count->store(index+1, std::memory_order_release);              
		}	
		
		//Return false if requested sequence is ahead of the write. 
		//Return oldest msg, if requested sequence is out of range.
		bool read(DO& item, const uint64_t& iReq, uint64_t& iRes)
		{
			uint64_t index = end();
			if ( index <= iReq ) return false; 			  	//nothing to read
			iRes = ( index >= iReq + _capacity )? 
				index - _capacity + 1:iReq;
			item = _array[iRes%_capacity];
			return true;
		}

	private:
		DO * _array;
		std::atomic<uint64_t> * _count; 
		int _capacity; 
};


///////////////////////////////////// SPSCQueue //////////////////////////////////////////////////
template <typename Element, size_t Size> 
class SPSCQueue {
    public:
        enum { Capacity = Size+1 };

        SPSCQueue() : _tail(0), _head(0){}   
        virtual ~SPSCQueue() {}
        
        bool push(const Element& item)
        {       
          const auto current_tail = _tail.load(std::memory_order_relaxed);  
          const auto next_tail = increment(current_tail);                   
          if(next_tail != _head.load(std::memory_order_acquire))                           
          {     
            _array[current_tail] = item;                                    
            _tail.store(next_tail, std::memory_order_release);              
            return true;
          }
          return false; // full queue
        }   
        
        bool pop(Element& item)
        {
          const auto current_head = _head.load(std::memory_order_relaxed);    
          if(current_head == _tail.load(std::memory_order_acquire))           
            return false; // empty queue

          item = _array[current_head];                                       
          _head.store(increment(current_head), std::memory_order_release);   
          return true;
        }

        bool empty() const
        {
          return (_head.load(std::memory_order_acquire) == _tail.load(std::memory_order_acquire));
        }
        
        bool full() const
        {
          auto index = _tail.load(std::memory_order_acquire);
          auto next_tail = (index + 1) % Capacity;
          return (next_tail == _head.load(std::memory_order_acquire));
        }

    private:
        size_t increment(size_t idx) const
        {
          return (idx + 1) % Capacity;
        }
        
        std::atomic<size_t>  _head; 
        Element   _array[Capacity];
        std::atomic<size_t>  _tail;  
};

////////////////////////////////////// MsgBuffer /////////////////////////////////////////////////

template <class T>
class MsgBuffer
{
	public:
    
        MsgBuffer()
        {
            capacity_       = 0;
            add_count_      = 0;
            flush_count_    = 0;
        }

		~MsgBuffer()
        {
            if (elems_ == nullptr)
                return;
            free (elems_);
        }

        inline int capacity()
        {
            return capacity_;
        }

        inline uint64_t firstSequence()
        {
            return flush_count_;
        }

        inline uint64_t nextAvailableSequence()
        {
            return add_count_;
        }
    
        // caller ensures external_memory has room for capacity * sizeof(T) bytes
        void setCapacity(int capacity)
        {
            capacity_       = capacity;
            elems_          = (T *)malloc(sizeof(T) * capacity);
        }

		inline uint64_t insert(const T &elem)
		{
            //Buffer full
            if ( size() >= capacity_ )
                return 0;
            
			elems_[add_count_++ %capacity_] = elem;
            
			return add_count_ - 1;
		}

		inline bool flush(uint64_t count)
		{
            //Buffer full
            if ( count < flush_count_ || count >= add_count_ )
                return false;
            flush_count_ = count+1;
			return true;
		}

        inline int size()
        {
            return (int)(add_count_ - flush_count_);
        }

        inline T * oldest()
        {
            if ( flush_count_ == add_count_ ) return nullptr;
            return &elems_[flush_count_%capacity_];
        }

        inline T * latest()
        {
            if ( flush_count_ == add_count_ ) return nullptr;
            return &elems_[(add_count_-1)%capacity_];
        }

        bool set(uint64_t count, const T * elem)
        {
            if ( count < flush_count_ || count >= add_count_ )
                return false;
            elems_[count%capacity_] = *elem;
            return true;
        }
    
        inline T * get(uint64_t count)
        {
            return ( count < flush_count_ || count >= add_count_ )?
                            NULL:&elems_[count%capacity_];
        }
    
		inline void reset(int sequence = 0)
        {
            add_count_   = sequence;
            flush_count_ = sequence;
        }

    private:
        uint64_t    add_count_;     //next empty slot
        uint64_t    flush_count_;   //valid oldest element
        int         capacity_;
        T *         elems_;
        
        MsgBuffer & operator=(const MsgBuffer &); 	//assigment op
        MsgBuffer(const MsgBuffer &);				//copy constructor
};

//////////////////////////////// SortedVector ///////////////////////////////////////////////////////

template <class Key, class T, class Comp>
struct CompAdapter : std::binary_function<std::pair<const Key, T>,std::pair<const Key, T>, bool>
{
    bool operator()(const std::pair<const Key, T>& x, const std::pair<const Key, T> &y) const { return d_comp(x.first, y.first); }
    bool operator()(const Key x, const std::pair<const Key, T>& y) const { return d_comp(x, y.first); }
    bool operator()(const std::pair<const Key, T>& x, const Key y) const { return d_comp(x.first, y); }
    bool operator()(const Key x, const Key y) const { return d_comp(x, y); }
    private:
        Comp d_comp;
};

template <class Key, class T, class Comp = std::less<Key>, class Alloc = std::allocator<std::pair<const Key,T>> >
class SortedVector
{
    typedef CompAdapter<Key, T, Comp> CompFunc;
    CompFunc d_compare;
    std::vector<std::pair<Key,T>> d_data;
    
    public:
        typedef Key key_type;
        typedef T mapped_type;
        typedef Comp key_compare;
        typedef std::pair<Key, T> value_type;
        typedef value_type& reference;
        typedef const value_type& const_reference;
        typedef size_t size_type;
        typedef ptrdiff_t difference_type;
        typedef typename std::vector<value_type>::iterator iterator;
        typedef typename std::vector<value_type>::const_iterator const_iterator;
        typedef typename std::vector<value_type>::reverse_iterator reverse_iterator;
        typedef typename std::vector<value_type>::const_reverse_iterator const_reverse_iterator;
        typedef Alloc AllocType;
        
    SortedVector(const Alloc& alloc = Alloc()) : d_data(alloc) {}    
    ~SortedVector() {}
    
    inline iterator begin() { return d_data.begin(); }
    inline const_iterator begin() const { return d_data.begin(); }
    inline iterator end() { return d_data.end(); }
    inline const_iterator end() const { return d_data.end(); }
    inline iterator rbegin() { return d_data.rbegin(); }
    inline const_iterator rbegin() const { return d_data.rbegin(); }
    inline iterator rend() { return d_data.rend(); }
    inline const_iterator rend() const { return d_data.rend(); }

    inline size_type size() const { return d_data.size(); }
    inline bool empty() const { return d_data.empty(); }
    
    void reserve(size_type n) { d_data.reserve(n); }
    void swap(SortedVector<Key,T,Comp, Alloc>& other)
    {
        d_data.swap(other.d_data);
        std::swap(d_compare,other.d_compare);
    }

    inline std::pair<iterator, bool> insert(const value_type& value) 
    {
        iterator itToInsert = std::lower_bound(d_data.begin(), d_data.end(), value, d_compare);
        if (itToInsert != d_data.end() && 
            !(d_compare(itToInsert->first, value.first)) && 
            !(d_compare(value.first,itToInsert->first)))
                return std::make_pair(itToInsert,false);
        iterator elemIt = d_data.insert(itToInsert, value);
        return std::make_pair(itToInsert,true);
    }
    
    bool push_back(const Key key, const T& value)
    {
        if (d_data.size() == 0 || d_compare(d_data.back().first,key))
        {
            d_data.push_back(std::make_pair(key,value));
            return true;    
        }
        return false;
    }
    
    inline iterator find(const Key value) const {
        return const_cast<SortedVector*>(this)->find(value);
    }

    inline iterator find(const Key key) {
        iterator it = std::lower_bound(d_data.begin(), d_data.end(), key, d_compare);
        if (it != d_data.end() && !(d_compare(it->first, key)) &&
            !(d_compare(key, it->first)))
                return it;
        return end();
    }

    inline void erase(const Key key) {
        iterator it = std::lower_bound(d_data.begin(), d_data.end(), key, d_compare);
        if (it != d_data.end() && !(d_compare(it->first, key)) &&
            !(d_compare(key, it->first)))
                d_data.erase(it);
    }

    inline void erase(iterator it) { d_data.erase(it); }
    inline void erase(iterator fi, iterator la) { d_data.erase(fi, la); }
    inline void clear() { d_data.clear(); }
    inline void resize(size_type n, value_type val = value_type()) { d_data.resize(n,val); }
    
    T& operator[](const Key key)
    {
        iterator it = find(key);
        if (it == end())
            it = insert(std::make_pair(key,T())).first;
        return it.second;
    }
    
    const T& operator[](const Key key) const 
    {
        return const_cast<SortedVector<Key,T,Comp,Alloc>*>(this)->operator[](key);
    }
};

//////////////////////////////// ZeroOneQueue ///////////////////////////////////////////////////////

/*
 This is a SPSC queue of indices based on lamport's queue augmented with empty element.
 This allows synchronization thru data instead of queue itself. Reader and writer pull indices
 to an external buffer from this queue instead of sharing pointers and synchronizing on them.
 
 Value 0 ==> data can be written into the buffer pointed by the index.
 Value 1 ==> data can be read from the buffer using that index.
 
 Reader pseudo code:
 while (q.isEmpty()) sleep;
 q.getReadPos(r_index)
 memcpy(localbuf, indexed_buf + r_index, bytestocopy)
 q.advance_read_ptr()
 
 Write pseudo code:
 while (q.isFull()) sleep();
 q.getWritePos(w_index);
 memcpy(indexed_buf+w_index, local_buf, bytesToCopy)
 q.advance_write_ptr()
 
 */

#if defined (__GNUC__)
#if defined (__powerpc__)
inline void MF_write_sync(void) { asm volatile("lwsync":::"memory"); }
inline void MF_read_sync(void) { asm volatile("isync":::"memory"); }
inline void MF_full_sync(void) { asm volatile("sync":::"memory"); }
#elif defined (__i386) || defined(__x86_64)
inline void MF_write_sync(void) { asm volatile("sfence":::"memory"); }
inline void MF_read_sync(void) { asm volatile("ifence":::"memory"); }
inline void MF_full_sync(void) { asm volatile("mfence":::"memory"); }
#elif defined(__sparc)
#define MF_write_sync(void) ((void)0)
#define MF_read_sync(void) ((void)0)
#define MF_full_sync(void) ((void)0)
#else 
# error "Unknown CPU implementation"
# endif
#elif defined (_IBMR2)

inline void MF_write_sync(void);
#pragma mc_func MF_write_sync {"7c2004ac"} //lwsync
#pragma reg_killed_by MF_write_sync

inline void MF_read_sync(void);
#pragma mc_func MF_read_sync {"4c00012c"} //isync
#pragma reg_killed_by MF_read_sync

inline void MF_full_sync(void);
#pragma mc_func MF_full_sync {"7c0004ac"} //sync
#pragma reg_killed_by MF_full_sync

#elif defined (__sparc)
#define MF_write_sync(void) ((void)0)
#define MF_read_sync(void) ((void)0)
#define MF_full_sync(void) ((void)0)

#elif defined (__ia64)
#include <ia64/sys/inline.h>
#define MF_write_sync(void) _Asm_mf() /* wrong should be st.rel */
#define MF_read_sync(void) _Asm_mf() /* wrong should be ld.acq */
#define MF_full_sync(void) _Asm_mf()
#else
#error "unknown cpu implementation or compiler."
#endif

template <size_t CAPACITY>
class ZeroOneQueue
{
    public:
        ZeroOneQueue(): readp_(0), writep_(0)
        { std::fill(buf_,buf_+CAPACITY,0x0);}

        // used by readers only    
        void getReadPos(uint32_t & data) { data = readp_; MF_read_sync(); }
        bool isEmpty() const { return buf_[readp_] == 0; }

        void advanceReadPtr() {
            MF_write_sync();    
            buf_[readp_] = 0x0;
            readp_ = (readp_+1 == CAPACITY)? 0:readp_+1;
        }
    
        void advance_read_ptr_by(uint32_t readSlots) {
            MF_write_sync();    
            if ( readp_ + readSlots < CAPACITY )
            {
                memset(buf_+readp_,0x0,readSlots*sizeof(uint64_t));
                readp_ += readSlots;
            }  
            else   
            {
                size_t unit = sizeof (uint64_t);
                uint32_t slotsAtTheEnd = CAPACITY - readp_;
                uint32_t slotsInFront = readSlots - slotsAtTheEnd;
                memset(buf_+readp_,0x0,slotsAtTheEnd*unit);
                memset(buf_,0x0,slotsInFront*unit);
                readp_ = slotsInFront;
            }
        }
        
        size_t getCount_r() const 
        {
            uint32_t localReadPtr = readp_;
            if (buf_[localReadPtr] == 0x0) return 0;
            uint32_t localWritePtr = writep_;
            uint32_t diff = (localReadPtr > localWritePtr)? 
                (CAPACITY - (localReadPtr - localWritePtr)):
                (localWritePtr-localReadPtr);
            if (diff == 0)
            {
                MF_read_sync();
                if (localReadPtr == 0)
                {
                    if (buf_[CAPACITY-1] == 0x0) return 1;
                    else return CAPACITY;
                }
                else if (buf_[localReadPtr-1]== 0x0) return 1;
                else return CAPACITY;
            }
            else return diff;
        }

        void getWritePos(uint32_t &data)  { 
            data = writep_;
            MF_read_sync(); 
        }
        
        void isFull() const { return buf_[writep_] == 0x1; } 
        
        void advance_write_ptr() 
        { 
            MF_write_sync();
            buf_[writep_]==0x1;
            writep_=(writep_+1 == CAPACITY)? 0:(writep_+1);
        }
        
    private:
        uint32_t readp_;
        char cache_line_pad1[128];    //cache line pad size
        uint32_t writep_;
        char cache_line_pad2[128];    //cache line pad size
        uint64_t buf_[CAPACITY];    
};

#endif