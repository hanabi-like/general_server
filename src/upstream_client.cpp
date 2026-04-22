#include "upstream_client.h"

#include <arpa/inet.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace
{
    const int BUFFER_SIZE = 4096;
    const int TIMEOUT = 5;

    bool sendRequest(int sockFd, const char *data, size_t size)
    {
        size_t byte = 0;
        while (byte < size)
        {
            ssize_t ret = send(sockFd, data + byte, size - byte, 0);
            if (ret <= 0)
                return false;
            byte += static_cast<size_t>(ret);
        }
        return true;
    }

    bool setTimeout(int sockFd, int sec)
    {
        timeval timeout{};
        timeout.tv_sec = sec;
        timeout.tv_usec = 0;

        return setsockopt(sockFd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0 &&
               setsockopt(sockFd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0;
    }
}

bool UpstreamClient::forward(const ProxyRequestTarget &proxyRequestTarget,
                             const HttpRequestParser &request,
                             UpstreamResponse &response) const
{
    response.reset();

    if (proxyRequestTarget.service.host == nullptr ||
        proxyRequestTarget.service.port <= 0 ||
        proxyRequestTarget.service.port > 65535 ||
        proxyRequestTarget.path.empty())
        return false;

    std::string requestText;
    if (!buildRequest(proxyRequestTarget, request, requestText))
        return false;

    int sockFd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockFd < 0)
        return false;

    if (!setTimeout(sockFd, TIMEOUT))
    {
        close(sockFd);
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(proxyRequestTarget.service.port);

    if (inet_pton(AF_INET, proxyRequestTarget.service.host, &address.sin_addr) != 1)
    {
        close(sockFd);
        return false;
    }

    if (connect(sockFd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0)
    {
        close(sockFd);
        return false;
    }

    if (!sendRequest(sockFd, requestText.data(), requestText.size()))
    {
        close(sockFd);
        return false;
    }

    char buffer[BUFFER_SIZE];
    while (true)
    {
        ssize_t ret = recv(sockFd, buffer, sizeof(buffer), 0);
        if (ret > 0)
        {
            response.raw.append(buffer, static_cast<size_t>(ret));
            continue;
        }
        else if (ret == 0)
            break;
        else
        {
            close(sockFd);
            return false;
        }
    }

    close(sockFd);

    return !response.raw.empty();
}

bool UpstreamClient::buildRequest(const ProxyRequestTarget &proxyRequestTarget,
                                  const HttpRequestParser &request,
                                  std::string &requestText) const
{
    std::string method;
    switch (request.method())
    {
    case HttpRequestParser::GET:
        method = "GET";
        break;
    case HttpRequestParser::POST:
        method = "POST";
        break;
    default:
        return false;
    }

    std::ostringstream out;

    out << method << " " << proxyRequestTarget.path;
    if (proxyRequestTarget.query != nullptr && proxyRequestTarget.query[0] != '\0')
        out << "?" << proxyRequestTarget.query;
    out << " HTTP/1.1\r\n";

    out << "Host: " << proxyRequestTarget.service.host << ":" << proxyRequestTarget.service.port << "\r\n";

    out << "Connection: close\r\n";

    if (request.host() != nullptr)
        out << "X-Forwarded-Host: " << request.host() << "\r\n";

    out << "X-Forwarded-Proto: http\r\n";

    if (request.xUserName() != nullptr)
        out << "X-User-Name: " << request.xUserName() << "\r\n";

    const char *body = request.content();
    const char *contentType = request.contentType();
    int contentLength = request.contentLength();
    if (body != nullptr && contentLength > 0)
    {
        if (contentType != nullptr)
            out << "Content-Type: " << contentType << "\r\n";
        out << "Content-Length: " << contentLength << "\r\n";
    }
    else
    {
        out << "Content-Length: 0\r\n";
    }

    out << "\r\n";

    if (body != nullptr && contentLength > 0)
        out.write(body, contentLength);

    requestText = out.str();

    return true;
}