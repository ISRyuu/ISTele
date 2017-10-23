#ifndef _ISEncryptor_HPP
#define _ISEncryptor_HPP

extern "C" {
  #include <openssl/aes.h>
  #include <openssl/pem.h>
  #include <openssl/ssl.h>
  #include <openssl/evp.h>
  #include <openssl/err.h>
  #include <openssl/bio.h>
  #include <openssl/rand.h>
  #include <stdio.h>
  #include "RMRSA.h"
}

#define ISAES_BLOCK_SIZE AES_BLOCK_SIZE
#define ISAES_KEY_LEN  (256 / 8)
#define IS_ENCRYPT AES_ENCRYPT
#define IS_DECRYPT AES_DECRYPT
  
class ISEncryptor {

public:
    ISEncryptor(const char* pri, const char* pub);
    ISEncryptor();
    bool setKey(const char *key, int pub);
    bool hasPubkey();
    bool hasPrikey();
    int encrypt(const unsigned char* text, unsigned char* cipher, ssize_t len);
    int decrypt(const unsigned char* cipher, unsigned char* clear, ssize_t len);
    ~ISEncryptor();

    char public_key[4096];
    ssize_t pubk_len;
    int size;
    int block_size;
    static void IS_cfb128_encrypt_k256(unsigned char *in, unsigned char *out,
                                size_t len, unsigned char* iv,
                                unsigned char *key, int *num, int enc);
    static void IS_rand_bytes(unsigned char* dst, size_t len);
    
private:
    RSA *pub_key, *pri_key;
};

#endif
