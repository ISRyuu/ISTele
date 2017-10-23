//
// Created by Kevin on 2017/9/20.
//

#include <iostream>
#include "ISTransporter.hpp"

using std::string;

extern "C" {
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include "rm_net/rm_net.h"
}

ISTransporter::ISTransporter(string host, string service, S_TYPE type, int in, int out, ISEncryptor *en)
:service_type(type), encryptor(en), header_len(sizeof(is_tele_trans_header)), fd_in(in), fd_out(out)
{
    sockaddr_in *addr;
    socklen_t  t;
    switch (service_type) {
        case S_TYPE::Client:
            if ( (tcp_fd = tcp_connect(host.c_str(), service.c_str())) < 0 ) {
                perror("cannot connect to server");
                exit(1);
            }
            if ( (udp_fd = udp_client("0.0.0.0", nullptr, (sockaddr**)&addr, &t)) < 0 ) {
                perror("cannot create udp client");
                exit(1);
            }
            std::cout << "client udp port" << ntohs(addr->sin_port) << std::endl;
            ISConnection c;
            c.status = ISSTATUS::IS_CONNECTED;
            initHandler(cons.insert({tcp_fd, c}).first);
            break;
        case S_TYPE::Server:
            if ( (tcp_fd = tcp_server(host.c_str(), service.c_str(), nullptr)) < 0 ) {
                perror("cannot create tcp server");
                exit(1);
            }
            if ( (udp_fd = udp_server(host.c_str(), service.c_str(), nullptr)) < 0 ) {
                perror("cannot create udp server");
                exit(1);
            }
            break;
    }

    if ( set_nblock(tcp_fd) != 0 ) {
        perror("error setting nonblock");
        exit(1);
    }

    addRead(udp_fd);
    addRead(tcp_fd);
    addRead(fd_in);
}

int
ISTransporter::handleIPCIn()
{
    /* from outsize to transport layer */
    ssize_t n = 0;
    auto res = IS_IPC_recv(fd_in, tcp_buffer, _TCPBUFFSIZE);
    auto len = res.second;
    auto type = (service_type == S_TYPE::Client) ? IS_RQST : IS_RPLY;
    if (len < 0) {
        exit(1);
    }
    int fd = res.first.fd;
    auto peer = cons.find(fd);
    if (peer != cons.end()) {
        if (res.first.type == IS_IPC_CLOSE) {
            /* close */
            removePeer(peer, false);
            if (service_type == S_TYPE::Client) {
                exit(0);
            }
        } else if (res.first.type == IS_IPC_NORMAL) {

            n = ISAESsend(peer, tcp_buffer, len, type);
            if (n != len) {
                // TODO ERROR
            }
        } else {
            /* UDP */
            connect(udp_fd, reinterpret_cast<sockaddr *>(&res.first.udp_addr), sizeof(res.first.udp_addr));
            IS_send(udp_fd, tcp_buffer, len, IS_UDPP);
            /* dissolve the association */
            res.first.udp_addr.sin_family = AF_UNSPEC;
            connect(udp_fd, reinterpret_cast<sockaddr *>(&res.first.udp_addr), sizeof(res.first.udp_addr));
        }
    } else close(fd);
    return -1;
}

int
ISTransporter::eventRead(int fd)
{
    ssize_t n = 0;
    sockaddr_storage addr;
    socklen_t len = sizeof(addr);
    char addrstr[INET_ADDRSTRLEN];

    if (fd == fd_in) {
        n = handleIPCIn();
        if ( n == -1 ) {

        }
        return 0;
    }
    /* client */
    if (service_type == S_TYPE::Client) {

        if (fd == fd_in) {
            if ( (n = read(fd_in, tcp_buffer, _TCPBUFFSIZE)) < 0 ) {
                perror("IPC fd_in read error");
                exit(1);
            }
            if ( n == 0 ) {
                /* pipe closed */
                std::cerr << "kill connection" << std::endl;
                exit(0);
            }
            /* There should be only on peer in client's cons */
            if (ISAESsend(cons.begin(), tcp_buffer, n, IS_RQST) != n + header_len ) {
                perror("ISAESsend error");
                exit(1);
            }
            return 0;
        }

        auto res = cons.find(fd);
        if ( res == cons.end() ) {
            close(fd);
            std::cerr << "unknown file descriptor" << std::endl;
            exit(-1);
        }
        switch (res->second.status) {
            case ISSTATUS::IS_CONNECTED:
                if ( initHandler(res) != 0 ) {
                    removePeer(res);
                    exit(1);
                }
                break;
            case ISSTATUS::IS_KEYEXCG:
                if ( keyexchangeHandler(res) != 0 ) {
                    removePeer(res);
                    exit(1);
                }
                break;
            case ISSTATUS::IS_READY:
                if ( requestHandler(res) != 0 ) {
                    removePeer(res);
                    exit(1);
                }
                break;
        }
        return 0;
    }

    if (fd == tcp_fd) {
        /* new connections */
        int client = accept(fd, reinterpret_cast<sockaddr*>(&addr), &len);
        if ( client < 0 ) {
            perror("accept error");
            close(client);
        } else {
            if (set_nblock(fd) != 0) {
                perror("error setting nonblock");
                close(client);
            } else {
                if (inet_ntop(addr.ss_family, &(reinterpret_cast<sockaddr_in *>(&addr)->sin_addr),
                              addrstr, INET_ADDRSTRLEN) != nullptr)
                    std::cout << "connection from: " << addrstr << std::endl;
                else
                    perror("cant convert address to string");

                auto c = ISConnection();
                c.status = ISSTATUS::IS_CONNECTED;
                addRead(client);
                cons.insert({client, c});
            }
        }

    } else if (fd == udp_fd) {
        /* udp messages */
        ssize_t n = recvfrom(udp_fd, udp_buffer, _UDPBUFFSIZE, 0, reinterpret_cast<sockaddr*>(&addr), &len);
        if ( n < header_len ) {
            if ( n < 0 )
                perror("recvfrom error");
            else std::cerr << "UDP format error" << std::endl;
        } else {
            auto header = reinterpret_cast<is_tele_trans_header*>(udp_buffer);
            if (ISMTYP(header) == IS_UDPP) {
                if (_UDPBUFFSIZE - len < n) {
                    /* in case of long illegal messages */
                    return 1;
                }
                memcpy(udp_buffer + n, &addr, len);
            }
            // TODO IPC SEND
            IS_IPC_send(fd_out, 0, IS_IPC_UDP, udp_buffer, len+n, reinterpret_cast<sockaddr_in*>(&addr));
        }
    } else {
        /* peers messages */
        auto peer = cons.find(fd);
        if ( peer != cons.end() ) {
            if (peer->second.status == ISSTATUS::IS_CONNECTED) {
                if (initHandler(peer) != 0)
                    removePeer(peer);
            } else if (peer->second.status == ISSTATUS::IS_READY) {
                if (requestHandler(peer) != 0)
                    removePeer(peer);
            } else if (peer->second.status == ISSTATUS::IS_KEYEXCG) {
                if (keyexchangeHandler(peer) != 0)
                    removePeer(peer);
            }
        } else {
            /* unknow connection */
            shutdown(fd, SHUT_WR);
            deleteRead(fd);
        }
    }
    return 0;
}

int
ISTransporter::IS_send(int fd, unsigned char *data, size_t data_len, unsigned char type, bool cleanheader) {
    if (fd < 0) return -1;

    iovec iov[2];
    iov[0].iov_base = &header;
    iov[0].iov_len  = header_len;
    iov[1].iov_base = data;
    iov[1].iov_len  = data_len;
    if (cleanheader) cleanHeader();

    SET_ISMTYP(&header, type);
    SET_ISMLEN(&header, data_len + header_len);
    header._type_len = htons(header._type_len);

    auto n = writev(fd, iov, 2);
    if (n < 0) {
        perror("writev in IS_send error");
        return -1;
    }
    if ( n != data_len + header_len ) {
        std::cerr << "IS_send sending data failed." << std::endl;
        return -1;
    }
    return static_cast<int>(n);
}

std::pair<int, unsigned char*>
ISTransporter::ISAESrecv(Peers::iterator peer)
{
    if (peer == cons.end()) return {-1, NULL};
    auto res = IS_recv(peer->first);
    if (res.first <= 0 || !encryptor) return res;
    int num = 0;
    encryptor->IS_cfb128_encrypt_k256(res.second, enc_buff, static_cast<size_t>(res.first), header.iv,
                                      peer->second.key, &num, AES_DECRYPT);
    return {res.first, enc_buff};
}

int
ISTransporter::ISAESsend(Peers::iterator peer, unsigned char *data, size_t data_len, unsigned char type)
{
    if (peer == cons.end()) return -1;
    cleanHeader();
    if (encryptor) {
        int num = 0;
        unsigned char iv[ISAES_BLOCK_SIZE];
        encryptor->IS_rand_bytes(iv, ISAES_BLOCK_SIZE);
        memcpy(header.iv, iv, sizeof(header.iv));
        encryptor->IS_cfb128_encrypt_k256(data, enc_buff, data_len, iv, peer->second.key, &num, AES_ENCRYPT);
        data = enc_buff;
    }
    return IS_send(peer->first, data, data_len, type, false);
}

std::pair<int, unsigned char*>
ISTransporter::IS_recv(int fd)
{
    if ( fd < 0 ) return {-1, nullptr};

    iovec iov[2];
    iov[0].iov_base = &header;
    iov[0].iov_len  = header_len;
    bzero(&header, header_len);

    iov[1].iov_base = tcp_buffer;
    iov[1].iov_len  = static_cast<size_t>(_TCPBUFFSIZE);

    auto n = read(fd, &header, header_len);

    if ( n <= 0 ) {
        /* recv error */
        return {n, nullptr};
    }

    if ( n != header_len ) {
        std::cerr << "IS_recv: header format error" << std::endl;
        return {-1, nullptr};
    }

    header._type_len = ntohs(header._type_len);
    auto len = ISMLEN(&header) - static_cast<int>(header_len);

    if ( len > _TCPBUFFSIZE || len < 0 ) {
        std::cerr << "IS_recv: ridiculous header length" << std::endl;
        return {-1, nullptr};
    }

    n = 0;
    if ( len > 0 ) {
        n = read(fd, tcp_buffer, static_cast<size_t >(ISMLEN(&header)));
        if (n != len) {
            std::cerr << "IS_recv: data length doesnt match" << std::endl;
            return {-1, nullptr};
        }
    }

    return {n+header_len, tcp_buffer};
}

int
ISTransporter::initHandler(Peers::iterator peer)
{
    if (peer != cons.end()) {
        int n = 0;
        /* server */
        if (service_type == S_TYPE::Server) {

            auto res = IS_recv(peer->first);
            if (res.first == 0)
                /* remote peer closes the connection */
                return 1;

            if (res.first != header_len) {
                if (res.first < 0) perror("error occurred reading init message");
                else std::cerr << "IS_INIT bad format" << std::endl;
            } else if (ISMTYP(&header) == IS_INIT) {
                if (encryptor) {
                    /* encryptor activated */
                    n = IS_send(peer->first, reinterpret_cast<unsigned char *>(encryptor->public_key),
                                static_cast<size_t>(encryptor->pubk_len), IS_RKEY);
                    if (n < 0) {
                        /* if there is no space for such a small number of data, close the connection */
                        perror("error occurred while sending RKEY");
                    } else {
                        peer->second.status = ISSTATUS::IS_KEYEXCG;
                        return 0;
                    }
                } else {
                    /* no encryption */
                    n = IS_send(peer->first, nullptr, 0, IS_REDY);
                    if (n < 0) {
                        perror("error occurred sending IS_REDY");
                    } else {
                        peer->second.status = ISSTATUS::IS_READY;
                        return 0;
                    }
                }
            }
        }

        /* client */
        if (service_type == S_TYPE::Client) {
            n = IS_send(peer->first, nullptr, 0, IS_INIT);
            if (n < 0) {
                perror("error occurred while sending IS_INIT");
            } else {
                peer->second.status = ISSTATUS::IS_KEYEXCG;
                return 0;
            }
        }

    }
    return -1;
}

int
ISTransporter::keyexchangeHandler(Peers::iterator peer)
{
    /* server */
    if (peer != cons.end()) {
        int n = 0;
        auto res = IS_recv(peer->first);

        if (res.first == 0)
            /* remote peer closes the connection */
            return 1;

        if (service_type == S_TYPE::Server) {
            if (!encryptor ||
                res.first != header_len + encryptor->block_size ) {
                /* message not integrated (each request should be received in one call)
                 * or error occurred */
                if (res.first < 0) { perror("error occurred while recving AKEY");
                } else { perror("IS_AKEY bad message format"); }
            } else if (ISMTYP(&header) == IS_AKEY) {
                n = encryptor->decrypt(res.second, enc_buff, res.first - header_len);
                if (n != ISAES_KEY_LEN)
                    std::cerr << "bad aes key" << std::endl;
                else {
                    memcpy(peer->second.key, enc_buff, ISAES_KEY_LEN);
                    n = IS_send(peer->first, nullptr, 0, IS_REDY);
                    if (n != header_len) {
                        if (n < 0) { perror("error sending IS_REDY");
                        } else { std::cerr << "No space for message" << std::endl; }
                    } else {
                        peer->second.status = ISSTATUS::IS_READY;
                        return 0;
                    }
                }
            }
        }

        /* client */
        if (service_type == S_TYPE::Client) {
            if (ISMTYP(&header) == IS_RKEY && encryptor) {
                if (res.first >= header_len) {
                    if (encryptor->setKey(reinterpret_cast<const char *>(res.second), 1)) {
                        encryptor->IS_rand_bytes(peer->second.key, ISAES_KEY_LEN);
                        n = encryptor->encrypt(peer->second.key, enc_buff, ISAES_KEY_LEN);
                        int m = IS_send(peer->first, enc_buff, static_cast<size_t>(n), IS_AKEY);
                        if (m <0) {
                            perror("error sending AKEY");
                        } else {
                            /* key exchange (stage 2) */
                            peer->second.status = ISSTATUS::IS_KEYEXCG;
                            return 0;
                        }
                    } else std::cerr << "public key error" << std::endl;
                }
            }

            /* client (no key, ready for service) */
            if (ISMTYP(&header) == IS_REDY) {
                peer->second.status = ISSTATUS::IS_READY;
                // TODO IPC SEND
                if (IS_IPC_send(fd_out, peer->first, IS_IPC_CONN, nullptr, 0) < 0) {
                    // TODO ERROR
                    perror("IS_IPC_send error");
                }
                addRead(fd_in);
                if (encryptor && !encryptor->hasPubkey()) {
                    /* disable encryptor */
                    encryptor.reset(nullptr);
                }
                return 0;
            }

            std::cerr << "unknown message" << std::endl;
        }
    }
    return -1;
}

int
ISTransporter::requestHandler(Peers::iterator peer)
{
    if ( peer != cons.end() ) {

        auto res = ISAESrecv(peer);
        if (res.first == 0) {
            return 1;
        }
        if (res.first <= header_len) {
            perror("bad message format or error occurred");
        } else if ((ISMTYP(&header) == IS_RQST && service_type == S_TYPE::Server)
                || (ISMTYP(&header) == IS_RPLY && service_type == S_TYPE::Client)) {

            if (IS_IPC_send(fd_out, peer->first, IS_IPC_NORMAL, res.second, res.first-header_len)
                != res.first-header_len) {
                // TODO ERROR
            }
            return 0;
        }
    }
    return -1;
}

void
ISTransporter::removePeer(Peers::iterator peer, bool IPC_notify)
{
    if (peer != cons.end()) {
        std::cerr << "remove " << peer->first << std::endl;
        deleteRead(peer->first);
        close(peer->first);
        // TODO IPC Send
        if (IPC_notify)
            IS_IPC_send(fd_out, peer->first, IS_IPC_CLOSE, nullptr, 0);
        cons.erase(peer);
    }
}

void
ISTransporter::cleanHeader()
{
    bzero(&header, header_len);
}

int
ISTransporter::IS_IPC_send(int fd, int from, uint16_t type, unsigned char* data, size_t data_len,
sockaddr_in *addr)
{
    /* add IPC header and send */
    iovec vec[2];
    is_IPC_msg_header header;
    bzero(&header, sizeof(header));
    vec[0].iov_base = &header;
    vec[0].iov_len  = sizeof(header);
    header.type     = type;
    header.fd       = from;
    if (addr != nullptr)
        memcpy(&header.udp_addr, addr, sizeof(header.udp_addr));
    vec[1].iov_len  = data_len;
    vec[1].iov_base = data;
    return static_cast<int>(writev(fd ,vec, 2) - sizeof(header));
}

std::pair<is_IPC_msg_header, int>
ISTransporter::IS_IPC_recv(int fd, unsigned char* buff, size_t buff_size)
{
    iovec vec[2];
    is_IPC_msg_header header;
    bzero(&header, sizeof(header));
    vec[0].iov_base = &header;
    vec[0].iov_len  = sizeof(header);
    vec[1].iov_len  = buff_size;
    vec[1].iov_base = buff;
    return {header, readv(fd, vec, 2) - sizeof(header)};
}

int
ISTransporter::set_nblock(int fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0)
        return -1;
    flags |= O_NONBLOCK;
    return fcntl(fd, F_SETFL, flags);
}

int
ISTransporter::eventWrite(int fd)
{
    /* not implemented */
    return 0;
}

int
ISTransporter::eventExcept(int fd) {
    /* not implemented */
    return 0;
}

