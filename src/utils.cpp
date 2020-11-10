#include <cassert>
#include <cstdlib>
#include <iostream>

#include <utils.h>

std::string
to_lower(const std::string& input_string)
{
    std::string output;
    std::transform(input_string.begin(),
    		input_string.end (),
			std::back_inserter(output),
            [](unsigned char c){ return std::tolower (c); }
    );
    return output;
}


std::vector<std::string>
split(const std::string& s, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back (token);
    }
    return tokens;
}


std::tuple<std::string, unsigned int>
parse_metric (const std::string& metric, unsigned long long int *hex_dummy)
{
    auto substrings = split (metric, '@');
    if (substrings.size () != 2) {
        throw std::invalid_argument("Invalid metric given");
    }

    *hex_dummy = strtoull(substrings[1].c_str(), NULL, 16);
    return { substrings[0], std::stoull(substrings[1]) };
}
