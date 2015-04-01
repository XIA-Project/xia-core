#include "utils.hpp"
#include <stdio.h>

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::string trim(const std::string& str)
{
	const std::string& whitespace = " \t";
    const size_t strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos)
        return ""; // no content

    const size_t strEnd = str.find_last_not_of(whitespace);
    const size_t strRange = strEnd - strBegin + 1;

    return str.substr(strBegin, strRange);
}

int stoi(const std::string& str, int pos, int base) {
    (void)(pos);    // mark this unused

    int output = -1;

    switch (base) {
    case 10:
        sscanf(str.c_str(), "%d",&output);
        break;
    case 16:
        sscanf(str.c_str(), "%x",&output);
        break;
    default:
        sscanf(str.c_str(), "%d",&output);
        break;
    }

    return output;
}
