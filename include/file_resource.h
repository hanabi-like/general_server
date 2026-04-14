#ifndef FILE_RESOURCE_H
#define FILE_RESOURCE_H

#include <sys/stat.h>

class FileResource
{
public:
    static const int FILE_PATH_LEN = 200;

    enum Result
    {
        OK = 0,
        BAD_REQUEST,
        FORBIDDEN,
        NOT_FOUND,
        INTERNAL_ERROR
    };

    FileResource();
    ~FileResource();

    void reset();
    Result load(const char *url);

    char *data() const;
    int size() const;
    const char *path() const;

private:
    void unmap();

private:
    char g_filePath[FILE_PATH_LEN];
    char *g_fileBuf;
    struct stat g_fileStat;
};

#endif
