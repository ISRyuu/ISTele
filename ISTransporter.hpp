//
// Created by Kevin on 2017/9/20.
//

#ifndef ISTELE_ISTRANSPORTER_H
#define ISTELE_ISTRANSPORTER_H

#include "SelectDriver.hpp"
#include "Encryption/ISEncryptor.hpp"
#include <string>
#include <map>

extern "C" {
#include "ist_protocol.h"
};

constexpr int _UDPBUFFSIZE = 512 - sizeof(is_tele_msg);
constexpr int _TCPBUFFSIZE = 4096 - sizeof(is_tele_msg);

enum class ISSTATUS {
    IS_CONNECTED,
    IS_KEYEXCG,
    IS_READY,
};

struct ISConnection {
    ISSTATUS status;
    unsigned char key[ISAES_KEY_LEN];
};

class ISTransporter: public SelectDriver {
    using Peers = std::map<int, ISConnection>;

public:
    enum class S_TYPE {
        Server,
        Client
    };

public:
    ISTransporter(std::string host, std::string service, S_TYPE type, int in, int out,
                  ISEncryptor* en = nullptr);
    static int
    IS_IPC_send(int fd, int from, uint16_t type, unsigned char* data, size_t data_len,
                sockaddr_in *addr=nullptr);
    static std::pair<is_IPC_msg_header, int>
    IS_IPC_recv(int fd, unsigned char* buff, size_t buff_size);

private:
    const S_TYPE service_type;
    int tcp_fd, udp_fd;
    int fd_in, fd_out;
    uint16_t port;
    static int set_nblock(int fd);
    std::unique_ptr<ISEncryptor> encryptor;
    Peers cons;
    unsigned char tcp_buffer[_TCPBUFFSIZE];
    unsigned char udp_buffer[_UDPBUFFSIZE];
    unsigned char enc_buff[_TCPBUFFSIZE];
    is_tele_trans_header header;
    size_t header_len;

    int initHandler(Peers::iterator);
    void cleanHeader();
    int handleIPCIn();
    int keyexchangeHandler(Peers::iterator);
    int requestHandler(Peers::iterator);
    void removePeer(Peers::iterator peer, bool IPC_notify=true);

    int eventRead(int fd) override;
    int eventWrite(int fd) override;
    int eventExcept(int fd) override;

    int IS_send(int fd, unsigned char *data, size_t data_len, unsigned char type, bool cleanheader=true);
    std::pair<int, unsigned char*> IS_recv(int fd);

    int ISAESsend(Peers::iterator peer, unsigned char* data, size_t data_len, unsigned char type);
    std::pair<int, unsigned char*> ISAESrecv(Peers::iterator peer);

};


#endif //ISTELE_ISTRANSPORTER_H