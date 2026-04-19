#ifndef HTTP_REQUEST_DISPATCHER_H
#define HTTP_REQUEST_DISPATCHER_H

#include <string>

#include "page_request_handler.h"

class HttpRequestDispatcher
{
public:
    const char *resolve(const char *url);

private:
    PageRequestHandler g_pageRequestHandler;
};

#endif
