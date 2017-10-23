#include "SelectDriver.hpp"

SelectDriver::SelectDriver()
{
  FD_ZERO(&rset);
  FD_ZERO(&wset);
  FD_ZERO(&eset);
  maxfd = -1;
}

int
SelectDriver::startPoll()
{
    fd_set r, w, e;

    for (;;) {
        r = rset;
        w = wset;
        e = eset;
        int n = 0;

        if ((n = select(maxfd + 1, &r, &w, &e, nullptr)) < 0) {
            if (errno == EINTR)
                continue;
            return errno;
        }

        for (int i = 0; i < maxfd + 1; i++) {

            if (FD_ISSET(i, &r))
                eventRead(i);
            if (FD_ISSET(i, &w))
                eventWrite(i);
            if (FD_ISSET(i, &e))
                eventExcept(i);

        }
    }
    return 0;
}

bool
SelectDriver::addRead(int fd)
{
    if (fd > maxfd) maxfd = fd;
    FD_SET(fd, &rset);
    return true;
}

bool
SelectDriver::addWrite(int fd)
{
    if (fd > maxfd) maxfd = fd;
    FD_SET(fd, &wset);
    return true;
}

bool
SelectDriver::addExcept(int fd)
{
    if (fd > maxfd) maxfd = fd;
    FD_SET(fd, &eset);
    return true;
}

bool
SelectDriver::deleteRead(int fd)
{
    FD_CLR(fd, &rset);
    return true;
}

bool
SelectDriver::deleteWrite(int fd)
{
    FD_CLR(fd, &wset);
    return true;
}

bool
SelectDriver::deleteExcept(int fd)
{
    FD_CLR(fd, &wset);
    return true;
}
