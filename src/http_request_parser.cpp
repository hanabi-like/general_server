#include "http_request_parser.h"

#include <cstdlib>
#include <cstring>
#include <strings.h>

HttpRequestParser::HttpRequestParser()
{
    reset();
}

void HttpRequestParser::reset()
{
    g_method = GET;
    g_url = 0;
    g_query = 0;
    g_version = 0;

    g_host = 0;
    g_linger = false;
    g_contentType = 0;
    g_contentLength = 0;

    g_content = 0;

    g_checkStatus = CHECK_STATE_REQUEST_LINE;

    std::memset(g_readBuf, '\0', READ_BUFFER_SIZE);
    g_readIdx = 0;
    g_startIdx = 0;
    g_checkedIdx = 0;
}

char *HttpRequestParser::buffer()
{
    return g_readBuf;
}

int HttpRequestParser::readIndex() const
{
    return g_readIdx;
}

void HttpRequestParser::increaseReadIndex(int bytes)
{
    g_readIdx += bytes;
}

HttpRequestParser::ParseResult HttpRequestParser::process()
{
    LineStatus lineStatus = LINE_OK;
    ParseResult parseResult = NO_REQUEST;
    char *text = 0;

    while ((g_checkStatus == CHECK_STATE_REQUEST_BODY && lineStatus == LINE_OK) || ((lineStatus = parseLine()) == LINE_OK))
    {
        text = g_readBuf + g_startIdx;
        g_startIdx = g_checkedIdx;
        switch (g_checkStatus)
        {
        case CHECK_STATE_REQUEST_LINE:
        {
            parseResult = parseRequestLine(text);
            if (parseResult == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_REQUEST_HEADERS:
        {
            parseResult = parseRequestHeaders(text);
            if (parseResult == BAD_REQUEST)
                return BAD_REQUEST;
            else if (parseResult == REQUEST_READY)
                return REQUEST_READY;
            break;
        }
        case CHECK_STATE_REQUEST_BODY:
        {
            parseResult = parseRequestBody(text);
            if (parseResult == REQUEST_READY)
                return REQUEST_READY;
            lineStatus = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

HttpRequestParser::LineStatus HttpRequestParser::parseLine()
{
    char temp;
    for (; g_checkedIdx < g_readIdx; ++g_checkedIdx)
    {
        temp = g_readBuf[g_checkedIdx];
        if (temp == '\r')
        {
            if ((g_checkedIdx + 1) == g_readIdx)
                return LINE_OPEN;
            else if (g_readBuf[g_checkedIdx + 1] == '\n')
            {
                g_readBuf[g_checkedIdx++] = '\0';
                g_readBuf[g_checkedIdx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            if (g_checkedIdx > 1 && g_readBuf[g_checkedIdx - 1] ==
                                        '\r')
            {
                g_readBuf[g_checkedIdx - 1] = '\0';
                g_readBuf[g_checkedIdx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

HttpRequestParser::ParseResult
HttpRequestParser::parseRequestLine(char *text)
{
    g_url = strpbrk(text, " \t");
    if (!g_url)
        return BAD_REQUEST;
    *g_url++ = '\0';

    char *method = text;
    if (strcasecmp(method, "GET") == 0)
        g_method = GET;
    else if (strcasecmp(method, "POST") == 0)
        g_method = POST;
    else
        return BAD_REQUEST;

    g_url += strspn(g_url, " \t");
    g_version = strpbrk(g_url, " \t");
    if (!g_version)
        return BAD_REQUEST;
    *g_version++ = '\0';
    g_version += strspn(g_version, " \t");

    if (strcasecmp(g_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;

    if (strncasecmp(g_url, "http://", 7) == 0)
    {
        g_url += 7;
        g_url = strchr(g_url, '/');
    }

    if (!g_url || g_url[0] != '/')
        return BAD_REQUEST;

    g_query = std::strchr(g_url, '?');
    if (g_query != nullptr)
    {
        *g_query++ = '\0';
    }

    if (std::strlen(g_url) == 1)
        std::strcat(g_url, "home.html");

    g_checkStatus = CHECK_STATE_REQUEST_HEADERS;
    return NO_REQUEST;
}

HttpRequestParser::ParseResult HttpRequestParser::parseRequestHeaders(char
                                                                          *text)
{
    if (text[0] == '\0')
    {
        if (g_contentLength != 0)
        {
            g_checkStatus = CHECK_STATE_REQUEST_BODY;
            return NO_REQUEST;
        }
        return REQUEST_READY;
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        g_host = text;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
            g_linger = true;
    }
    else if (strncasecmp(text, "Content-Type:", 13) == 0)
    {
        text += 13;
        text += strspn(text, " \t");
        g_contentType = text;
    }
    else if (strncasecmp(text, "Content-Length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        g_contentLength = atol(text);
    }

    return NO_REQUEST;
}

HttpRequestParser::ParseResult HttpRequestParser::parseRequestBody(char
                                                                       *text)
{
    if ((g_contentLength + g_checkedIdx) <= g_readIdx)
    {
        text[g_contentLength] = '\0';
        g_content = text;
        return REQUEST_READY;
    }
    return NO_REQUEST;
}

HttpRequestParser::Method HttpRequestParser::method() const
{
    return g_method;
}

char *HttpRequestParser::url() const
{
    return g_url;
}

char *HttpRequestParser::query() const
{
    return g_query;
}

char *HttpRequestParser::version() const
{
    return g_version;
}

char *HttpRequestParser::host() const
{
    return g_host;
}

bool HttpRequestParser::keepAlive() const
{
    return g_linger;
}

char *HttpRequestParser::contentType() const
{
    return g_contentType;
}

int HttpRequestParser::contentLength() const
{
    return g_contentLength;
}

char *HttpRequestParser::content() const
{
    return g_content;
}
