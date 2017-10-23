//
//  RMRSA.h
//  RSA_SAMPLE
//
//  Created by Kevin on 2017/3/18.
//  Copyright © 2017年 Kevin. All rights reserved.
//

#ifndef _RMRSA_H
#define _RMRSA_H

#include <openssl/aes.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/bio.h>

#define PADDING RSA_PKCS1_PADDING

RSA *loadRSAfromfile(char *file, int pub);
RSA *createRSA(unsigned char* key, int pub);
int public_encrypt(unsigned char *data, int data_len, RSA *rsa, unsigned char* cipher);
int private_decrypt(unsigned char *chiper, int data_len, RSA *rsa, unsigned char* cleartext);
void print_error(char *msg);


#endif
