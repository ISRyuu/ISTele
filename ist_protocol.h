//
// Created by Kevin on 2017/9/19.
//

#ifndef ISTELE_IST_PROTOCOL_H
#define ISTELE_IST_PROTOCOL_H

#include <sys/types.h>
#include <netinet/in.h>
#include <stdint.h>

#define IS_INIT 0x001 /* init message   */
#define IS_RQST 0x002 /* request        */
#define IS_RPLY 0x003 /* reply          */
#define IS_RKEY 0x004 /* ras public key */
#define IS_AKEY 0x005 /* aes key        */
#define IS_UDPP 0x006 /* UDP package    */
#define IS_REDY 0x008 /* at your service*/

#define IS_IPC_CLOSE  0x0001  /* close the connection   */
#define IS_IPC_NORMAL 0x0002  /* normal message (tcp)   */
#define IS_IPC_UDP    0x0003  /* message from udp       */
#define IS_IPC_CONN   0x0004  /* successfully connected */

#define IS_TELE_LOGIN    0x0001   /* login             */
#define IS_TELE_ADDR_ADV 0x0002   /* address advertise */
#define IS_TELE_ADDR_SOL 0x0003   /* address solicate  */
#define IS_TELE_CLOSE    0x0004   /* close the tele    */
#define IS_TELE_CALL     0x0005   /* call              */
#define IS_TELE_ERR      0x0006   /* error */

#define IS_ERROR_BUSY    0x0001
#define IS_ERROR_OFFL    0x0002  /* offline */
#define IS_ERROR_INTL    0x0003  /* internal error */

/* notification for user */
typedef int16_t  IS_USER_NOTIF; /* 16 bits */
#define IS_USER_READY    0x0001 /* connection is ready */
#define IS_USER_HANUP    0x0002 /* hang up （close)    */

struct is_tele_trans_header {
    /* transport layer header */
    uint16_t _type_len;
    uint16_t _reserve;
    uint8_t iv[128 / 8]; /* AES_BLOCK_SIZE 128 bits */
#define ISMTYP(m)         ((m)->_type_len >> 12)
#define ISMLEN(m)         ((m)->_type_len & 0x0fff)
#define SET_ISMLEN(m, l)  ((m)->_type_len |= ((l) & 0x0fff))
#define SET_ISMTYP(m, t)  ((m)->_type_len |= ((t) << 12))
};

struct is_tele_msg
{
    /* router layer header */
    uint16_t type;
    uint16_t error;
    uint32_t id;
    uint32_t seq_sess; /* sequence number or session number */
};

struct is_IPC_msg_header {
    /* IPC msg header */
    uint16_t type;
    uint16_t _padding;
    int32_t fd;
    sockaddr_in udp_addr;
};

#define header_hton(header) \
do { \
    (header)->id       = htonl((header)->id);\
    (header)->error    = htons((header)->error);\
    (header)->type     = htons((header)->type);\
    (header)->seq_sess = htonl((header)->seq_sess);\
} while(0)

#define header_ntoh(header) \
do { \
    (header)->id       = ntohl((header)->id);\
    (header)->error    = ntohs((header)->error);\
    (header)->type     = ntohs((header)->type);\
    (header)->seq_sess = ntohl((header)->seq_sess);\
} while (0)

#endif //ISTELE_IST_PROTOCOL_H
