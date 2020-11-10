#pragma once

#include <algorithm>
//#include <boost/filesystem.hpp>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

/* Enable Debug prints */
//#define UCX_DEBUG_ENABLE

#if defined(UCX_DEBUG_ENABLE)
#define DEBUG_PRINT printf
#else
#define DEBUG_PRINT
#endif

std::string
to_lower (const std::string& input_string);

std::vector<std::string>
split (const std::string& s, char delimiter);

std::tuple<std::string, unsigned int>
parse_metric (const std::string& metric, unsigned long long int *hex_dummy);

std::size_t
read_buffer_size ();

std::size_t
to_mb (std::size_t nbytes);

size_t
convert_thread_id (std::thread::id tid);
