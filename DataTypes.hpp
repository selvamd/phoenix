#ifndef _DATA_TYPES_HPP_
#define _DATA_TYPES_HPP_

#include <iostream>
#include <string>
#include <chrono>
#include "GlobalUtils.hpp"
#include <algorithm>
#include <cassert>

/*
 *  Wrapper class around enumerations that provides print, translation, 
 *  assignment and compare functions for enum data types. 
 */

/********* This is a sample. Actual enums are macro generated in DomainObjects.hh *************
*    enum OrdType_t {  MARKET,LIMIT,STOP,STOPLIMIT };
*    inline static std::string str4enum(OrdType_t) { return "MARKET,LIMIT,STOP,STOPLIMIT"; };
*    inline std::ostream& operator<< (std::ostream &out, OrdType_t v) 
*    { out << EnumData<decltype (v)>(v);return out; };
***********************************************************************************************/

template <typename enumtype>
class EnumData
{
    private:
        enumtype sValue;
    
        void assignFromString(std::string s)
        {
            std::vector<std::string> enumVec;
            tokenize(str4enum(sValue), ',', enumVec);
            for (size_t i=0;i < enumVec.size();i++ )
                if (s == enumVec[i])
                    sValue = (enumtype)i;
        }
    
        std::string getString() const
        {
            std::vector<std::string> enumVec;
            tokenize(str4enum(sValue), ',', enumVec);
            if ((size_t)sValue >= enumVec.size())
                return "Unknown";
            return enumVec[(size_t)sValue];
        }
        
    public:
        static constexpr int32_t INVALID = 0xFF;
    
        void init()
        {
            sValue = (enumtype)0xFF;
        }

        EnumData()
        {
            init();
        };

        EnumData(const std::string& data)
        { 
            init();
            assignFromString(data); 
        }

        EnumData(const int8_t& data)    
        { 
            init();
            sValue = (enumtype)data; 
        }

        EnumData(const EnumData& data)  
        { 
            init();
            sValue = data.sValue; 
        }

        EnumData(const enumtype& data)  
        { 
            init();
            sValue = data; 
        }
  
        bool isValid() const
        {
            std::vector<std::string> enumVec;
            tokenize(str4enum(sValue), ',', enumVec);
            return ((size_t)sValue < enumVec.size());
        }

        //returns number of enums
        size_t enumcount() const
        {
            static size_t enumcount = 0;
            if (enumcount == 0)
            {
                std::vector<std::string> enumVec;
                tokenize(str4enum(sValue), ',', enumVec);
                enumcount = enumVec.size(); 
            }
            return enumcount;
        }

        size_t size() const
        { 
            return sizeof(enumtype);  
        }

        std::string toString() const
        { 
            return getString();
        }

        void operator =(const int8_t& data)             
        { 
            sValue = (enumtype)data; 
        }

        void operator =(const std::string& data)             
        { 
            assignFromString(data); 
        }

        void operator =(const enumtype data)            
        { 
            sValue = data; 
        }

        void operator =(const EnumData<enumtype> data)  
        { 
            sValue = data.sValue; 
        }
    
        enumtype operator ()() const
        { 
            return sValue; 
        }
      
        operator int()
        {
            return (int)sValue;
        }

        bool operator < (const enumtype& data) const
        {
            return (int8_t)sValue < (int8_t)data();
        }

        int compareTo (const EnumData& data) const
        {
            if (sValue == data()) return 0;
            return (sValue < data())? -1:1;
        }

        int compareForIndex (const EnumData& data) const
        {
            enumtype temp = (enumtype)0xFF;
            if ( temp == sValue || temp == data.sValue) return 0;
            if (sValue == data()) return 0;
            return (sValue < data())? -1:1;
        }
        
        friend std::ostream & operator << (std::ostream & out, const EnumData & data)
        {
            out << data.toString();
            return out;
        }
        
        ~EnumData() {}
};


////////////////////////////////////// FIXED ARRAY //////////////////////////////////////////
template <typename datatype, size_t maxsize>
class FixedArray
{
    datatype sBuf[maxsize];

    public:
        
        FixedArray() { init(); };

        FixedArray(const FixedArray& data) {
            for (size_t i=0;i < size();i++)
                sBuf[i] = data.sBuf[i];
        }
        
        size_t size() const { return maxsize; }

        void operator =(const FixedArray& data)
        {
            for (size_t i=0;i < size();i++)
                sBuf[i] = data.sBuf[i];
        }
    
        bool operator < (const FixedArray& data) const
        {
            for (size_t i=0;i < size();i++)
                if (sBuf[i] != data.sBuf[i])
                    return sBuf[i] < data.sBuf[i];
            return size() < data.size();
        }

        int compareTo (const FixedArray& data) const
        {
            for (size_t i=0;i < size();i++)
                if (sBuf[i] != data.sBuf[i])
                    return (sBuf[i] < data.sBuf[i])? +1:-1;
            if (size() == data.size()) return 0;
            return (size() < data.size())? +1:-1;
        }

        void init() {
            for (size_t i=0;i < maxsize;i++)
                sBuf[i] = 0;
        }

        datatype get(size_t index) const {
            return (index > size())? 0:sBuf[index];
        }

        //cannot do this if part of domainobject 
        //because this has to be updated only 
        //by a set call on domainobject

        //datatype& operator[] (const size_t index)
        //{
        //    assert (index >= 0 && index < size());
        //    return sBuf[index];
        //}

        const datatype& operator[] (const size_t index) const
        {
            assert (index < size());
            return sBuf[index];
        }

        size_t find(const datatype &val) const {
            for (size_t i=0;i < size();i++)
                if (val == sBuf[i])
                    return i;
            return maxsize;
        }
    
        bool set(const datatype &val, size_t index)
        {
            if (index >= maxsize) return false;
            sBuf[index] = val;
            return true;
        }
    
        //special compare which treats any compare with empty string as match.
        //Used for partial lookup with DB indices
        int compareForIndex (const FixedArray& data) const
        {
            return compareTo(data);
        }

        friend std::ostream & operator<<(std::ostream & out, const FixedArray & data)
        {
            bool empty = true;
            for (size_t i=0;i < data.size();i++)
                if (data.sBuf[i] != 0) empty = false;
            if (empty) return out;
            for (size_t i=0;i < data.size() - 1;i++)
                out << data.sBuf[i] << ";";
            out << data.sBuf[data.size()-1];
            return out;
        }
        
        ~FixedArray() {}
};

/*template <typename datatype, size_t maxsize>
class FixedArray
{
    datatype sBuf[maxsize+1];

    public:
        
        FixedArray() { init(); };

        FixedArray(const FixedArray& data) {
            sBuf[maxsize] = data.sBuf[maxsize];
            for (size_t i=0;i < size();i++)
                sBuf[i] = data.sBuf[i];
        }
        
        size_t size() const { return sBuf[maxsize]; }
        size_t capacity()   { return maxsize;       }

        void operator =(const FixedArray& data)
        {
            sBuf[maxsize] = data.sBuf[maxsize];
            for (size_t i=0;i < size();i++)
                sBuf[i] = data.sBuf[i];
        }
    
        bool operator < (const FixedArray& data) const
        {
            for (size_t i=0;i < size();i++)
                if (sBuf[i] != data.sBuf[i])
                    return sBuf[i] < data.sBuf[i];
            return sBuf[maxsize] < data.sBuf[maxsize];
        }

        int compareTo (const FixedArray& data) const
        {
            for (size_t i=0;i < size();i++)
                if (sBuf[i] != data.sBuf[i])
                    return (sBuf[i] < data.sBuf[i])? +1:-1;
            if (size() == data.size()) return 0;
            return (size() < data.size())? +1:-1;
        }

        //initialize with array prefilled 
        //with 0 if empty = false
        void init(bool empty = true) {
            for (size_t i=0;i <= maxsize;i++)
                sBuf[i] = 0;
            if (!empty) sBuf[maxsize] = maxsize;
        }

        datatype get(size_t index) const {
            return (index < 0 || index > size())? 0:sBuf[index];
        }

        //cannot do this if part of domainobject 
        //because this has to be updated only 
        //by a set call on domainobject

        //datatype& operator[] (const size_t index)
        //{
        //    assert (index >= 0 && index < size());
        //    return sBuf[index];
        //}

        const datatype& operator[] (const size_t index) const
        {
            assert (index >= 0 && index < size());
            return sBuf[index];
        }

        size_t find(const datatype &val) const {
            for (size_t i=0;i < size();i++)
                if (val == sBuf[i])
                    return i;
            return maxsize;
        }
    
        bool remove(const datatype &val, int index = -1)
        {
            if (index == -1) index = find(val);
            if (index >= 0 && index < (int)size())
            {
                for (size_t i=index;i < size();i++)
                    sBuf[i] = sBuf[i+1];
                sBuf[maxsize]--;    
                return true;
            }
            return false;
        }
        
        bool set(const datatype &val, int index = -1)
        {
            if (index > (int)size()) return false;
            if (index == -1 && size() >= maxsize) return false;
            if (index == -1) index = sBuf[maxsize]++;
            sBuf[index] = val;
            sBuf[maxsize] = std::max<int>(index,sBuf[maxsize]);
            return true;
        }
    
        //special compare which treats any compare with empty string as match.
        //Used for partial lookup with DB indices
        int compareForIndex (const FixedArray& data) const
        {
            if ( size() == 0 || data.size() == 0 ) return 0;
            return compareTo(data);
        }

        friend std::ostream & operator<<(std::ostream & out, const FixedArray & data)
        {
            if (data.size() <= 0) return out;
            for (size_t i=0;i < data.size() - 1;i++)
                out << data.sBuf[i] << ";";
            out << data.sBuf[data.size()-1];
            return out;
        }
        
        ~FixedArray() {}
};
*/
////////////////////////////////////// FIXED STRING //////////////////////////////////////////
template <int maxsize>
class FixedString
{
    char sBuf[maxsize];

    private:
        void copy(const void * source)
        {
            memset(sBuf,0,maxsize);
            memcpy(sBuf, source, maxsize-1);
        }
        
    public: 
        
        FixedString() {};
        FixedString(const char * data)  { copy(data); }
        FixedString(const std::string& data) { copy(data.c_str()); }
        FixedString(const FixedString& data) { copy(data.c_str()); }
        
        size_t size() { return maxsize;  }
        
        void operator =(const std::string& data) { copy(data.c_str()); }
        void operator =(const char * data)  { copy(data); }
        
        bool operator < (const FixedString& data) const
        {
            return strncmp(sBuf,data.sBuf,maxsize) < 0;
        }

        int compareTo (const FixedString& data) const
        {
            return strncmp(sBuf,data.sBuf,maxsize);
        }

        //special compare which treats any compare with empty string as match.
        //Used for partial lookup with DB indices
        int compareForIndex (const FixedString& data) const
        {
            if ( data.sBuf[0] == (char)0xFF && data.sBuf[1] == (char)0 )
                return 0;
            if ( sBuf[0] == (char)0xFF && sBuf[1] == (char)0 )
                return 0;
            return strncmp(sBuf,data.sBuf,maxsize);
        }

        int length () const
        {
            return strnlen(sBuf,maxsize);
        }

        std::string toString() const
        { 
            std::string s;
            return (s = sBuf);
        }


        const char * c_str() const  
        {
            return sBuf;
        }
        
        friend std::ostream & operator<<(std::ostream & out, const FixedString & fstring)
        {
            out << fstring.sBuf;
            return out;
        }
        
        ~FixedString() {}
};

////////////////////////////////////// TIMESTAMP //////////////////////////////////////////
class Timestamp
{
    uint64_t time;
    
    public:
    
        Timestamp(uint64_t t=0) : time(t) {}

        Timestamp(std::tm& ttm)
        {
            time_t ttime_t = mktime(&ttm);

            std::chrono::system_clock::time_point time_point_result = std::chrono::system_clock::from_time_t(ttime_t);
            const std::chrono::high_resolution_clock::duration since_epoch = time_point_result.time_since_epoch();
            time = std::chrono::duration_cast<std::chrono::microseconds>(since_epoch).count();
        }

        void operator =(const std::string& data) {
            if (data.empty())
                init();
            else if (std::count(data.begin(), data.end(), '-') == 2) // format: YYYY-MM-DD
                date_to_micros(data);
            else if (std::count(data.begin(), data.end(), '-') == 0) // format: YYYYMMDD
                string_to_micros(std::string(data).append("-00:00:00.000000"));
            else // format: YYYYMMDD-HH:MM:SS.ssssss
                string_to_micros(data);
        }

        void init() { time = 0; }

        void set()
        {
            std::chrono::system_clock::time_point time_point_now = std::chrono::system_clock::now();
            time = std::chrono::duration_cast<std::chrono::microseconds>(time_point_now.time_since_epoch()).count();
        }

        void operator =(const char * /* data */  )  { }
        
        bool operator ==(const Timestamp& data) const
        {
            return time == data.time;
        }
        
        bool operator < (const Timestamp& data) const
        {
            return time < data.time;
        }
    
        operator uint64_t() { return time; }
        
        uint64_t operator ()() const { return time; }
    
        int compareTo (const Timestamp& data) const
        {
            if (time == data.time) return 0;
            return (time > data.time)? 1:-1; 
        }

        int compareForIndex (const Timestamp& data) const
        {
            if (time == 0 || data.time == 0 ) return 0;
            if (time == data.time) return 0;
            return (time > data.time)? 1:-1;
        }

        friend std::ostream & operator<<(std::ostream & out, const Timestamp & ftime)
        {
            out << ftime.micros_to_string("%Y%m%d-%H:%M:%S", true);
            return out;
        }
    
        std::string toStringWithMicros() const
        {
            return toString(true);
        }

        std::string toString(const bool show_millis = false, const std::string & format = "%Y-%m-%d %H:%M:%S") const
        {
            return micros_to_string(format.c_str(), show_millis);
        }

        std::string getFixString() const
        {
            return micros_to_string("%Y%m%d");
        }

        /*
         *  Convenience Functions:
         */
        std::string getMysqlDate() const
        {
            return micros_to_string("%Y-%m-%d");
        }

        std::string getXmlApiDate() const
        {
            return micros_to_string("%Y%m%d");
        }

        std::string getXmlApiTime() const
        {
            return micros_to_string("%H%M%S");
        }

        int64_t getInternalDate() const
        {
            return std::stol(micros_to_string("%Y%m%d"), NULL);
        }

        int64_t getInternalTime() const
        {
            return std::stol(micros_to_string("%H%M%S"), NULL);
        }

        void subtractDays(int32_t days)
        {
            std::chrono::hours hours(days * 24);
            std::chrono::microseconds micros = std::chrono::duration_cast<std::chrono::microseconds>(hours);

            time -= micros.count();
        }

        int daysDiff(Timestamp another) const
        {
            static std::chrono::hours hours(24);
            std::chrono::microseconds micros = std::chrono::duration_cast<std::chrono::microseconds>(hours);
            return (another.time > time)? (another.time - time)/micros.count():
                                          (time - another.time)/micros.count();
        }

        void addDays(int32_t days)
        {
            std::chrono::hours hours(days * 24);
            std::chrono::microseconds micros = std::chrono::duration_cast<std::chrono::microseconds>(hours);

            time += micros.count();
        }

    private:
        void date_to_micros(const std::string &str)
        {
            int yyyy, mm, dd;
            
            //YYYYMMDD-HH:MM:SS.ssssss
            char scanf_format[] = "%4d-%2d-%2d";
            
            sscanf(str.c_str(), scanf_format, &yyyy, &mm, &dd);
            if (yyyy < 2000 || yyyy > 3000 || mm < 1 || mm > 12 || dd < 1 || dd > 31)
            {
                time = 0;
                return;
            }
            
            tm ttm = tm();
            ttm.tm_year = yyyy - 1900; // Year since 1900
            ttm.tm_mon = mm - 1; // Month since January
            ttm.tm_mday = dd; // Day of the month [1-31]
            ttm.tm_hour = 0; // Hour of the day [00-23]
            ttm.tm_min = 0;
            ttm.tm_sec = 0;
            
            time_t ttime_t = mktime(&ttm);
            
            std::chrono::system_clock::time_point time_point_result = std::chrono::system_clock::from_time_t(ttime_t);
            const std::chrono::high_resolution_clock::duration since_epoch = time_point_result.time_since_epoch();
            time = std::chrono::duration_cast<std::chrono::microseconds>(since_epoch).count();
        }
        
        void string_to_micros(const std::string &str)
        {
            int yyyy, mm, dd, HH, MM, SS, fff;
            
            //YYYYMMDD-HH:MM:SS.ssssss
            char scanf_format[] = "%4d%2d%2d-%2d:%2d:%2d.%6d";
            
            sscanf(str.c_str(), scanf_format, &yyyy, &mm, &dd, &HH, &MM, &SS, &fff);
            if (yyyy < 2000 || yyyy > 3000 || mm < 1 || mm > 12 || dd < 1 || dd > 31)
            {
                time = 0;
                return;
            }
            
            tm ttm = tm();
            ttm.tm_year = yyyy - 1900; // Year since 1900
            ttm.tm_mon = mm - 1; // Month since January
            ttm.tm_mday = dd; // Day of the month [1-31]
            ttm.tm_hour = HH; // Hour of the day [00-23]
            ttm.tm_min = MM;
            ttm.tm_sec = SS;
            
            time_t ttime_t = mktime(&ttm);
            
            std::chrono::system_clock::time_point time_point_result = std::chrono::system_clock::from_time_t(ttime_t);
            time_point_result += std::chrono::microseconds(fff);
            const std::chrono::high_resolution_clock::duration since_epoch = time_point_result.time_since_epoch();
            time = std::chrono::duration_cast<std::chrono::microseconds>(since_epoch).count();
        }
        
        std::string micros_to_string(const std::string & date_time_format, const bool show_millis = false) const
        {
            std::chrono::system_clock::time_point tp;
            tp += std::chrono::microseconds(time);
            
            auto ttime_t = std::chrono::system_clock::to_time_t(tp);
            auto tp_sec = std::chrono::system_clock::from_time_t(ttime_t);
            std::chrono::microseconds ms = std::chrono::duration_cast<std::chrono::microseconds>(tp - tp_sec);
            
            std::tm * ttm = localtime(&ttime_t);
            
            char time_str[50];
            
            strftime(time_str, sizeof(time_str), date_time_format.c_str(), ttm);
            
            std::string result(time_str);
            if (show_millis)
            {
                result.append(".");
                long num = 1000000 + ms.count();
                result.append(std::to_string(num).substr(1));
            }
            
            return result;
        }
};


#endif


