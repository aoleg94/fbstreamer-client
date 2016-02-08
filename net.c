#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <errno.h>
#include <signal.h>
#include <sys/uio.h>
#define __USE_MISC
#include <netdb.h>
#include <sys/select.h>
#include "net.h"

#define CHECK(x) if((x) < 0) { perror(#x); return -1; }

#define HEADER_SYNC "LalkA123"
struct MsgHeader
{
    char sync[8];
    size_t length;
    int number, count;
} mh = { HEADER_SYNC, 0, 0, 0 };

int fd;
int do_connect(const char* ip)
{
    struct sockaddr_in addr;

    CHECK(fd = socket(AF_INET, SOCK_STREAM, 0));

    struct hostent* he = gethostbyname(ip);
    if(!he || !he->h_length)
    {
        fprintf(stderr, "do_connect: gethostbyname failed: %s\n", hstrerror(h_errno));
        return -1;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ((struct in_addr*)(he->h_addr_list[0]))->s_addr;
    addr.sin_port = htons(1920);

    CHECK(connect(fd, (struct sockaddr*)&addr, sizeof(addr)));

    return fd;
}

void* buffer = NULL;
int reqlen = 0;
int buflen = 1024;
int avail = 0;
int connstate;
enum State
{
    START,
    HEADER,
    DATA
};

int read_more()
{
    if(!buffer || reqlen > buflen)
    {
        buffer = realloc(buffer, (buflen = reqlen*125/100));
    }
    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd, &set);
    struct timeval tv;
    tv.tv_usec = 5*1000;
    tv.tv_sec = 0;
    if(select(fd+1, &set, NULL, NULL, &tv))
    {
        int nb;
        CHECK(nb = read(fd, buffer+avail, reqlen-avail));
        avail += nb;
    }
}

int poll_frame()
{
    switch(connstate)
    {
    case START:
        reqlen = sizeof(mh);
        connstate = HEADER;
        //break;
    case HEADER:
        read_more();
        if(avail == sizeof(mh))
        {
            mh = *(struct MsgHeader*)(buffer);
            if(strncmp(mh.sync, HEADER_SYNC, 8))
            {
                fprintf(stderr, "poll_frame: got wrong header\n");
                shutdown(fd, SHUT_RDWR);
                close(fd);
                return -1;
            }
            avail = 0;
            reqlen = mh.length;
            connstate = DATA;
        }
        else break;
    case DATA:
        read_more();
        if(avail == reqlen)
        {
            return 1;
        }
        else break;
    }
    return 0;
}

int get_jpeg_data(void **data)
{
    if(connstate != DATA || avail != reqlen)
        return -1;

    connstate = START;
    avail = 0;

    *data = buffer;
    return reqlen;
}
