//
// Created by Kevin on 2017/9/20.
//

#include <iostream>
#include "ISRouter.hpp"
#include "ISTransporter.hpp"
#include "Encryption/ISEncryptor.hpp"

extern "C" {
#include <unistd.h>
}

ISRouter::ISRouter(int in, int out, const ISRouterType &t, int u_in, int u_out)
:fd_in(in), fd_out(out), type(t), fd_u_in(u_in), fd_u_out(u_out),
 header(reinterpret_cast<is_tele_msg*>(_buff)), data(_buff + HeaderSize)
{
    addRead(fd_in);
    if ( type == ISRouterType::Client ) {
        if (fd_u_in < 0 || fd_u_out < 0) {
            std::cerr << "bad file descriptor" << std::endl;
            exit(1);
        }
    }
}

enum class ISRClientStatus {
    Online,
    Busy
};

enum class ISRSessionStatus {
    Init,
};

struct ISRClient {
    int             fd;
    unsigned int    id;
    unsigned int    session;
    ISRClientStatus status;
};

struct ISRSession {
    int              from;
    int              to;
    ISRSessionStatus status;
};

void
ISRouter::IPCRecv()
{
    IPC_res = ISTransporter::IS_IPC_recv(fd_in, _buff, BuffSize);
}

int
ISRouter::eventRead(int fd)
{
    if (type == ISRouterType::Client && fd == fd_u_in) {
        /* user input */
        auto n = read(fd_u_in, data, BuffSize);
        data[n] = 0;
        std::cout << data << std::endl;
        return 0;
    }

    IPCRecv();

    if ( IPC_res.second < 0 ) {
        perror("Error IS_IPC_recv");
        return -1;
    }

    switch (IPC_res.first.type) {
        case IS_IPC_UDP:
            handleUDP();
            break;
        case IS_IPC_NORMAL:
            handleNormal();
            break;
        case IS_IPC_CLOSE:
            handleClose();
            break;
        case IS_IPC_CONN:
            handleConn();
            break;
        default:
        std::cerr << "unknown IPC message" << std::endl;
            break;
    }
    return 0;
}

int
ISRouter::handleConn()
{
    if (type != ISRouterType::Client) {
        IPCCloseFD(IPC_res.first.fd);
        return -1;
    }
    IS_USER_NOTIF msg = IS_USER_READY;
    auto len = sizeof(msg);
    if ( write(fd_u_out, &msg, len) != len ) {
        perror("write fd_u_out error");
        exit(1);
    }
    /* start polling user's commands */
    addRead(fd_u_in);
    return 0;
}

void
ISRouter::cleanHeader()
{
    bzero(header, HeaderSize);
}

int
ISRouter::SendTo(int fd, int data_len, uint16_t type, sockaddr_in *addr)
{
    header_hton(header);
    if (addr) {
        return ISTransporter::IS_IPC_send(fd_out, fd, IS_IPC_UDP, _buff,
                                          data_len + HeaderSize, addr) - HeaderSize;
    }
    auto n = data_len + HeaderSize;
    if (type == IS_IPC_CLOSE)
        n = 0;
    return ISTransporter::IS_IPC_send(fd_out, fd, type, _buff, data_len + HeaderSize) - HeaderSize;
}

void
ISRouter::IPCCloseFD(int fd)
{
    auto client = Clients.find(fd);
    if ( client != Clients.end()) {
        if (client->second.session != 0) {
            auto sess = sessions.find(client->second.session);
            if ( sess != sessions.end() ) {
                cleanHeader();
                header->type = IS_TELE_CLOSE;
                header->seq_sess = sess->first;
                SendTo(sess->second.to, 0);
                sessions.erase(client->second.session);
            }
        }
        IDs.erase(client->second.id);
        Clients.erase(fd);
    }
    SendTo(fd, 0, IS_IPC_CLOSE);
}

int
ISRouter::handleNormal()
{
    int fd = IPC_res.first.fd;
    auto len = IPC_res.second;
    if ( len < HeaderSize ) {
        IPCCloseFD(fd);
    }

    header_ntoh(header);

    auto client = Clients.find(fd);

    switch (header->type) {

        case IS_TELE_LOGIN:
        {
            auto id = IDs.find(header->id);
            if (id == IDs.end()) {
                if (client != Clients.end()) {
                    std::cerr << "invalid request" << std::endl;
                    IPCCloseFD(fd);
                    return -1;
                }
                IDs[header->id] = fd;
                Clients[fd] = {fd, header->id, 0, ISRClientStatus::Online};
                std::cout << header->id << "logged in" << std::endl;
            } else {
                /* id is not unique */
                std::cerr << header->id << " is already online" << std::endl;
                IPCCloseFD(fd);
                return -1;
            }
        }
        memcpy(data, "hello", 5);
        std::cout << (SendTo(fd, 5) == 5) << std::endl;
            break;

        case IS_TELE_CALL:
        {
            /* Here the id is the id which be called. */
            auto remote_id = IDs.find(header->id);
            if (len - HeaderSize != ISAES_KEY_LEN ||
                client == Clients.end() || client->second.status == ISRClientStatus::Busy) {
                std::cerr << "invalid request" << std::endl;
                IPCCloseFD(fd);
                return -1;
            }
            cleanHeader();
            if (remote_id == IDs.end()) {
                /* remote id is not online */
                header->type  = IS_TELE_ERR;
                header->error = IS_ERROR_OFFL;
                if (SendTo(fd, 0) < 0)
                    IPCCloseFD(fd);
                return 0;
            } else {
                auto remote_client = Clients.find(remote_id->second);
                if (remote_client == Clients.end()) {
                    std::cerr << "fetal error: unknown ID exists" << std::endl;
                    exit(-1);
                }
                if (remote_client->second.status == ISRClientStatus::Busy) {
                    /* remote peer is busy now */
                    cleanHeader();
                    header->type  = IS_TELE_ERR;
                    header->error = IS_ERROR_BUSY;
                    header->id    = remote_id->first;
                    if (SendTo(fd, 0) < 0)
                        IPCCloseFD(fd);
                    return 0;
                }
                header->type  = IS_TELE_CALL;
                header->id    = client->second.id;
                unsigned sess = 0;
                do {
                    sess = (unsigned int)arc4random();
                } while (sess != 0 && sessions.find(sess) == sessions.end());
                header->seq_sess = sess;
                if (SendTo(remote_id->second, len - HeaderSize) == HeaderSize) {
                    ISRSession s;
                    s.from   = fd;
                    s.to     = remote_id->second;
                    s.status = ISRSessionStatus::Init;
                    sessions[sess] = s;
                    client->second.status  = remote_client->second.status  = ISRClientStatus::Busy;
                    client->second.session = remote_client->second.session = sess;
                } else {
                    header->type  = IS_TELE_ERR;
                    header->error = IS_ERROR_INTL;
                    if (SendTo(fd, 0) < 0)
                        IPCCloseFD(fd);
                }
            }
        }
            break;

        default:
            std::cerr << "not implemented" << std::endl;
            IPCCloseFD(fd);
        break;
    }
    return 0;
}

int
ISRouter::handleUDP()
{

}

int
ISRouter::handleClose()
{
    IPCCloseFD(IPC_res.first.fd);
    return 0;
}