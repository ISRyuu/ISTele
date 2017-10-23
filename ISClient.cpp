//
// Created by Kevin on 2017/10/22.
//

#include "ISClient.hpp"
#include "ISTransporter.hpp"

ISClient::ISClient(int in, int out, int ipc_in, int ipc_out)
: fd_in(in), fd_out(out), ipc_fd_in(ipc_in), ipc_fd_out(ipc_out),
  data(_buff + HeaderSize), header(reinterpret_cast<is_tele_msg*>(_buff))
{
    addRead(ipc_fd_in);
    addRead(fd_in);
}

int
ISClient::eventRead(int fd)
{
    if ( fd == fd_in ) {
        /* user command */
    }

    if ( fd == ipc_fd_in ) {
        auto res = IPCRecv();
        if ( res.second < 0 ) {
            perror("IPCRecv error");
            return -1;
        }

        switch (res.first.type) {
            case IS_IPC_NORMAL:

                break;
            case IS_IPC_CONN:
                std::cout << "connected" << std::endl;
                break;
            case IS_IPC_CLOSE:
                break;
            case IS_IPC_UDP:
                break;
            default:
                break;
        }

    }
    return 0;
}


void
ISClient::cleanHeader()
{
    bzero(header, HeaderSize);
}




int
ISClient::SendTo(int fd, int data_len, uint16_t type, sockaddr_in *addr)
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

