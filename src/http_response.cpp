#include "http_response.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace
{
    const char *OK_200_TITLE = "OK";

    const char *ERROR_400_TITLE = "Bad Request";
    const char *ERROR_400_FORM = "Bad Request\n";

    const char *ERROR_403_TITLE = "Forbidden";
    const char *ERROR_403_FORM = "Forbidden\n";

    const char *ERROR_404_TITLE = "Not Found";
    const char *ERROR_404_FORM = "Not Found\n";

    const char *ERROR_500_TITLE = "Internal Error";
    const char *ERROR_500_FORM = "Internal Error\n";

    const char *ERROR_502_TITLE = "Bad Gateway";
    const char *ERROR_502_FORM = "Bad Gateway\n";
}

HttpResponse::HttpResponse()
{
    init();
}

void HttpResponse::init()
{
    std::memset(g_writeBuf, '\0', WRITE_BUFFER_SIZE);
    g_writeIdx = 0;
}

bool HttpResponse::buildOkHeader(int contentLength, bool linger)
{
    return addStatusLine(200, OK_200_TITLE) && addHeaders(contentLength, linger);
}

bool HttpResponse::buildBadRequest(bool linger)
{
    return buildErrorResponse(400, ERROR_400_TITLE, ERROR_400_FORM, linger);
}

bool HttpResponse::buildForbidden(bool linger)
{
    return buildErrorResponse(403, ERROR_403_TITLE, ERROR_403_FORM, linger);
}

bool HttpResponse::buildNotFound(bool linger)
{
    return buildErrorResponse(404, ERROR_404_TITLE, ERROR_404_FORM, linger);
}

bool HttpResponse::buildInternalError(bool linger)
{
    return buildErrorResponse(500, ERROR_500_TITLE, ERROR_500_FORM, linger);
}

bool HttpResponse::buildBadGateway(bool linger)
{
    return buildErrorResponse(502, ERROR_502_TITLE, ERROR_502_FORM, linger);
}

char *HttpResponse::buffer()
{
    return g_writeBuf;
}

int HttpResponse::bufferSize() const
{
    return g_writeIdx;
}

bool HttpResponse::buildErrorResponse(int status, const char *title, const char *content, bool linger)
{
    return addStatusLine(status, title) && addHeaders(strlen(content), linger) && addContent(content);
}

bool HttpResponse::addResponse(const char *format, ...)
{
    if (g_writeIdx >= WRITE_BUFFER_SIZE)
        return false;
    va_list argList;
    va_start(argList, format);
    int len = std::vsnprintf(g_writeBuf + g_writeIdx, WRITE_BUFFER_SIZE - 1 - g_writeIdx, format, argList);
    va_end(argList);

    if (len < 0 || len >= WRITE_BUFFER_SIZE - 1 - g_writeIdx)
        return false;
    g_writeIdx += len;
    return true;
}

bool HttpResponse::addStatusLine(int status, const char *title)
{
    return addResponse("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool HttpResponse::addHeaders(int contentLength, bool linger)
{
    return addContentLength(contentLength) && addLinger(linger) && addBlankLine();
}

bool HttpResponse::addContentLength(int contentLength)
{
    return addResponse("Content-Length: %d\r\n", contentLength);
}

bool HttpResponse::addLinger(bool linger)
{
    return addResponse("Connection: %s\r\n", linger ? "keep-alive" : "close");
}

bool HttpResponse::addBlankLine()
{
    return addResponse("%s", "\r\n");
}

bool HttpResponse::addContent(const char *content)
{
    return addResponse("%s", content);
}
