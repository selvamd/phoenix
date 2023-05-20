/**
 * \brief Logging functions
 * 
 *
 * \author Dave Demeter
 * \copyright CPOD 2015
 */

#include "Logger.hpp"
#include <iostream>

int32_t global_log_level = log_level_error;
bool    global_log_initialized = false;
std::ostream& global_log_output = std::cout;

int32_t setLogLevel(std::string& level)
{
    int32_t rc = 0;
    std::string lower_case_level = level;

    //Convert the input to lower case
    for(auto &elem : lower_case_level)
    {
        elem = std::tolower(elem);
    }

    if (lower_case_level == "debug")
    {
        global_log_level = log_level_debug;
    }
    else if (lower_case_level == "error")
    {
        global_log_level = log_level_error;
    }
    else if (lower_case_level == "none")
    {
        global_log_level = log_level_none;
    }
    else
    {
        LOG_ERROR << "Invalid log level setting: " << level << LOG_END;
        rc = -1;
    }

    return rc;
}

