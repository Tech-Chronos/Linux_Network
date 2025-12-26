#include <iostream>
#include <string>
#include <regex>

int main()
{
    // GET /index.html?user=zhangsan&passwd=123456 HTTP/1.1\r\n;
    std::string req_header = "GET /index.html/a/b/c?user=zhangsan&passwd=123456 HTTP/1.1\r\n";

    // GET /login.html HTTP/1.0\r\n
    //std::string req_header = "GET /login.html HTTP/1.0\r\n";
    std::smatch matches;

    std::regex pattern("(GET|POST|DELETE|PUT|OPTION|PATCHY|HEAD)\\s([^\\?]+)(?:[\\?])?([^\\s]+)?\\s(HTTP/1\\.[01])(?:\n|\r\n)");
    bool ret = std::regex_match(req_header, matches, pattern);

    if (ret == false)
    {
        std::cout << "regex_match false" << std::endl;
        return -1;
    }

    for (auto& match : matches)
    {
        std::cout << match << std::endl;
    }

    return 0;
}

//std::regex pattern("(GET|POST|PUT|DELETE|HEAD) ([^\\?]+)(?:\\?)?([^\\&]+)?(?:\\&)?([^\\s]+)? (HTTP/1\\.[01])(?:\n|\r\n)");
