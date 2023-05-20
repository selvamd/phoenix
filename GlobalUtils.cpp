#include "GlobalUtils.hpp"
#include "Logger.hpp"

#include <inttypes.h>
#include <fstream>
#include <iostream>
#include <string>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <float.h>
#include <vector>
#include <sstream>
#include "jsonxx.hpp"
#include "DataTypes.hpp"

//type u=unsigned, i=signed, d=double, s=string
std::string jsonfield(jsonxx::Object &req, const std::string &name, const char type, const int32_t index)
{
    std::string s = "";
    if (req.has<jsonxx::String>(name)) 
        s = req.get<jsonxx::String>(name);    
    
    if (req.has<jsonxx::Array>(name)) 
    {
        auto array = req.get<jsonxx::Array>(name); 
        if (array.has<jsonxx::String>(index))
            s = array.get<jsonxx::String>(index);
    }

    if (type == 's') return s;
    if (!isNumber(s)) return (type == 'i')? "-1":"0";    
    if (type == 'd') return s;
    //check if whole number 
    double d = strtod(s.c_str(),nullptr);
    if (floor(d) != d) 
        return (type == 'i')? "-1":"0";
    //check if negative 
    if (type == 'u' && d < 0) return "0";
    return s;
}

bool isNumber(const std::string& s)
{
    for (size_t i=0;i<s.size();i++)
    {
        char c = s.c_str()[i];
        if (i == 0 && c == '-') continue;
        if (i == 0 && c == '+') continue;
        if (c == '.') continue;
        if (c >= '0' && c <= '9') continue;
        return false;        
    }
    return (s.size() > 0);
}

std::string NumberToString(double d, int precision)
{
    std::stringstream stream;
    stream << std::fixed << std::setprecision(precision) << d;
    return stream.str();
}

//Turns ((b+(c/d))*a) ==> 2b+,3c/,3d*,1a (number prefix represent depth level of paranthesis)
void parseExpr(const std::string exprstr, bool bCompareNotCompute, std::vector<expr> &result)
{
    result.clear();
    char brkt = 0, word[100];
    int closes = 0, wordindex = 0;

    memset(word,0,100);
    auto cstr = exprstr.c_str();

    for (size_t i=0; i < exprstr.length(); i++)
    {
        bool newexpr = false;
        if (bCompareNotCompute)
            newexpr = (cstr[i] == '&' || cstr[i] == '|');
        else 
            newexpr = (cstr[i] == '+' || cstr[i] == '-' || cstr[i] == '*' || cstr[i] == '/');

        if (newexpr || i == exprstr.length() - 1)
        {
            expr e;
            e.brkt = brkt + closes;
            e.oper = cstr[i];
            if (i+1 == exprstr.length())
                if (cstr[i] != ')')
                    word[wordindex] = cstr[i];
            e.name = word;
            result.push_back(e);
            wordindex = closes = 0;
            memset(word,0,100);
            if (cstr[i] == ')')
                --brkt;
        }
        else if (cstr[i] == '(')
        {
            ++brkt;
        }
        else if (cstr[i] == ')')
        {
            --brkt;
            ++closes;
        }
        else word[wordindex++] = cstr[i];
    }

    if (brkt != 0) result.clear();

    for (auto &e: result)
    {
        wordindex = closes = 0;
        memset(word,0,100);
        auto cstr = e.name.c_str();
        e.val = "";e.cond = 0;
        for (size_t i=0; i < e.name.length(); i++)
        {
            if (cstr[i]=='<'||cstr[i]=='>'||
                cstr[i]=='='||cstr[i]=='~' || cstr[i]=='!')
            {
                e.cond = cstr[i];
                e.val = std::string(&cstr[i+1]);
                e.name = std::string(cstr,&cstr[i]);
                break;
            }
        }
    }
}

std::string deserialize(const std::string & source_str)
{
    std::string val = source_str;
    strSearchReplace(val,"&eq;","=");       
    strSearchReplace(val,"&cm;",",");       
    return val;
}

std::string serialize(const std::string & source_str)
{
    std::string val = source_str;
    strSearchReplace(val,"=","&eq;");       
    strSearchReplace(val,",","&cm;");       
    return val;
}

bool checkExpr(std::vector<expr> &result)
{
    auto cursize = result.size();
    while (cursize > 1)
    {
        for (size_t i = 0; i < result.size() - 1; i++ )
        {
            auto &cur = result[i];
            auto &nxt = result[i+1];
            if (cur.brkt == nxt.brkt)
            {
                if      (cur.oper == '&')   nxt.result = (cur.result > 0.5) && (nxt.result > 0.5);
                else if (cur.oper == '|')   nxt.result = (cur.result > 0.5) || (nxt.result > 0.5);
                else                        nxt.result = 0;
                nxt.brkt--;
                result.erase(result.begin() + i);
                break;
            }
        }
        if (cursize == result.size()) return 0;
        cursize = result.size();
    }
    return (result[0].result > 0.5);
}

double computeExpr(std::vector<expr> &result)
{
    auto cursize = result.size();
    if (cursize == 0) return 0;
    while (cursize > 1)
    {
        for (size_t i = 0; i < result.size() - 1; i++ )
        {
            auto &cur = result[i];
            auto &nxt = result[i+1];
            if (cur.brkt == nxt.brkt)
            {
                if      (cur.oper == '+')   nxt.result = cur.result + nxt.result;
                else if (cur.oper == '-')   nxt.result = cur.result - nxt.result;
                else if (cur.oper == '*')   nxt.result = cur.result * nxt.result;
                else if (cur.oper == '/')   nxt.result = cur.result / nxt.result;
                else                    nxt.result = 0;
                nxt.brkt--;
                result.erase(result.begin() + i);
                break;
            }
        }
        if (cursize == result.size()) return 0;
        cursize = result.size();
    }
    return result[0].result;
}

void strSearchReplace(std::string & source_str, const std::string &search, const std::string &replace)
{
    for( size_t pos = 0; ; pos += replace.length() ) 
    {
        pos =  source_str.find( search, pos );
        if( pos == std::string::npos ) break;

        source_str.erase( pos, search.length() );
        source_str.insert( pos, replace );
    }
}

void tokenize(const std::string &input, const char delimiter, std::vector<std::string> &result)
{
    const char * str = input.c_str();
    result.clear();
    
    do
    {
        const char *begin = str;
        while(*str != delimiter && *str) str++;
        result.push_back(std::string(begin, str));
    }
    while (0 != *str++);
}

int32_t sendEmail(const std::string& body, const std::string& subject, const std::string& from, const std::string& to,
                  const std::string& smtp_server_address, const std::string& smtp_user_id, 
                  const std::string& smtp_password, const std::string& smtp_options)
{
    std::string command("echo \"");
    command.append(body);
    command.append("\" | mailx -r \"");
    command.append(from);
    command.append("\"");
    command.append(" -s \"");
    command.append(subject);
    command.append("\" -S smtp=\"");
    command.append(smtp_server_address);
    command.append("\" ");
    command.append(smtp_options);
    command.append(" -S smtp-auth-user=\"");
    command.append(smtp_user_id);
    command.append("\" -S smtp-auth-password=\"");
    command.append(smtp_password);
    command.append("\" ");
    command.append(to);

    LOG_DEBUG << "Email command: \n" << command << LOG_END;
    system(command.c_str());

    return 0;
}


double stdnormal_cdf(double u)
{
    // at least on unix this apears to be exactly the same as Cody, below.
    // return erfc(-u/sqrt(2))/2;
    static double SQRT2 = 0.707106781186547524401;
    return 0.5 * erfc(-SQRT2*u);
}

double stdnormal_inv(double p)
{
    // Ackman without refinement
    // speedy, but broken?
    const double a[6] = {
        -3.969683028665376e+01,  2.209460984245205e+02,
        -2.759285104469687e+02,  1.383577518672690e+02,
        -3.066479806614716e+01,  2.506628277459239e+00
    };
    const double b[5] = {
        -5.447609879822406e+01,  1.615858368580409e+02,
        -1.556989798598866e+02,  6.680131188771972e+01,
        -1.328068155288572e+01
    };
    const double c[6] = {
        -7.784894002430293e-03, -3.223964580411365e-01,
        -2.400758277161838e+00, -2.549732539343734e+00,
        4.374664141464968e+00,  2.938163982698783e+00
    };
    const double d[4] = {
        7.784695709041462e-03,  3.224671290700398e-01,
        2.445134137142996e+00,  3.754408661907416e+00
    };
    
    double z, R;
    double plow = 0.02425;
    double phigh = 1 - plow;
    
    // assume no invalid entries...
    if (p == 0.0)
    return -DBL_MAX;
    if (p == 1.0)
    return  DBL_MAX;
    
    if( p < plow )
    {
        z = sqrt(-2.0 * log( p ) );
        z = (((((c[0] * z + c[1]) * z + c[2]) * z + c[3]) * z + c[4]) * z + c[5]) / ((((d[0] * z + d[1]) * z + d[2]) * z + d[3]) * z + 1.0);
    }
    else if( p > phigh )
    {
        z = sqrt(-2.0 * log( 1.0 - p ) );
        z = -(((((c[0] * z + c[1]) * z + c[2]) * z + c[3]) * z + c[4]) * z + c[5]) / ((((d[0] * z + d[1]) * z + d[2]) * z + d[3]) * z + 1.0);
    }
    else
    {
        z = p - 0.5;
        R = z * z;
        z *= (((((a[0] * R + a[1]) * R + a[2]) * R + a[3]) * R + a[4]) * R + a[5]) / (((((b[0] * R + b[1]) * R + b[2]) * R + b[3]) * R + b[4]) * R + 1.0);
    };
    
    return z;
}

int32_t nextDay(int input)
{
    static std::vector<int32_t> daysOfMonth = {  31,28,31,30,31,30,31,31,30,31,30,31 };
    int32_t inyy = input/10000, inmm = (input%10000)/100, indd = 1+(input%100);
    bool leapstatus = (inyy % 4 == 0 && (inyy % 100 > 0 || inyy % 400 == 0));
    int32_t monthmax = (leapstatus && inmm == 2 )? daysOfMonth[inmm-1]+1:daysOfMonth[inmm-1];
    
    if (indd > monthmax)
    {
        indd = 1;
        inmm++;
    }
    
    if (inmm == 13)
    {
        inyy++;
        inmm = 1;
    }
    
    return inyy*10000+ inmm * 100 + indd;
}

int32_t dateToDays(int32_t input)
{
    static std::vector<int32_t> daysOfMonth = {  31,28,31,30,31,30,31,31,30,31,30,31 };
    int32_t inyy = input/10000, inmm = (input%10000)/100, indd = input%100;
    bool leapstatus = (inyy % 4 == 0 && (inyy % 100 > 0 || inyy % 400 == 0));
    inyy--;
    int32_t leapyears = (inyy/4) - (inyy/100) + (inyy/400);
    int32_t days = (leapyears * 366) + (inyy-leapyears)*365;
    for (int i=1;i < inmm; i++)
        days += ((leapstatus && i == 2 )? daysOfMonth[i-1]+1:daysOfMonth[i-1]);
    days += indd;
    return days;
}
