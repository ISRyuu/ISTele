//
//  RMRSA.c
//  RSA_SAMPLE
//
//  Created by Kevin on 2017/3/18.
//  Copyright © 2017年 Kevin. All rights reserved.
//

#include <stdio.h>
#include <unistd.h>
#include "RMRSA.h"

void
print_error(char *msg) {
    char error[2048] = {0};
    ERR_load_crypto_strings();
    ERR_error_string(ERR_get_error(), error);
    printf("%s: %s\n", msg, error);
}

RSA*
loadRSAfromfile(char *file, int pub) {
    FILE *fp = fopen(file, "rb");
    if (fp == NULL) {
        perror("cannot open file");
        return NULL;
    }
    RSA *rsa = NULL;
    if (pub)
        rsa = PEM_read_RSA_PUBKEY(fp, &rsa, NULL, NULL);
    else
        rsa = PEM_read_RSAPrivateKey(fp, &rsa, NULL, NULL);

    if (rsa == NULL)
       print_error("failed to create RSA.\n");
  
    return rsa;
}

RSA*
createRSA(unsigned char* key, int pub) {
    RSA *rsa = NULL;
    BIO *keybio;
    keybio = BIO_new_mem_buf(key, -1);
    if (keybio == NULL) {
        print_error("failed to create key BIO.\n");
        return NULL;
    }
    if (pub)
        rsa = PEM_read_bio_RSA_PUBKEY(keybio, &rsa, NULL, NULL);
    else
        rsa = PEM_read_bio_RSAPrivateKey(keybio, &rsa, NULL, NULL);
    
    if (rsa == NULL)
        print_error("failed to create RSA.\n");
    
    BIO_free(keybio);
    return rsa;
}

int
public_encrypt(unsigned char *data, int data_len, RSA *rsa, unsigned char* cipher) {
    return RSA_public_encrypt(data_len, data, cipher, rsa, PADDING);
}

int
private_decrypt(unsigned char *cipher, int data_len, RSA *rsa, unsigned char* cleartext) {
    return RSA_private_decrypt(data_len, cipher, cleartext, rsa, PADDING);
}

