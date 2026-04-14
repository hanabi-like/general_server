#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

class HttpResponse
{
public:
    static const int WRITE_BUFFER_SIZE = 1024;

    HttpResponse();

    void init();

    bool buildOkHeader(int contentLength, bool linger);
    bool buildBadRequest(bool linger);
    bool buildForbidden(bool linger);
    bool buildNotFound(bool linger);
    bool buildInternalError(bool linger);

    char *buffer();
    int bufferSize() const;

private:
    bool buildErrorResponse(int status, const char *title, const char *content, bool linger);
    bool addResponse(const char *format, ...);
    bool addStatusLine(int status, const char *title);
    bool addHeaders(int contentLength, bool linger);
    bool addContentLength(int contentLength);
    bool addLinger(bool linger);
    bool addBlankLine();
    bool addContent(const char *content);

private:
    char g_writeBuf[WRITE_BUFFER_SIZE];
    int g_writeIdx;
};

#endif
