#ifndef _LOGGER_HPP_
#define _LOGGER_HPP_
/**
 * \brief Logging functions
 * 
 *
 * \author Dave Demeter
 * \copyright CPOD 2015
 */

#include <stdint.h>
#include <string>
#include <sys/time.h>
#include <iostream>

static const int32_t log_level_none  = 0;
static const int32_t log_level_error = 5;
static const int32_t log_level_debug = 10;

extern int32_t global_log_level;
//For now this is just a reference to std::cout
//In the future it will be a reference to a log file
extern std::ostream& global_log_output;

inline void formatLogTimestamp(char* buffer)
{
    buffer[0] = 0;
    char format[64];
    struct timeval tv;
    struct tm*     tm;
    gettimeofday(&tv, NULL);
    if ((tm = localtime(&tv.tv_sec)) != nullptr)
    {
        strftime(format, sizeof(format), "%Y-%m-%d %H:%M:%S.%%06u", tm);
        snprintf(buffer, sizeof(format), format, tv.tv_usec);
    }
}


#ifdef LOG_PREFIX
#undef LOG_PREFIX
#endif

#define LOG_PREFIX __FILE__ << ":" << __func__ << ":" << __LINE__  << ": "

#define LOG_ERROR \
if (global_log_level >= log_level_error) \
{ \
    char timestamp[64]; \
    formatLogTimestamp(timestamp); \
    global_log_output << timestamp << ":ERROR: " << LOG_PREFIX


#define LOG_DEBUG \
if (global_log_level >= log_level_debug) \
{ \
    char timestamp[64]; \
    formatLogTimestamp(timestamp); \
    global_log_output << timestamp << ":DEBUG: " << LOG_PREFIX


#define LOG_ALWAYS \
{ \
    char timestamp[64]; \
    formatLogTimestamp(timestamp); \
    global_log_output << timestamp << ": LOG : " << LOG_PREFIX


#define LOG_END \
    std::endl; \
}

int32_t setLogLevel(std::string& level);

#endif

