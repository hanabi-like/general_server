#ifndef HTTP_REQUEST_PARSER_H
#define HTTP_REQUEST_PARSER_H

class HttpRequestParser
{
public:
    static const int READ_BUFFER_SIZE = 2048;

    enum CheckStatus
    {
        CHECK_STATE_REQUEST_LINE = 0,
        CHECK_STATE_REQUEST_HEADERS,
        CHECK_STATE_REQUEST_BODY
    };

    enum LineStatus
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

    enum ParseResult
    {
        NO_REQUEST = 0,
        REQUEST_READY,
        BAD_REQUEST,
        INTERNAL_ERROR
    };

    enum Method
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATCH
    };

public:
    HttpRequestParser();

    void reset();

    char *buffer();
    int readIndex() const;
    void increaseReadIndex(int bytes);

    ParseResult process();

    Method method() const;
    char *url() const;
    char *query() const;
    char *version() const;
    char *host() const;
    char *xUserName() const;
    bool keepAlive() const;
    char *contentType() const;
    int contentLength() const;
    char *content() const;

private:
    LineStatus parseLine();
    ParseResult parseRequestLine(char *text);
    ParseResult parseRequestHeaders(char *text);
    ParseResult parseRequestBody(char *text);

private:
    // request line
    Method g_method;
    char *g_url;
    char *g_query;
    char *g_version;
    // request headers
    char *g_host;
    char *g_xUserName;
    bool g_linger;
    char *g_contentType;
    int g_contentLength;
    // request body
    char *g_content;

    CheckStatus g_checkStatus;

    char g_readBuf[READ_BUFFER_SIZE];
    int g_readIdx;
    int g_startIdx;
    int g_checkedIdx;
};

#endif
