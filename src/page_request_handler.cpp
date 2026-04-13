#include "page_request_handler.h"

#include <cstring>

const char *PageRequestHandler::resolve(const char *url) const
{
    const char *p = std::strrchr(url, '/');
    if (!p || *(p + 1) == '\0')
        return url;

    if (*(p + 1) == '0')
        return "/reg.html";

    if (*(p + 1) == '1')
        return "/login.html";

    return url;
}