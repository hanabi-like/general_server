#include "http_request_dispatcher.h"

const char *HttpRequestDispatcher::resolve(const char *url)
{
    return g_pageRequestHandler.resolve(url);
}
