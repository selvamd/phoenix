#pragma once

#include <string>
#include <memory>
#include <sstream>
#include <iostream>
#include <vector>
#include <cassert>
#include "GlobalUtils.hpp"
#include "DataTypes.hpp"

//Simple Json parser
//Supports heirarchy of objects
//object/array nodes can hold child nodes which 
//themselves can be objects/arrays/elements
//ele nodes hold single value or array of single values
class Json 
{
    public:
      typedef Json JsonNode;
      typedef std::vector<JsonNode> JsonContainer;

      static Json& root(JsonContainer &nodes) 
      { 
          std::string temp = "";
          return root(nodes,temp); 
      }

      static Json& root(JsonContainer &nodes, std::string &input)
      {
          nodes.reserve(1000);
          char s[16388];
          int count = 0, len = 0;
          //strips the whitespaces before parsing
          for (size_t i=0; i<input.length(); ++i)
          {
              char c = input.at(i); 
              if (c != ' ') s[len++] = c;
              //whitespaces inside data is ok
              if ( c == ' ' && count % 2 == 1)
                  s[len++] = c;
              if (c == '\"') count++;
          }

          if (s[0]!= '{' || s[len-1] != '}')
            assert("invalid json");

          s[len] = 0;
          count = 0;
          nodes.clear();
          parseObject(s, len, count, nodes);
          return nodes[0];
      }

      Json& obj(std::string n = "")
      {
          auto &nodes = *m_nodes;
          nodes.push_back(Json('3',n,"",nodes));
          return nodes.back();
      }

      Json& arr(std::string n = "")
      {
          auto &nodes = *m_nodes;
          nodes.push_back(Json('2',n,"",nodes));
          return nodes.back();
      }

      Json& ele(std::string v) { return ele("",v); }

      Json& ele(std::string n, std::string v)
      {
          auto &nodes = *m_nodes;
          nodes.push_back(Json('1',n,v,nodes));
          return nodes.back();
      }

      //level,type(1-ele,2-arr,3-obj),name,value
      bool isobj()  { return type == '3'; }
      bool isarr()  { return type == '2'; }
      bool isele()  { return type == '1'; }


      std::string & nodename() { return this->name; };
      std::string & nodevalue() { return this->value; };
  
      size_t size()
      {
          size_t n = 0;
          auto temp = child;

          while (temp > 0)
          {
              auto &js = m_nodes->at(temp);
              temp = js.next;
              n++;
          }
          return n;  
      }

      bool has(std::string s)
      {
          auto temp = child;
          while (temp > 0)
          {
              auto &js = m_nodes->at(temp);
              if (js.name == s) return true;
              temp = js.next;
          }
          return false;
      }

      std::string gets(std::string s) { return get(s).value; }
      std::string gets(size_t i) { return get(i).value; }
      double getd(std::string s) { return getn<double>(s); }
      double getd(size_t i) { return getn<double>(i); }
      int geti(std::string s) { return getn<int>(s); }
      int geti(size_t i) { return getn<int>(i); }

      template <typename T>
      EnumData<T> gete(std::string s)
      {
          EnumData<T> atype;
          atype = gets(s);
          return atype;
      }

      template <typename T>
      EnumData<T> gete(int i)
      {
          EnumData<T> atype;
          atype = gets(i);
          return atype;
      }


      template <typename T>
      T getn(size_t i) { return StringToNumber<T>(get(i).value); }

      template <typename T>
      T getn(std::string s) { return StringToNumber<T>(get(s).value); }


      Json & get(std::string s)
      {
          auto temp = child;
          while (temp > 0)
          {
              auto &js = m_nodes->at(temp);
              if (js.name == s) return js;
              temp = js.next;
          }
          return *this; //error if reference to itself is returned
      }

      Json & get(size_t i)
      {
          size_t n = 0;
          auto temp = child;
          while (temp > 0)
          {
              auto &js = m_nodes->at(temp);
              if (n == i) return js;
              temp = js.next;
              n++;
          }
          return *this; //error if reference to itself is returned
      }

      //reserves size for large element node
      void reserve(int size)
      {
          if (isele()) this->value.reserve(size);
      }

      //element nodes also support array of singular values
      Json & operator<<(const std::string data)
      {
          if (!isele()) return *this;
          if (this->value.length() > 0)
              this->value.append("|");
          this->value.append(data);
          return *this;
      }

      Json & operator<<(Json & newchild)
      {
          Json &parent = m_nodes->at(this->index);
          parent.addChild(newchild);
          return *this;
      }

      void addChild(Json & newchild)
      {
          //if (index > m_nodes->size()) return *this;
          //std::cout << this << std::endl;
          
          //arr - have same type of children and name is ignored
          //obj - must have unique name and any type of children
          //ele - child is always null
          if (isele()) return;
          if (isobj() && newchild.name.length() <= 0) return;
          if (isarr() && newchild.name.length() > 0)  return;
          if (child == 0)
          {
              child = newchild.index;
              //std::cout << "setting " << *this << " child with " << newchild << std::endl;
              return;
          }

          auto temp = child;
          while (temp > 0)
          {
              //Insert the new child at the end. Error if arr has different child types
              auto &js = m_nodes->at(temp);
              if (isarr() && js.type != newchild.type)
                  return;

              //Replace if name already exists, if not insert at the end
              if (isobj() && js.name == newchild.name)
              {
                  js.value = newchild.value;   
                  return;
              }

              if (js.next == 0) break;
              temp = js.next;
          }
          auto &js = m_nodes->at(temp);
          js.next = newchild.index;
          //std::cout << "setting " << *temp << " next with " << newchild << std::endl;
      }

      std::string json()
      {

          if (isele() && name.length() > 0) return "\"" + name + "\":\"" + value + "\"";
          if (isele() && name.length() <= 0) return "\"" + value + "\"";
          if (isarr())
          {
              std::string result = "\"" + name + "\":[";
              auto temp = child;
              while (temp > 0)
              {
                  if (temp != child)
                      result.append(",");
                  auto &js = m_nodes->at(temp); 
                  result.append(js.json());
                  temp = js.next;
              }
              result.append("]");
              return result;
          }
          if (isobj())
          {
              //If request originated from cloned json object
              //not in the JsonContainer, refer to the original  
              //object copy in the container for most upto date values
              if (index == 0 && m_nodes->size() > 0)
                 if (this != &m_nodes->at(0)) 
                    return m_nodes->at(0).json();
                  
              std::string result = "\"" + name + "\":{";
              if (name.length() == 0) result = "{";
              auto temp = child;
              while (temp > 0)
              {
                  if (temp != child)
                      result.append(",");
                  auto &js = m_nodes->at(temp);
                  result.append(js.json());
                  temp = js.next;
              }
              result.append("}");
              return result;
          }
          return "";
      }

      virtual ~Json() 
      {
          //std::cout << *this << " destroyed " << std::endl;
      }

      friend std::ostream & operator << (std::ostream & out, const Json & data)
      {
          out << "Addr:" << data.index << ",";
          out << "name:" << data.name << ",";
          out << "value:" << data.value << ",";
          out << "type:" << data.type << ",";
          out << "child:" << data.child << ",";
          out << "next:" << data.next << ",";
          return out;
      }    

    private:

      //Always expected to call such that s[from] = '\"' 
      static std::string parseTag(const char * s, int &from)
      {
          int cidx = 0;char c[256];
          while (s[++from] != '\"' && cidx < 256)
            c[cidx++] = s[from];
          c[cidx] = 0;
          from++;
          return c;
      }

      static Json& parseObject(const char * s, const int strlen, int &from, JsonContainer &nodes)
      {
          //JsonNode ptr(new Json('3', "", "", nodes));
          nodes.push_back(Json('3', "", "", nodes));
          auto &rec = nodes[nodes.size()-1];
          from++;
          while (from < strlen)
          {
              //std::cout << rec << std::endl;
              std::string n = parseTag(s,from);
              from += 1; //skip ':' 

              if (s[from] == '\"')
              {
                  std::string v = parseTag(s,from);
                  //std::cout << n << "," << v << "," << &rec << std::endl;
                  rec.addChild(rec.ele(n,v));
              }    
              else if (s[from] == '{')
              {
                  auto& child = parseObject(s, strlen, from, nodes);  
                  child.name = n;
                  rec.addChild(child);
              }
              else if (s[from] == '[')
              {
                  auto &child = parseArray(s, strlen, from, nodes);
                  child.name = n;
                  rec.addChild(child);
              }
              
              if (s[from++] == '}')
                  break;
          }
          return rec;
      }

      static Json& parseArray(const char * s, const int strlen, int &from, JsonContainer &nodes)
      {
          nodes.push_back(Json('2', "", "", nodes));
          auto &rec = nodes.back();
          from++;
          while (from < strlen)
          {
              if (s[from] == '\"')
                  rec << rec.ele(parseTag(s,from));
              else if (s[from] == '{')
                  rec << parseObject(s, strlen, from, nodes);
              else if (s[from] == '[')
                  rec << parseArray(s, strlen, from, nodes);
              if (s[from++] == ']')
                  break;
          }
          return rec;
      }

      Json(char t, const std::string &n, std::string v, JsonContainer &nodes) : 
              type(t), name(n), value(v), child(0), next(0), m_nodes(&nodes), index(nodes.size()) {  }

      char type; //3-Object,2-Array,1-Element
      std::string name;
      std::string value;
      size_t child;
      size_t next;
      JsonContainer *m_nodes;
      size_t index;

      //Json& operator= (const Json &) = default; //copy assign
      //Json(const Json &) = default;

};
