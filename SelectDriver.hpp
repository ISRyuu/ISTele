#ifndef NVEDriver_HPP
#define NVEDriver_HPP

#include "EventDriver.hpp"

extern "C" {
  #include <sys/select.h>
  #include <errno.h>
}

class SelectDriver: public EventDriver {
  /* select() version */
public:
    SelectDriver();

    bool addRead(int fd) override;
    bool addWrite(int fd) override;
    bool addExcept(int fd) override;
  
    bool deleteRead(int fd) override;
    bool deleteWrite(int fd) override;
    bool deleteExcept(int fd) override;

    int eventRead(int fd) override = 0;
    int eventWrite(int fd) override = 0;
    int eventExcept(int fd) override = 0;

    int startPoll() override;

private:
    fd_set rset, wset, eset;
    int maxfd;
};

#endif
