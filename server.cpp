//
// Created by Kevin on 2017/10/19.
//
#include <iostream>
#include "ISTransporter.hpp"
#include "Encryption/ISEncryptor.hpp"
#include "ISRouter.hpp"

extern "C" {
#include <unistd.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/socket.h>
}

const int MAX_BUFF = 1024;
unsigned char buff[MAX_BUFF];

using std::string;

int main() {

    int pid = 0;
    int socket_in[2];
    int socket_out[2];

    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, socket_in) < 0) {
        perror("socketpair error");
        exit(1);
    }

    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, socket_out) < 0) {
        perror("socketpair error");
        exit(1);
    }

    if ( (pid = fork()) < 0 ) {
        perror("cannot fork");
        exit(1);
    }

    if ( pid == 0 ) {
        /* child */
        close(socket_in[0]);
        close(socket_out[1]);
        auto i = new ISEncryptor("/Users/Kevin/Developer/ISTele/rsa/private.pem",
                                 "/Users/Kevin/Developer/ISTele/rsa/public.pem");
        auto t = new ISTransporter("0.0.0.0", "9999", ISTransporter::S_TYPE::Server,
                        socket_in[1], socket_out[0], i);
        t->startPoll();
    } else {
        /* parent */
        close(socket_in[1]);
        close(socket_out[0]);

        auto r = new ISRouter(socket_out[1], socket_in[0], ISRouterType::Server);
        r->startPoll();

        /*string in;
        fd_set rd;
        int n = 0;
        int from = 0;

        while (true) {
            FD_ZERO(&rd);
            FD_SET(socket_out[1], &rd);
            if ((n = select(socket_out[1] + 1, &rd, nullptr, nullptr, nullptr)) < 0) {
                if (errno == EINTR) {
                    std::cerr << "interrupted" << std::endl;
                    continue;
                }
                break;
            } else {
                auto res = ISTransporter::IS_IPC_recv(socket_out[1], buff, MAX_BUFF);
                if (res.second < 0) {
                    perror("IS_IPC_recv error");
                }
                if (res.first.type == IS_IPC_CLOSE) {
                    std::cout << "IS_IPC_CLOSE received" << std::endl;
                    continue;
                }
                if( res.first.type == IS_IPC_UDP) {
                    char addrstr[INET_ADDRSTRLEN];
                    if (inet_ntop(res.first.udp_addr.sin_family,
                                  &res.first.udp_addr.sin_addr,
                                  addrstr, INET_ADDRSTRLEN) != nullptr)
                        std::cout << "msg from :" << addrstr << std::endl;
                    else
                        perror("cant convert address to string");
                    continue;
                }
                from = res.first.fd;
                buff[res.second] = 0;
                std::cout << buff << std::endl;
                ISTransporter::IS_IPC_send(socket_in[0], from, IS_IPC_NORMAL, (unsigned char*)"ack", 3);
            }
        }*/
    }

    return 0;
}