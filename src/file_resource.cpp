#include "file_resource.h"

#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace
{
    const char *STATIC_ROOT = "./static";
}

FileResource::FileResource()
    : g_fileBuf(0)
{
    std::memset(g_filePath, '\0', FILE_PATH_LEN);
    std::memset(&g_fileStat, 0, sizeof(g_fileStat));
}

FileResource::~FileResource()
{
    unmap();
}

void FileResource::reset()
{
    unmap();
    std::memset(g_filePath, '\0', FILE_PATH_LEN);
    std::memset(&g_fileStat, 0, sizeof(g_fileStat));
}

FileResource::Result FileResource::load(const char *url)
{
    if (url == nullptr)
        return BAD_REQUEST;

    reset();

    std::strcpy(g_filePath, STATIC_ROOT);
    int len = std::strlen(STATIC_ROOT);
    std::strncpy(g_filePath + len, url, FILE_PATH_LEN - len - 1);

    if (stat(g_filePath, &g_fileStat) < 0)
        return NOT_FOUND;

    if (!(g_fileStat.st_mode & S_IROTH))
        return FORBIDDEN;

    if (S_ISDIR(g_fileStat.st_mode))
        return BAD_REQUEST;

    int fd = open(g_filePath, O_RDONLY);

    if (fd < 0)
        return NOT_FOUND;

    if (g_fileStat.st_size == 0)
    {
        close(fd);
        return OK;
    }

    g_fileBuf = static_cast<char *>(mmap(0, g_fileStat.st_size, PROT_READ, MAP_PRIVATE, fd, 0));
    close(fd);

    if (g_fileBuf == MAP_FAILED)
    {
        g_fileBuf = 0;
        return INTERNAL_ERROR;
    }

    return OK;
}

char *FileResource::data() const
{
    return g_fileBuf;
}

int FileResource::size() const
{
    return g_fileStat.st_size;
}

const char *FileResource::path() const
{
    return g_filePath;
}

void FileResource::unmap()
{
    if (g_fileBuf)
    {
        munmap(g_fileBuf, g_fileStat.st_size);
        g_fileBuf = 0;
    }
}
