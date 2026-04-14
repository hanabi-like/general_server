#include "auth_request_handler.h"

#include <cstdlib>
#include <cstring>

const char *AuthRequestHandler::resolve(
    const char *url,
    const char *content,
    MYSQL *conn,
    UserRepository &userRepository)
{
    const char *p = std::strrchr(url, '/');
    if (!p || *(p + 1) == '\0')
        return url;

    std::string username;
    std::string password;
    parseUserInfo(content, username, password);

    if (*(p + 1) == '2')
    {
        if (userRepository.create(username, password, conn))
            return "/regOk.html";
        return "/regError.html";
    }

    if (*(p + 1) == '3')
    {
        if (userRepository.verify(username, password))
            return "/loginOk.html";
        return "/loginError.html";
    }

    return url;
}

void AuthRequestHandler::parseUserInfo(const char *content, std::string &username, std::string &password) const
{
    char usernameBuffer[USER_INFO_LEN];
    char passwordBuffer[USER_INFO_LEN];
    int idx = 0;
    int i = 0;
    for (idx = 9; content[idx] != '&'; ++idx, ++i)
        usernameBuffer[i] = content[idx];
    usernameBuffer[i] = '\0';
    int j = 0;
    for (idx = idx + 10; content[idx] != '\0'; ++idx, ++j)
        passwordBuffer[j] = content[idx];
    passwordBuffer[j] = '\0';
    username = usernameBuffer;
    password = passwordBuffer;
}
