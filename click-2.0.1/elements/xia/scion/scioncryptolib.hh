#ifndef SCIONCRYPTOLIB_HH_INCLUDED
#define SCIONCRYPTOLIB_HH_INCLUDED

#include<polarssl/x509.h>
#include<polarssl/rsa.h>
#include<polarssl/havege.h>
#include<polarssl/config.h>
#include<polarssl/sha1.h>
#include<polarssl/aes.h>
#include "define.hh"

enum CryptoErrorCode
{
    scionCryptoSuccess = 0,
    scionCryptoError = -1,
    scionCryptoObjectNULLError
    // suggestion by Tenma, might use different type of errors
};

class SCIONCryptoLib{

    public :
        static int genSig(uint8_t* msg, uint8_t msgSize, uint8_t* output, uint16_t key_len, rsa_context *pkey);

        static int verifySig(uint8_t* msg, uint8_t* sig, int msgSize, rsa_context *pubKey); 

        static uint32_t genMAC(uint8_t* msg, uint16_t msgSize, aes_context* ctx);
        
        // for extract serial number from a x509_cert
        // return value is the length of identifier, and result is stored in p
        static int GetSerialFromCert(x509_cert* crt, char* p);

	static void PrintPolarSSLError(int code);
};
#endif
