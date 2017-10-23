
#include <iostream>
#include "ISRouter.hpp"
#include "ISTransporter.hpp"
#include "Encryption/ISEncryptor.hpp"

extern "C" {
#include <unistd.h>
#include <sys/select.h>
#include <sys/uio.h>
#include <pthread.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "ist_protocol.h"
}

const int MAX_BUFF = 4096;
unsigned char buff[MAX_BUFF];

using std::string;

void sigcld_handler(int sig)
{
    wait(nullptr);
    std::cerr << "transporter is dead" << std::endl;
    exit(1);
}

void *router_thread(void* arg);

int main() {

    struct sigaction sig;
    sigemptyset(&sig.sa_mask);
    sig.sa_flags   = 0;
    sig.sa_handler = sigcld_handler;
    if ( sigaction(SIGCHLD, &sig, nullptr) < 0 ) {
        perror("cannot set SIGCHLD handler");
        exit(1);
    }

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
        /* child - transporter */
        close(socket_in[0]);
        close(socket_out[1]);

        auto i = new ISEncryptor();
        ISTransporter t("0.0.0.0", "9999", ISTransporter::S_TYPE::Client,
                        socket_in[1], socket_out[0], i);
        t.startPoll();
    } else {
        /* parent */
        close(socket_in[1]);
        close(socket_out[0]);

        int socket_u_in[2];
        int socket_u_out[2];
        pthread_t tid;
        int fds[4];
        fd_set rd;
        int n = 0;

        if (socketpair(AF_UNIX, SOCK_DGRAM, 0, socket_u_in) < 0) {
            perror("socketpair error");
            kill(pid, SIGKILL);
            exit(1);
        }

        if (socketpair(AF_UNIX, SOCK_DGRAM, 0, socket_u_out) < 0) {
            perror("socketpair error");
            kill(pid, SIGKILL);
            exit(1);
        }

        fds[0] = socket_out[1];
        fds[1] = socket_in[0];
        fds[2] = socket_u_in[1];
        fds[3] = socket_u_out[0];

        pthread_create(&tid, nullptr, router_thread, static_cast<void *>(fds));

        auto u_input = STDIN_FILENO;
        auto r_input = socket_u_out[1];
        auto r_fd    = socket_u_in[0];

        auto maxfd = std::max(u_input, r_input);
        bool connected = false;

        while ( true ) {
            FD_ZERO(&rd);
            FD_SET(u_input, &rd);
            FD_SET(r_input, &rd);

            n = select(maxfd+1, &rd, nullptr, nullptr, nullptr);

            if (n < 0) {
                if (errno == EINTR)
                    continue;
                perror("select error");
                kill(pid, SIGKILL);
                exit(1);
            }

            if (FD_ISSET(u_input, &rd)) {
                /* user's input */
                read(u_input, buff, MAX_BUFF);
                std::cout << buff << std::endl;
            }

            if (FD_ISSET(r_input, &rd)) {
                /* message from ISRouter */
                read(r_input, buff, MAX_BUFF);
                auto ind = reinterpret_cast<IS_USER_NOTIF*>(buff);

                if ( *ind == IS_USER_READY ) {
                    std::cout << "connnected" << std::endl;
                    write(r_fd, "LOGIN", 5);
                } else { std::cout << "unknow msg" << std::endl; }
            }

        }
    }
/*
        string in;
        fd_set rd;
        int n = 0;

        std::cout << "connecting..." << std::endl;
        int from = 0;
        while (true) {
            FD_ZERO(&rd);
            FD_SET(socket_out[1], &rd);
            if ((n = select(socket_out[1] + 1, &rd, nullptr, nullptr, nullptr)) < 0) {
                if (errno == EINTR) {
                    std::cerr << "interrupted" << std::endl;
                    continue;
                }
                perror("select error");
            } else {
                auto res = ISTransporter::IS_IPC_recv(socket_out[1], buff, MAX_BUFF);

                if (res.second < 0) {
                    perror("IS_IPC_recv ERROR");
                    break;
                }
                if ( res.first.type == IS_IPC_CONN ) {
                    std::cout << "connected" << std::endl;
                }
                if (res.first.type == IS_IPC_CLOSE) {
                    std::cout << "client close" << std::endl;
                    break;
                }
                from = res.first.fd;
                buff[res.second] = 0;
                std::cout << buff+sizeof(is_tele_msg) << std::endl;

                is_IPC_msg_header header;
                bzero(&header, sizeof(header));
                header.type = IS_IPC_NORMAL;
                string in;
                std::cin >> in;
                if (std::cin.eof())
                    break;
                sockaddr_in addr;
                bzero(&addr, sizeof(addr));
                addr.sin_port = htons(9999);
                addr.sin_family = AF_INET;
                inet_pton(AF_INET, "localhost", &addr.sin_addr.s_addr);
                is_tele_msg msg;
                msg.id = htonl(12345);
                msg.type = htons(IS_TELE_LOGIN);

                n = ISTransporter::IS_IPC_send(socket_in[0], from, IS_IPC_NORMAL,
                                               (unsigned char*)&msg, sizeof(msg));
                //n = ISTransporter::IS_IPC_send(socket_in[0], from, IS_IPC_UDP, nullptr, 0, &addr);
            }*/
}

void*
router_thread(void* arg)
{
    auto fds = reinterpret_cast<int*>(arg);
    auto r = new ISRouter(fds[0], fds[1], ISRouterType::Client, fds[2], fds[3]);
    r->startPoll();
}

