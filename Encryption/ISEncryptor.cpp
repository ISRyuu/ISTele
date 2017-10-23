#include "ISEncryptor.hpp"

extern "C" {
    #include <fcntl.h>
    #include <unistd.h>
}

ISEncryptor::ISEncryptor()
{
  pri_key = pub_key = nullptr;
}

ISEncryptor::ISEncryptor(const char* pri, const char* pub)
{
    int fd = open(pub, O_RDONLY);
    if ( fd < 0 ) {
        perror("cannot open public key file");
        exit(1);
    }
    pubk_len = read(fd, public_key, 4096);
    if ( pubk_len < 0 ) {
        perror("public key read error");
        exit(1);
    }
    pub_key = loadRSAfromfile(const_cast<char*>(pub), 1);
    pri_key = loadRSAfromfile(const_cast<char*>(pri), 0);
    if (!(pub_key && pri_key)) {
        fprintf(stderr, "cannot creat RSA key\n");
        exit(1);
    }
    /*
     ![Manual](https://www.openssl.org/docs/man1.0.2/crypto/RSA_public_encrypt.html)
     flen must be less than RSA_size(rsa) - 11 for the PKCS #1 v1.5 based padding modes, less than RSA_size(rsa) - 41 for
     RSA_PKCS1_OAEP_PADDING and exactly RSA_size(rsa) for RSA_NO_PADDING.
     */
    block_size = RSA_size(pub_key);
    size = block_size - 11;
}

bool
ISEncryptor::setKey(const char *key, int pub)
{
  if (pub) {
    if (pub_key) RSA_free(pub_key);
    pub_key = createRSA((unsigned char*)key, 1);
    if (pub_key == NULL) return false;
    block_size = RSA_size(pub_key);
    size = block_size - 11;
    return true;
  } else {
    if (pri_key) RSA_free(pri_key);
    pri_key = createRSA((unsigned char*)key, 0);
    return pri_key != NULL;
  }
  return false;
}

int
ISEncryptor::encrypt(const unsigned char *text, unsigned char *cipher, ssize_t len)
{
  size_t offset_ch = 0; 	// offset of cipher
  size_t offset_cl = 0;		// offset of cleartext
  
  while (len > 0) {
    int n = public_encrypt(const_cast<unsigned char*>(text) + offset_cl,
			   (len > size ? size : len),
			   pub_key, cipher + offset_ch);
    if (n < 0) {
      print_error((char*)"error occur when encryting");
      return -1;
    }
    len -= size;
    offset_ch += n;
    offset_cl += size;
  }
  /* return the cipher length */
  /*  NOT the length of how many bytes have been encrypted. */
  return static_cast<int>(offset_ch);
}

int
ISEncryptor::decrypt(const unsigned char *cipher, unsigned char *clear, ssize_t len)
{
  size_t offset = 0;
  size_t t = 0;
  
  while (len > 0) {
    int n = private_decrypt(const_cast<unsigned char*>(cipher) + offset,
			    size + 11,
			    pri_key, clear + t);
    if (n < 0) {
      print_error((char*)"error occur when decrypting");
      return -1;
    }
    len -= size + 11;
    offset += size + 11;
    t += n;
  }
  /* return clear text length */
  return static_cast<int>(t);
}

bool
ISEncryptor::hasPubkey()
{
    return pub_key != nullptr;
}

bool
ISEncryptor::hasPrikey()
{
    return pri_key != nullptr;
}

ISEncryptor::~ISEncryptor()
{
  if (pub_key) RSA_free(pub_key);
  if (pri_key) RSA_free(pri_key);
}


void
ISEncryptor::IS_rand_bytes(unsigned char *dst, size_t len)
{
  RAND_bytes(dst, len);
}

void
ISEncryptor::IS_cfb128_encrypt_k256(unsigned char *in, unsigned char *out, size_t len, unsigned char* iv, unsigned char *key, int *num, int enc)
{
    unsigned char keybit[ISAES_KEY_LEN]; // 256 bits;
    bzero(keybit, ISAES_KEY_LEN);
    memcpy(keybit, key, ISAES_KEY_LEN);
    
    AES_KEY e_key;
    AES_set_encrypt_key(keybit, 256, &e_key);
    
    unsigned char iv_ec[AES_BLOCK_SIZE];
    memcpy(iv_ec, iv, AES_BLOCK_SIZE);
    
    AES_cfb128_encrypt(in, out, len, &e_key, iv_ec, num, enc);
    
    memcpy(iv, iv_ec, AES_BLOCK_SIZE);
}

