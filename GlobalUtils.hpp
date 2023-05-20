#ifndef _GLOBAL_UTILS_HPP_
#define _GLOBAL_UTILS_HPP_

#include <inttypes.h>
#include <string>
#include <cstring>
#include <vector>
#include <sstream>
#include <iomanip>
#include <math.h>
#include <sys/time.h>
#include "jsonxx.hpp"

typedef struct expression
{
  char brkt;	
  char oper; // (&|), (+-*/)

  char cond; // (><=~!)
  std::string name;
  std::string val; //used for check not compute

  friend std::ostream& operator << (std::ostream& out, const struct expression &e)
  {
      out << "Name=" << e.name << ",Cond=" << e.cond << 
          ",Eval=" << e.val << ",Oper=" << e.oper;
      return out;
  }
  //output
  double result;
} expr;

//********** functions to parse and evaluate expressions ************
//Turns ((b+(c/d))*a) ==> 2b+,3c/,3d*,1a
void parseExpr(const std::string exprstr, bool bCompareNotCompute, std::vector<expr> &result);
bool checkExpr(std::vector<expr> &result);
double computeExpr(std::vector<expr> &result);
//********** functions to parse and evaluate expressions ************

template <typename T>
T StringToNumber(const std::string &Text, T defValue = T())
{
    std::stringstream ss;
    for ( std::string::const_iterator i=Text.begin(); i!=Text.end(); ++i )
        if ( isdigit(*i) || *i=='e' || *i=='-' || *i=='+' || *i=='.' )
            ss << *i;
    T result;
    return ss >> result ? result : defValue;
}

std::string NumberToString(double d, int precision = 2);
bool isNumber(const std::string& s);

//type u=unsigned, i=signed, d=double, s=string
std::string jsonfield(jsonxx::Object &req, const std::string &name, const char type = 's', const int32_t index = 0);

//source_str is the target string that will be modified. 
void strSearchReplace(std::string & source_str, const std::string &search, const std::string &replace);

std::string serialize(const std::string & source_str);
std::string deserialize(const std::string & source_str);

void tokenize(const std::string &input, const char delimiter, std::vector<std::string> &result);

int32_t sendEmail(const std::string& body, const std::string& subject, const std::string& from, const std::string& to,
                  const std::string& smtp_server_address, const std::string& smtp_user_id, 
                  const std::string& smtp_password, const std::string& smtp_options);

//Returns prob (0.0-1.0) for z value
double stdnormal_cdf(double u);

//Returns z value for prob (0.0-1.0)
double stdnormal_inv(double p);

//Returns next calendar day for an given input date
//Both input and output are in yyyymmdd format
int32_t nextDay(int input);

//Returns date to days which can be 
//used for date math
int32_t dateToDays(int32_t input);

#endif
