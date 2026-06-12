#include "../hpp/Common.hpp"


using namespace std;

void trim (string& line) {

    size_t start = 0;
    while (start < line.size() && isspace(line[start])) start++;
    size_t end = line.size();

    while(end > start && isspace(line[end - 1])) end--;
    line = line.substr(start, end - start);
}