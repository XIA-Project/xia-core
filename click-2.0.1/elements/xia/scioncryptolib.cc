/*
** Copyright 2013 Carnegie Mellon University / ETH Zurich
** 
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
** 
** http://www.apache.org/licenses/LICENSE-2.0 
** 
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*/

#include <polarssl/rsa.h>
#include <polarssl/x509.h>
#include <polarssl/aes.h>
#include <polarssl/config.h>
#include <polarssl/sha1.h>
#include <click/config.h>
#include <time.h>
#include "scioncryptolib.hh"
#include <stdio.h>
#include <string.h>
#include <errno.h>


CLICK_DECLS

void SCIONCryptoLib::PrintPolarSSLError(int code)
{
    switch(code)
    {
        case POLARSSL_ERR_RSA_BAD_INPUT_DATA:
            printf("CRYPTO ERROR:Bad input parameters to function.\n");
            break;
        case POLARSSL_ERR_RSA_INVALID_PADDING:
            printf("CRYPTO ERROR:Input data contains invalid padding and is rejected.\n");
            break;
        case POLARSSL_ERR_RSA_KEY_GEN_FAILED:
            printf("CRYPTO ERROR:Something failed during generation of a key.\n");
            break;
        case POLARSSL_ERR_RSA_KEY_CHECK_FAILED:
            printf("CRYPTO ERROR:Key failed to pass the libraries validity check.\n");
            break;
        case POLARSSL_ERR_RSA_PUBLIC_FAILED:
            printf("CRYPTO ERROR:The public key operation failed.\n");
            break;
        case POLARSSL_ERR_RSA_PRIVATE_FAILED:
            printf("CRYPTO ERROR:The private key operation failed.\n");
            break;
        case POLARSSL_ERR_RSA_VERIFY_FAILED:
            printf("CRYPTO ERROR:The PKCS#1 verification failed.\n");
            break;
        case POLARSSL_ERR_RSA_OUTPUT_TOO_LARGE:
            printf("CRYPTO ERROR:The output buffer for decryption is not large enough.\n");
            break;
        case POLARSSL_ERR_RSA_RNG_FAILED:
            printf("CRYPTO ERROR:The random generator failed to generate non-zeros.\n");
            break;
    }
}

/*
    SCIONCryptoLib::genSig
    - generates signature for the given message and the private key.
    - usses polarssl rsa PKCS1 function
*/
int SCIONCryptoLib::genSig(uint8_t* msg, uint8_t msgSize, uint8_t* output, uint16_t sigLen, rsa_context* pkey){

	//gererate hash
	unsigned char sha1Hash[SHA1_SIZE];
	sha1(msg, msgSize, sha1Hash);
	
	// TODO: No RNG here!!
	int err = rsa_pkcs1_sign(pkey, 0, 0, RSA_PRIVATE, SIG_RSA_SHA1, SHA1_SIZE, sha1Hash, output);
	
	if(err){
        	printf("ERROR CREATING SIGNATURE.\n");
		PrintPolarSSLError(err);
        	return scionCryptoError;    
    	} 
	return scionCryptoSuccess;
}



/*
    SCIONCryptoLib::verfiySig
    - verification code of the given signature.
    - uses polarssl library rsa PKCS1 function 
*/
int SCIONCryptoLib::verifySig(uint8_t* msg, uint8_t* sig, int msgSize, rsa_context *pubKey){

	//creates the hash
	unsigned char sha1Hash[SHA1_SIZE];
	sha1(msg, msgSize, sha1Hash);

	// Verify the signature
	int err = rsa_pkcs1_verify(pubKey, RSA_PUBLIC, SIG_RSA_SHA1, SHA1_SIZE, sha1Hash, sig);
	if(err){
		printf("ERROR VERIFYING SIGNATURE.\n");
		PrintPolarSSLError(err);
		return scionCryptoError;
	}
	return scionCryptoSuccess;
}

/*
    SCIONCryptoLib::genMAC
    - generates MAC using aes cbc MAC. 
*/
uint32_t SCIONCryptoLib::genMAC(uint8_t* msg, uint16_t msgSize, aes_context* ctx)
{
    uint16_t r = msgSize%16;
    if(r!=0){
        msgSize+=(16-r);
    }
    uint8_t input[msgSize];
    memset(input, 0, msgSize);
	//SL:padding with 1000...0
    //memcpy(input, msg, msgSize-((16-r)%16));
    memcpy(input, msg, msgSize-(16-r));
	if(r){
		input[msgSize-(16-r)] = 0x80;
	}
    memset(input+(msgSize-(16-r))+1,0,(16-r)-1);
   
    uint8_t buf[msgSize];
    memset(buf, 0, msgSize);
    
    unsigned char iv[16];
    memset(iv, 0, 16);

	aes_crypt_cbc(ctx, SCION_AES_ENCRYPT, msgSize, iv, input, buf);
    uint32_t* ret = (uint32_t*)(buf+msgSize-4);
    return *ret;
}

int SCIONCryptoLib::GetSerialFromCert(x509_cert* crt, char* p)
{
	int ret = 0, i = 0, j = 0;
	char buf[29];	// max length is 20 bytes and : seperators for 2 bytes
	
	if(p==NULL||crt==NULL) return scionCryptoObjectNULLError;
	
	ret = x509parse_serial_gets( (char *)buf, sizeof(buf), &crt->serial);
	
	if( ret != -1 )
	{
	  for (i = 0; i < 29; i++)
	  {
		if (buf[i] != ':'){
			strncpy (&p[j], (const char*)&buf[i], 1);
			j++;
		}
	  }
	}
	
	return strlen(p);
}




CLICK_ENDDECLS
ELEMENT_PROVIDES(SCIONCryptoLib)
ELEMENT_LIBS(-lpolarssl)
