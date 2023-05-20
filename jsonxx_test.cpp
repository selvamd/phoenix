#include <cassert>
#include <sstream>
#include <string>
#include <iostream>
#include <fstream>
#include <string>
#include "jsonxx.hpp"
#include "gtest/gtest.h"

#include "TcpUtils.hpp"
#include "ux_selector.hh"
#include "SocketStreamerBase.hh"
#include "ReliableMulticastChannel.hpp"
#include "gtest/gtest.h"
#include <string>

namespace jsonxx
{
    //Settings::Assertions = Disabled;
    extern bool parse_string(std::istream& input, std::string& value);
    extern bool parse_number(std::istream& input, jsonxx::Number& value);
    extern bool match(const char* pattern, std::istream& input);
}

struct custom_type {};      // Used in a test elsewhere

namespace 
{
	class JsonTest : public ::testing::Test { };

	TEST_F(JsonTest, TestAllJsonParsing)
	{
        using namespace jsonxx;
        using namespace std;
        {
            string teststr("\"field1\"");
            string value;
            istringstream input(teststr);
            ASSERT_TRUE(parse_string(input, value));
            ASSERT_TRUE(value == "field1");
        }
        if( Parser != Strict )
        {
            string teststr("'field1'");
            string value;
            istringstream input(teststr);
            ASSERT_TRUE(parse_string(input, value));
            ASSERT_TRUE(value == "field1");
        }
        {
            string teststr("\"  field1\"");
            string value;
            istringstream input(teststr);
            ASSERT_TRUE(parse_string(input, value));
            ASSERT_TRUE(value == "  field1");
        }
        if( Parser != Strict )
        {
            string teststr("'  field1'");
            string value;
            istringstream input(teststr);
            ASSERT_TRUE(parse_string(input, value));
            ASSERT_TRUE(value == "  field1");
        }

        {
            string teststr("  \"field1\"");
            string value;
            istringstream input(teststr);
            ASSERT_TRUE(parse_string(input, value));
            ASSERT_TRUE(value == "field1");
        }
        if( Parser != Strict )
        {
            string teststr("  'field1'");
            string value;
            istringstream input(teststr);
            ASSERT_TRUE(parse_string(input, value));
            ASSERT_TRUE(value == "field1");
        }
        {
            // 'escaped text to unescaped text' test
            string teststr("\"\\b\\f\\n\\r\\t\\u000e\\u0002\"");
            string value;
            istringstream input(teststr);
            ASSERT_TRUE(parse_string(input, value));
            ASSERT_TRUE( value == "\b\f\n\r\t\xe\x2" );
        }
        {
            string teststr("6");
            istringstream input(teststr);
            Number value;
            ASSERT_TRUE(parse_number(input, value));
            ASSERT_TRUE(value == 6);
        }
        {
            string teststr(" }");
            istringstream input(teststr);
            ASSERT_TRUE(match("}", input));
        }
        {
            string teststr("{ \"field1\" : 6 }");
            if(UnquotedKeys == Enabled) {
                teststr = "{ field1 : 6 }";
            }
            istringstream input(teststr);
            Object o;
            ASSERT_TRUE(o.parse(input));
        }
        {
            string teststr("{ \"field1 : 6 }");
            istringstream input(teststr);
            Object o;
            ASSERT_TRUE(!o.parse(input));
        }

        {
            string teststr("6");
            istringstream input(teststr);
            Value v;
            ASSERT_TRUE(v.parse(input));
            ASSERT_TRUE(v.is<Number>());
            ASSERT_TRUE(v.get<Number>() == 6);
        }
        {
            string teststr("+6");
            istringstream input(teststr);
            Value v;
            ASSERT_TRUE(v.parse(input));
            ASSERT_TRUE(v.is<Number>());
            ASSERT_TRUE(v.get<Number>() == 6);
        }
        {
            string teststr("-6");
            istringstream input(teststr);
            Value v;
            ASSERT_TRUE(v.parse(input));
            ASSERT_TRUE(v.is<Number>());
            ASSERT_TRUE(v.get<Number>() == -6);
        }
        {
            string teststr("asdf");
            istringstream input(teststr);
            Value v;
            ASSERT_TRUE(!v.parse(input));
        }
        {
            string teststr("true");
            istringstream input(teststr);
            Value v;
            ASSERT_TRUE(v.parse(input));
            ASSERT_TRUE(v.is<Boolean>());
            ASSERT_TRUE(v.get<Boolean>());
        }
        {
            string teststr("false");
            istringstream input(teststr);
            Value v;
            ASSERT_TRUE(v.parse(input));
            ASSERT_TRUE(v.is<Boolean>());
            ASSERT_TRUE(!v.get<Boolean>());
        }
        {
            string teststr("null");
            istringstream input(teststr);
            Value v;
            ASSERT_TRUE(v.parse(input));
            ASSERT_TRUE(v.is<Null>());
            ASSERT_TRUE(!v.is<Boolean>());
        }
        {
            string teststr("\"field1\"");
            istringstream input(teststr);
            Value v;
            ASSERT_TRUE(v.parse(input));
            ASSERT_TRUE(v.is<String>());
            ASSERT_TRUE("field1" == v.get<String>());
            ostringstream stream;
            stream << v;
            ASSERT_TRUE(stream.str() == "\"field1\"");
        }
        if( Parser != Strict )
        {
            string teststr("'field1'");
            istringstream input(teststr);
            Value v;
            ASSERT_TRUE(v.parse(input));
            ASSERT_TRUE(v.is<String>());
            ASSERT_TRUE("field1" == v.get<String>());
            ostringstream stream;
            stream << v;
            ASSERT_TRUE(stream.str() == "\"field1\"");
        }
        {
            string teststr("[\"field1\", 6]");
            istringstream input(teststr);
            Array a;
            ASSERT_TRUE(a.parse(input));
            ASSERT_TRUE(a.has<String>(0));
            ASSERT_TRUE("field1" == a.get<String>(0));
            ASSERT_TRUE(a.has<Number>(1));
            ASSERT_TRUE(6 == a.get<Number>(1));
            ASSERT_TRUE(!a.has<Boolean>(2));
        }

        {
            string teststr(
                    "{"
                    "  \"foo\" : 1,"
                    "  \"bar\" : false,"
                    "  \"person\" : {\"name\" : \"GWB\", \"age\" : 60},"
                    "  \"data\": [\"abcd\", 42, 54.7]"
                    "}"
                    );
            if(UnquotedKeys == Enabled) {
                teststr = 
                        "{"
                        "  foo : 1,"
                        "  bar : false,"
                        "  person : {name : \"GWB\", age : 60},"
                        "  data: [\"abcd\", 42, 54.7]"
                        "}";
            }
            istringstream input(teststr);
            Object o;
            ASSERT_TRUE(o.parse(input));
            ASSERT_TRUE(1 == o.get<Number>("foo"));
            ASSERT_TRUE(o.has<Boolean>("bar"));
            ASSERT_TRUE(o.has<Object>("person"));
            ASSERT_TRUE(o.get<Object>("person").has<Number>("age"));
            ASSERT_TRUE(o.has<Array>("data"));
            ASSERT_TRUE(o.get<Array>("data").get<Number>(1) == 42);
            ASSERT_TRUE(o.get<Array>("data").get<String>(0) == "abcd");
            ASSERT_TRUE(o.get<Array>("data").get<Number>(2) - 54.7 < 1e-6 ||
                    - o.get<Array>("data").get<Number>(2) + 54.7 < 1e-6 );
            ASSERT_TRUE(!o.has<Number>("data"));
        }

        {
            string teststr("{\"bar\": \"a\\rb\\nc\\td\", \"foo\": true}");
            if(UnquotedKeys == Enabled) {
                teststr = "{bar: \"a\\rb\\nc\\td\", foo: true}";
            }
            istringstream input(teststr);
            Object o;
            ASSERT_TRUE(o.parse(input));
            ASSERT_TRUE(o.has<String>("bar"));
            ASSERT_TRUE(o.get<String>("bar") == "a\rb\nc\td");
            ASSERT_TRUE(o.has<Boolean>("foo"));
            ASSERT_TRUE(o.get<Boolean>("foo") == true);
        }
        {
            string teststr("[ ]");
            istringstream input(teststr);
            ostringstream output;
            Array root;
            ASSERT_TRUE(root.parse(input));
            output << root;
        }

        {
            string teststr("{}");
            istringstream input(teststr);
            Object o;
            ASSERT_TRUE(o.parse(input));
        }

        {
            string teststr("{\"attrs\":{}}");
            if(UnquotedKeys == Enabled) {
                teststr = "{attrs:{}}";
            }
            istringstream input(teststr);
            Object o;
            ASSERT_TRUE(o.parse(input));
            ASSERT_TRUE(o.has<Object>("attrs"));
        }

        {
            string teststr("{\"place\":{\"full_name\":\"Limburg, The Netherlands\""
                           ",\"attributes\":{},\"name\":\"Limburg\","
                           "\"place_type\":\"admin\",\"bounding_box\":{"
                           "\"type\":\"Polygon\",\"coordinates\":"
                           "[[[5.5661376,50.750449],[6.2268848,50.750449],"
                           "[6.2268848,51.7784841],[5.5661376,51.7784841]]]},"
                           "\"url\":\"http:\\/\\/api.twitter.com\\/1\\/geo\\/id\\/"
                           "4ef0c00cbdff9ac8.json\",\"country_code\":\"NL\","
                           "\"id\":\"4ef0c00cbdff9ac8\","
                           "\"country\":\"The Netherlands\"}}");
            if(UnquotedKeys == Enabled) {
                teststr = "{place:{full_name:\"Limburg, The Netherlands\""
                               ",attributes:{},name:\"Limburg\","
                               "place_type:\"admin\",bounding_box:{"
                               "type:\"Polygon\",coordinates:"
                               "[[[5.5661376,50.750449],[6.2268848,50.750449],"
                               "[6.2268848,51.7784841],[5.5661376,51.7784841]]]},"
                               "url:\"http:\\/\\/api.twitter.com\\/1\\/geo\\/id\\/"
                               "4ef0c00cbdff9ac8.json\",country_code:\"NL\","
                               "id:\"4ef0c00cbdff9ac8\","
                               "country:\"The Netherlands\"}}";
            }

            istringstream input(teststr);
            Object o;
            ASSERT_TRUE(o.parse(input));
            ASSERT_TRUE(o.has<Object>("place"));
            ASSERT_TRUE(o.get<Object>("place").has<Object>("attributes"));
        }


        {
            string teststr("{\"file\": \"test.txt\", \"types\": {\"one\": 1, \"two\": 2},"
                           "\"list\": [\"string\", 10]}");
            if(UnquotedKeys == Enabled) {
                teststr = "{file: \"test.txt\", types: {one: 1, two: 2},"
                               "list: [\"string\", 10]}";
            }
            istringstream input(teststr);
            Object o;
            ASSERT_TRUE(o.parse(input));
            ASSERT_TRUE(o.has<String>("file"));
            ASSERT_TRUE(o.has<Object>("types"));
            ASSERT_TRUE(o.get<String>("file", "lol.txt") == "test.txt");
            ASSERT_TRUE(o.get<Object>("types").has<Number>("one"));
            ASSERT_TRUE(!o.get<Object>("types").has<Number>("three"));
            ASSERT_TRUE(o.get<Object>("types").get<Number>("three", 3) == 3);
            ASSERT_TRUE(o.has<Array>("list"));
            ASSERT_TRUE(o.get<Array>("list").has<String>(0));
            ASSERT_TRUE(!o.get<Array>("list").has<String>(2));
            ASSERT_TRUE(o.get<Array>("list").get<String>(0) == "string");
            ASSERT_TRUE(o.get<Array>("list").get<String>(2, "test") == "test");
        }

        if( Parser != Strict && UnquotedKeys == Disabled /* '/' not supported as an identifier */ ) 
        {
            #define QUOTE(...) #__VA_ARGS__
            string input = QUOTE(
            {
              "name/surname":"John Smith",
              'alias': 'Joe',
              "address": {
                "streetAddress": "21 2nd Street",
                "city": "New York",
                "state": "NY",
                "postal-code": 10021,
              },
              "phoneNumbers": [
                "212 555-1111",
                "212 555-2222",
              ],
              "additionalInfo": null,
              "remote": false,
              "height": 62.4,
              "ficoScore": " > 640",
            }
            );

            string sample_output = QUOTE(
            <?xml version="1.0" encoding="UTF-8"?>
            <json:object xsi:schemaLocation="http://www.datapower.com/schemas/json jsonx.xsd"
                xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                xmlns:json="http://www.ibm.com/xmlns/prod/2009/jsonx">
              <json:string name="name/surname">John Smith</json:string>
              <json:string name="alias">Joe</json:string>
              <json:object name="address">
                <json:string name="streetAddress">21 2nd Street</json:string>
                <json:string name="city">New York</json:string>
                <json:string name="state">NY</json:string>
                <json:number name="postal-code">10021</json:number>
              </json:object>
              <json:array name="phoneNumbers">
                <json:string>212 555-1111</json:string>
                <json:string>212 555-2222</json:string>
              </json:array>
              <json:null name="additionalInfo" />
              <json:boolean name="remote">false</json:boolean>
              <json:number name="height">62.4</json:number>
              <json:string name="ficoScore">&gt; 640</json:string>
            </json:object>
            );

            Object o;
            if( o.parse(input) ) {
                //cout << o.xml(JSONx) << endl;            // XML output, JSONx flavor
                //cout << o.xml(JXML) << endl;             // XML output, JXML flavor
                //cout << o.xml(JXMLex) << endl;           // XML output, JXMLex flavor
                cout << o.xml(TaggedXML) << endl;        // XML output, tagged XML flavor
            }

            ASSERT_TRUE( jsonxx::validate(input) );
        }

        {
            jsonxx::Array a;
            a << true;
            a << false;
            a << 'A';
            a << 65;
            a << 65L;
            a << 65LL;
            a << 65U;
            a << 65UL;
            a << 65ULL;
            a << 65.f;
            a << 65.0;
            a << jsonxx::Value( "hello world" );
            a << std::string("hello world");
            a << "hello world";

            ASSERT_TRUE( jsonxx::Array().parse( a.json() ) );   // self-evaluation
            ASSERT_TRUE( validate( a.json() ) );                // self-evaluation
        }

        {
            jsonxx::Object o;
            o << "hello" << "world";
            o << "hola" << "mundo";
            ASSERT_TRUE( o.get<String>("hello") == "world" );
            ASSERT_TRUE( o.get<String>("hola") == "mundo" );
            o << "hello" << "mundo";
            o << "hola" << "world";
            ASSERT_TRUE( o.get<String>("hello") == "mundo" );
            ASSERT_TRUE( o.get<String>("hola") == "world" );
        }

        {
            // Generate JSON document dinamically
            jsonxx::Array a;
            a << jsonxx::Null();           // C++11: 'a << nullptr' is preferred
            a << 123;
            a << "hello \"world\"";
            a << 3.1415;
            a << 99.95f;
            a << 'h';

            jsonxx::Object o;
            o << "key1" << "value";
            o << "key2" << 123;
            o << "key3" << a;

            a << o;

            ASSERT_TRUE( jsonxx::Array().parse( a.json() ) );   // self-evaluation
            ASSERT_TRUE( validate( a.json() ) );                // self-evaluation
        }

        {
            // Generate JSON document dinamically
            jsonxx::Object o1;
            o1 << "key1" << "value 1";
            o1 << "key2" << 123;

            jsonxx::Object o2;
            o2 << "key3" << "value 2";
            o2 << "key4" << 456;

            o1 << "key3" << o2;

            ASSERT_TRUE( jsonxx::Object().parse( o1.json() ) );  // self-evaluation
            ASSERT_TRUE( validate( o1.json() ) );                // self-evaluation
        }

        {
            jsonxx::Array a;
            a << true;
            a << false;

            jsonxx::Object o;
            o << "number" << 123;
            o << "string" << "hello world";
            o << "boolean" << false;
            o << "null" << static_cast<void*>(0);
            o << "array" << a;
            o << "object" << jsonxx::Object("child", "object");
            o << "undefined" << custom_type();

            cout << o.write(JSON) << std::endl;        // same than o.json()
            //cout << o.write(JSONx) << std::endl;       // same than o.xml()
            //cout << o.write(JXML) << std::endl;        // same than o.xml(JXML)
            //cout << o.write(JXMLex) << std::endl;      // same than o.xml(JXMLex)
            cout << o.write(TaggedXML) << std::endl;      // same than o.xml(JXMLex)

            ASSERT_TRUE( jsonxx::Object().parse( o.json() ) );   // self-evaluation
            ASSERT_TRUE( validate( o.json() ) );                 // self-evaluation

            for(auto &kv : o.kv_map())
            {
                //Number,String,Boolean,Null,Array,Object
                std::cout << kv.first << ":" << kv.second->is<std::string>() << std::endl;
                //jsonxx::Object obj = kv.second->get<jsonxx::Object>();
            }
        }

        {
            // recursion test
            jsonxx::Array a;
            a << 123;
            a << "hello world";
            a << a;
            ASSERT_TRUE( a.size() == 4 );
        }

        {
            // recursion test
            jsonxx::Object o;
            o << "number" << 123;
            o << "string" << "world";
            o << "recursion" << o;
            ASSERT_TRUE( o.size() == 3 );
        }

        if( Parser != Strict )
        {
            // C++ style comments test
            string teststr(
                    "//this is comment #1 {\n"
                    "{"
                    "  \"foo\" : 1,"
                    "  \"bar\" : false, //this is comment #2\n"
                    "  \"person\" : {\"name //this shall not be removed\" : \"GWB\", \"age\" : 60},"
                    "  \"data\": [\"abcd\", 42, 54.7]"
                    "} //this is comment #3"
           );
            jsonxx::Object o;
            ASSERT_TRUE( o.parse(teststr) );
        }

        cout << "All tests ok." << endl;

	}

    TEST_F(JsonTest, PerformanceMovVsCopy)
    {
        string teststr("{\"place\":{\"full_name\":\"Limburg, The Netherlands\""
                       ",\"attributes\":{},\"name\":\"Limburg\","
                       "\"place_type\":\"admin\",\"bounding_box\":{"
                       "\"type\":\"Polygon\",\"coordinates\":"
                       "[[[5.5661376,50.750449],[6.2268848,50.750449],"
                       "[6.2268848,51.7784841],[5.5661376,51.7784841]]]},"
                       "\"url\":\"http:\\/\\/api.twitter.com\\/1\\/geo\\/id\\/"
                       "4ef0c00cbdff9ac8.json\",\"country_code\":\"NL\","
                       "\"id\":\"4ef0c00cbdff9ac8\","
                       "\"country\":\"The Netherlands\"}}");
        if(jsonxx::UnquotedKeys == jsonxx::Enabled) {
            teststr = "{place:{full_name:\"Limburg, The Netherlands\""
                           ",attributes:{},name:\"Limburg\","
                           "place_type:\"admin\",bounding_box:{"
                           "type:\"Polygon\",coordinates:"
                           "[[[5.5661376,50.750449],[6.2268848,50.750449],"
                           "[6.2268848,51.7784841],[5.5661376,51.7784841]]]},"
                           "url:\"http:\\/\\/api.twitter.com\\/1\\/geo\\/id\\/"
                           "4ef0c00cbdff9ac8.json\",country_code:\"NL\","
                           "id:\"4ef0c00cbdff9ac8\","
                           "country:\"The Netherlands\"}}";
        }

        istringstream input(teststr);
        jsonxx::Object o;
        o.parse(input);

        Timestamp t1, t2;
        t1.set();
        for (int i = 0;i<100000;i++)
        {
            auto o1 = o;    
        }
        t2.set();
        std::cout << "timediff for copy " << (t2() - t1()) << std::endl;

        t1.set();
        for (int i = 0;i<100000;i++)
        {
            auto o1 = std::move(o);    
        }
        t2.set();
        std::cout << "timediff for move " << (t2() - t1()) << std::endl;
    }

    
}  // namespace


