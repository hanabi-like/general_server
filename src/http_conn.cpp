#include "fd_event.h"
#include "http_conn.h"

void http_conn::init_mysql(mysql_conn_pool *connPool)
{
    MYSQL *conn = NULL;
    connRAII mysqlConn(&conn, connPool);

    if (mysql_query(conn, "SELECT username,password FROM user"))
        cout << "SELECT error" << endl;

    MYSQL_RES *res = mysql_store_result(conn);

    int num_fields = mysql_num_fields(res);

    MYSQL_FIELD *fields = mysql_fetch_fields(res);

    while (MYSQL_ROW row = mysql_fetch_row(res))
    {
        string t1(row[0]);
        string t2(row[1]);
        h_users[t1] = t2;
    }
}

int http_conn::h_user_count = 0;
int http_conn::h_epollfd = -1;
unordered_map<string, string> http_conn::h_users;

void http_conn::init(int sockfd, const sockaddr_in &addr, string user, string pwd, string dbname)
{
    h_sockfd = sockfd;
    h_address = addr;
    fd_event::add(h_epollfd, sockfd, true);
    ++h_user_count;

    strcpy(mysql_user, user.c_str());
    strcpy(mysql_password, pwd.c_str());
    strcpy(mysql_dbname, dbname.c_str());

    init();
}

void http_conn::init()
{
    conn = NULL;

    h_method = GET;
    h_url = 0;
    h_version = 0;

    h_check_state = CHECK_STATE_REQUESTLINE;

    memset(h_read_buf, '\0', READ_BUFFER_SIZE);
    h_checked_idx = 0;
    h_read_idx = 0;
    h_start_idx = 0;

    h_linger = false;
    h_content_length = 0;
    h_host = 0;

    h_cgi = 0;
    h_content = 0;

    g_fileResource.reset();

    g_response.init();
}

void http_conn::close_http_conn(bool real_close)
{
    if (real_close && h_sockfd != -1)
    {
        fd_event::remove(h_epollfd, h_sockfd);
        h_sockfd = -1;
        --h_user_count;
    }
}

/*
非阻塞读
循环读取直至无数据可读或者对方关闭连接
*/
bool http_conn::read()
{
    if (h_read_idx >= READ_BUFFER_SIZE)
        return false;

    int ret = 0;
    while (true)
    {
        ret = recv(h_sockfd, h_read_buf + h_read_idx, READ_BUFFER_SIZE - h_read_idx, 0);
        if (ret < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            return false;
        }
        else if (ret == 0)
            return false;
        h_read_idx += ret;
    }
    return true;
}

http_conn::LINE_STATUS http_conn::parse_line()
{
    char temp;
    for (; h_checked_idx < h_read_idx; ++h_checked_idx)
    {
        temp = h_read_buf[h_checked_idx];
        if (temp == '\r')
        {
            if ((h_checked_idx + 1) == h_read_idx)
                return LINE_OPEN;
            else if (h_read_buf[h_checked_idx + 1] == '\n')
            {
                h_read_buf[h_checked_idx++] = '\0';
                h_read_buf[h_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            if (h_checked_idx > 1 && h_read_buf[h_checked_idx - 1] == '\r')
            {
                h_read_buf[h_checked_idx - 1] == '\0';
                h_read_buf[h_checked_idx++] == '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

http_conn::HTTP_CODE http_conn::parse_requestline(char *text)
{
    h_url = strpbrk(text, " \t");
    if (!h_url)
        return BAD_REQUEST;
    *h_url++ = '\0';

    char *method = text;
    if (strcasecmp(method, "GET") == 0)
        h_method = GET;
    else if (strcasecmp(method, "POST") == 0)
    {
        h_method = POST;
        h_cgi = 1;
    }
    else
        return BAD_REQUEST;

    h_url += strspn(h_url, " \t");
    h_version = strpbrk(h_url, " \t");
    if (!h_version)
        return BAD_REQUEST;
    *h_version++ = '\0';
    h_version += strspn(h_version, " \t");

    if (strcasecmp(h_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;

    if (strncasecmp(h_url, "http://", 7) == 0)
    {
        h_url += 7;
        h_url = strchr(h_url, '/');
    }

    if (!h_url || h_url[0] != '/')
        return BAD_REQUEST;

    if (strlen(h_url) == 1)
        strcat(h_url, "home.html");

    h_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{
    if (text[0] == '\0')
    {
        if (h_content_length != 0)
        {
            h_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
            h_linger = true;
    }
    else if (strncasecmp(text, "Content-Length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        h_content_length = atol(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        h_host = text;
    }
    else
    {
        // printf("unknown header: %s\n",text);
    }

    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_content(char *text)
{
    if ((h_content_length + h_checked_idx) <= h_read_idx)
    {
        text[h_content_length] = '\0';
        h_content = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE http_code = NO_REQUEST;
    char *text = 0;
    while ((h_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || (line_status = parse_line()) == LINE_OK)
    {
        text = h_read_buf + h_start_idx;
        h_start_idx = h_checked_idx;
        switch (h_check_state)
        {
        case CHECK_STATE_REQUESTLINE:
        {
            http_code = parse_requestline(text);
            if (http_code == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_HEADER:
        {
            http_code = parse_headers(text);
            if (http_code == BAD_REQUEST)
                return BAD_REQUEST;
            else if (http_code == GET_REQUEST)
                return do_request(); // GET_REQUEST;
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            http_code = parse_content(text);
            if (http_code == GET_REQUEST)
                return do_request();
            line_status = LINE_OPEN;
            break;
        }
        default:
        {
            return INTERNAL_ERROR;
        }
        }
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::do_request()
{
    const char *targetUrl = g_requestDispatcher.resolve(
        h_url,
        h_cgi == 1,
        h_content,
        conn,
        h_users,
        h_lock);

    FileResource::Result result = g_fileResource.load(targetUrl);
    switch (result)
    {
    case FileResource::OK:
        return FILE_REQUEST;
    case FileResource::BAD_REQUEST:
        return BAD_REQUEST;
    case FileResource::FORBIDDEN:
        return FORBIDDEN_REQUEST;
    case FileResource::NOT_FOUND:
        return NO_RESOURCE;
    default:
        return INTERNAL_ERROR;
    }
}

bool http_conn::process_write(HTTP_CODE http_code)
{
    switch (http_code)
    {
    case INTERNAL_ERROR:
    {
        if (!g_response.buildInternalError(h_linger))
            return false;
        break;
    }
    case BAD_REQUEST:
    {
        if (!g_response.buildBadRequest(h_linger))
            return false;
        break;
    }
    case NO_RESOURCE:
    {
        if (!g_response.buildNotFound(h_linger))
            return false;
        break;
    }
    case FORBIDDEN_REQUEST:
    {
        if (!g_response.buildForbidden(h_linger))
            return false;
        break;
    }
    case FILE_REQUEST:
    {
        if (!g_response.buildOkHeader(g_fileResource.size(), h_linger))
            return false;
        if (g_fileResource.size() != 0)
        {
            h_iv[0].iov_base = g_response.buffer();
            h_iv[0].iov_len = g_response.bufferSize();
            h_iv[1].iov_base = g_fileResource.data();
            h_iv[1].iov_len = g_fileResource.size();
            h_iv_count = 2;
            return true;
        }
    }
    default:
        return false;
    }
    h_iv[0].iov_base = g_response.buffer();
    h_iv[0].iov_len = g_response.bufferSize();
    h_iv_count = 1;
    return true;
}

bool http_conn::write()
{
    int temp = 0;
    int bytes_have_send = 0;
    int iovec_ptr = 0;
    int bytes_to_send = g_response.bufferSize() + g_fileResource.size();
    if (bytes_to_send == 0)
    {
        fd_event::mod(h_epollfd, h_sockfd, EPOLLIN);
        init();
        return true;
    }
    while (1)
    {
        temp = writev(h_sockfd, h_iv, h_iv_count);
        if (temp > 0)
        {
            bytes_have_send += temp;
            iovec_ptr = bytes_have_send - g_response.bufferSize();
        }
        else if (temp <= -1)
        {
            if (errno == EAGAIN) // 缓冲区已满
            {
                if (bytes_have_send >= h_iv[0].iov_len)
                {
                    h_iv[0].iov_len = 0;
                    h_iv[1].iov_base = g_fileResource.data() + iovec_ptr;
                    h_iv[1].iov_len = bytes_to_send;
                }
                else
                {
                    h_iv[0].iov_base = g_response.buffer() + bytes_have_send;
                    h_iv[0].iov_len -= bytes_have_send;
                }
                fd_event::mod(h_epollfd, h_sockfd, EPOLLOUT);
                return true;
            }
            g_fileResource.reset();
            return false;
        }
        bytes_to_send -= temp;
        if (bytes_to_send <= 0)
        {
            g_fileResource.reset();
            fd_event::mod(h_epollfd, h_sockfd, EPOLLIN);
            if (h_linger)
            {
                init();
                return true;
            }
            else
                return false;
        }
    }
}

void http_conn::process()
{
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST)
    {
        fd_event::mod(h_epollfd, h_sockfd, EPOLLIN);
        return;
    }
    bool write_ret = process_write(read_ret);
    if (!write_ret)
        close_http_conn();
    fd_event::mod(h_epollfd, h_sockfd, EPOLLOUT);
}
