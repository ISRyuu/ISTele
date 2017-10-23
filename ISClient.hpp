//
// Created by Kevin on 2017/10/22.
//

#ifndef ISTELE_ISCLIENT_HPP
#define ISTELE_ISCLIENT_HPP

#include "SelectDriver.hpp"
#include "ist_protocol.h"
#include <iostream>

class ISClient: SelectDriver {
public:
    ISClient(int in, int out, int ipc_in, int ipc_out);
    int SendTo(int fd, int data_len, uint16_t type=IS_IPC_NORMAL, sockaddr_in *addr=nullptr);
    void cleanHeader();

private:
    static constexpr int HeaderSize = sizeof(is_tele_msg);
    static constexpr int BuffSize = 4096 + HeaderSize;
    int fd_in, fd_out, ipc_fd_in, ipc_fd_out;
    unsigned char _buff[BuffSize];
    is_tele_msg *header;
    unsigned char *data;

    std::pair<is_IPC_msg_header, int> IPCRecv();
    int eventRead(int fd) override;
    int eventWrite(int fd) override { return 0; }
    int eventExcept(int fd) override { return 0; }
};


#endif //ISTELE_ISCLIENT_HPP
