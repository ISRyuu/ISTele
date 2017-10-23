//
// Created by Kevin on 2017/9/20.
//

#ifndef ISTELE_ISROUTER_HPP
#define ISTELE_ISROUTER_HPP

#include <map>
#include <vector>

#include "SelectDriver.hpp"
#include "ist_protocol.h"

constexpr int BuffSize = 4096 + sizeof(is_tele_msg);

struct ISRClient;
struct ISRSession;

enum class ISRouterType {
    Client,
    Server,
};

class ISRouter: public SelectDriver {

public:
    ISRouter(int in, int out, const ISRouterType &t, int u_in=-1, int u_out=-1);
    int eventRead(int fd) override ;
    int eventWrite(int fd) override { return 0; };
    int eventExcept(int fd) override  { return 0; }

private:
    int fd_in, fd_out, fd_u_in, fd_u_out;
    unsigned char _buff[BuffSize];
    unsigned char *data;
    is_tele_msg *header;
    std::map<int, ISRClient> Clients;
    std::map<unsigned int, int> IDs;
    std::map<unsigned int, ISRSession> sessions;
    std::pair<is_IPC_msg_header, int> IPC_res;
    static constexpr int HeaderSize = sizeof(is_tele_msg);
    const ISRouterType type;

    int handleNormal();
    int handleUDP();
    int handleClose();
    int handleConn();
    void cleanHeader();
    void IPCCloseFD(int fd);

    int SendTo(int fd, int data_len, uint16_t type=IS_IPC_NORMAL, sockaddr_in *addr=nullptr);
    void IPCRecv();

};


#endif //ISTELE_ISROUTER_HPP
