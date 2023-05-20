#ifndef _MACROS_HH_
#define _MACROS_HH_

#include <cstring>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <assert.h>
#include <iostream>
using namespace std;

//This macro prints name+args for caller function
//call as follows : puts( __PRETTY_FUNCTION__);
#ifndef PRINTFUNC
#define PRINTFUNC puts(__PRETTY_FUNCTION__);
#endif

#ifndef hton64
#define hton64(x) ((1==htonl(1)) ? (x) : ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
#endif

#ifndef ntoh64
#define ntoh64(x) ((1==ntohl(1)) ? (x) : ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))
#endif

/*
* Creates enums that are printable and comparable
* No spacing between macro var-args for enum values
*/
#ifndef DEFINE_ENUM_TYPES
#define DEFINE_ENUM_TYPES(EnumType, ...)                    \
    enum class EnumType { __VA_ARGS__ };                    \
    inline static std::string str4enum(EnumType) {          \
        return #__VA_ARGS__; };                             \
    inline std::ostream& operator<<                         \
        (std::ostream &out, EnumType v)                     \
        { out << EnumData<decltype (v)>(v);return out; };
#endif

/*
* Members can be FString<X> or any basic datatypes
*/
#ifndef DECLARE_MEMBER
#define DECLARE_MEMBER(FieldType, FieldName, FieldIndex)    \
    private:                                                \
        FieldType d_##FieldName;                            \
    public:	                                                \
        string getValue##FieldIndex() {                     \
            std::stringstream ss;                           \
            ss.setf( std::ios_base::fixed,                  \
                std::ios_base::floatfield );                \
            ss << "" << d_##FieldName;                      \
            return ss.str();                                \
        }                                                   \
        string getName##FieldIndex() {                      \
            return #FieldName;                              \
        }                                                   \
        bool setValue##FieldIndex(string n, string v) {     \
            if (n != getName##FieldIndex()) return false;   \
            FieldType temp;                                 \
            setString(temp,v);                              \
            set##FieldName(temp);                           \
            return true;                                    \
        }                                                   \
        size_t size##FieldIndex() {                         \
            return sizeof(d_##FieldName);                   \
        }                                                   \
        int copy##FieldIndex(char * sBuf, bool bInOut) {    \
            int size = sizeof(d_##FieldName);               \
            if (bInOut)                                     \
                memcpy(&d_##FieldName, sBuf, size);         \
            else                                            \
                memcpy(sBuf, &d_##FieldName, size);         \
            return size;                                    \
        }                                                   \
    public:                                                 \
        const FieldType & get##FieldName() const {          \
            return d_##FieldName;                           \
        }                                                   \
        void set##FieldName(const FieldType & val) {        \
            notifyFieldUpdate(FieldIndex);                  \
            d_##FieldName = val;                            \
        }
#endif

#ifndef DECLARE_INDEX
#define DECLARE_INDEX(FieldType, FieldName, FieldIndex)     \
    private:                                                \
        FieldType d_##FieldName;                            \
    public:                                                 \
        string getValue##FieldIndex() {                     \
            std::stringstream ss;                           \
            ss.setf( std::ios_base::fixed,                  \
                std::ios_base::floatfield );                \
            ss << "" << d_##FieldName;                      \
            return ss.str();                                \
        }                                                   \
        string getName##FieldIndex() {                      \
            return #FieldName;                              \
        }                                                   \
        bool setValue##FieldIndex(string n, string v) {     \
            if (n != getName##FieldIndex()) return false;   \
            FieldType temp;                                 \
            setString(temp,v);                              \
            set##FieldName(temp);                           \
            return true;                                    \
        }                                                   \
        size_t size##FieldIndex() {                         \
            return sizeof(d_##FieldName);                   \
        }                                                   \
        int copy##FieldIndex(char * sBuf, bool bInOut) {    \
            int size = sizeof(d_##FieldName);               \
            if (bInOut)                                     \
                memcpy(&d_##FieldName, sBuf, size);         \
            else                                            \
                memcpy(sBuf, &d_##FieldName, size);         \
            return size;                                    \
        }                                                   \
    public:                                                 \
        const FieldType & get##FieldName() const {          \
            return d_##FieldName;                           \
        }                                                   \
        void set##FieldName(const FieldType & val) {        \
            notifyFieldUpdate(FieldIndex);                  \
            deleteFromIndices();                            \
            d_##FieldName = val;                            \
            addToIndices();                                 \
        }                                                   \
        void def##FieldName() {                             \
            InitFunctions func;                             \
            func.initDefault(d_##FieldName);                \
        }
#endif

#ifndef DECLARE_END
#define DECLARE_END(FieldIndex)                             \
    public:                                                 \
        string getValue##FieldIndex()                       \
        {                                                   \
            cout << "Invalid getVALUE" <<                   \
                FieldIndex << endl;                         \
            return "";                                      \
        }                                                   \
        string getName##FieldIndex()                        \
        {                                                   \
            cout << "Invalid getName" <<                    \
                FieldIndex << endl;                         \
            return "";                                      \
        }                                                   \
        size_t size##FieldIndex() {                         \
            return 0;                                       \
        }                                                   \
        bool setValue##FieldIndex(string n, string v) {     \
            cout << "SET BAD FLD:" << n << v << endl;       \
            return true;                                    \
        }                                                   \
        int copy##FieldIndex(char * sBuf, bool bIn) {       \
            cout << "COPY BAD FLD:" << sBuf                 \
                << FieldIndex << bIn << endl;               \
            return 0;                                       \
        }
#endif

/*
 * Macros donot support loops directly
 * Using recursive invocation for the same effect
*/
#define SET63() SET(63)
#define SET62() SET(62) SET63()
#define SET61() SET(61) SET62()
#define SET60() SET(60) SET61()
#define SET59() SET(59) SET60()
#define SET58() SET(58) SET59()
#define SET57() SET(57) SET58()
#define SET56() SET(56) SET57()
#define SET55() SET(55) SET56()
#define SET54() SET(54) SET55()
#define SET53() SET(53) SET54()
#define SET52() SET(52) SET53()
#define SET51() SET(51) SET52()
#define SET50() SET(50) SET51()
#define SET49() SET(49) SET50()
#define SET48() SET(48) SET49()
#define SET47() SET(47) SET48()
#define SET46() SET(46) SET47()
#define SET45() SET(45) SET46()
#define SET44() SET(44) SET45()
#define SET43() SET(43) SET44()
#define SET42() SET(42) SET43()
#define SET41() SET(41) SET42()
#define SET40() SET(40) SET41()
#define SET39() SET(39) SET40()
#define SET38() SET(38) SET39()
#define SET37() SET(37) SET38()
#define SET36() SET(36) SET37()
#define SET35() SET(35) SET36()
#define SET34() SET(34) SET35()
#define SET33() SET(33) SET34()
#define SET32() SET(32) SET33()
#define SET31() SET(31) SET32()
#define SET30() SET(30) SET31()
#define SET29() SET(29) SET30()
#define SET28() SET(28) SET29()
#define SET27() SET(27) SET28()
#define SET26() SET(26) SET27()
#define SET25() SET(25) SET26()
#define SET24() SET(24) SET25()
#define SET23() SET(23) SET24()
#define SET22() SET(22) SET23()
#define SET21() SET(21) SET22()
#define SET20() SET(20) SET21()
#define SET19() SET(19) SET20()
#define SET18() SET(18) SET19()
#define SET17() SET(17) SET18()
#define SET16() SET(16) SET17()
#define SET15() SET(15) SET16()
#define SET14() SET(14) SET15()
#define SET13() SET(13) SET14()
#define SET12() SET(12) SET13()
#define SET11() SET(11) SET12()
#define SET10() SET(10) SET11()
#define SET9() SET(9) SET10()
#define SET8() SET(8) SET9()
#define SET7() SET(7) SET8()
#define SET6() SET(6) SET7()
#define SET5() SET(5) SET6()
#define SET4() SET(4) SET5()
#define SET3() SET(3) SET4()
#define SET2() SET(2) SET3()
#define SET1() SET(1) SET2()
#define SET0() SET(0) SET1()

#endif

