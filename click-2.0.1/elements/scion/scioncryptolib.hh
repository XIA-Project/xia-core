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

#ifndef SCIONCRYPTOLIB_HH_INCLUDED
#define SCIONCRYPTOLIB_HH_INCLUDED

#include<polarssl/x509.h>
#include<polarssl/rsa.h>
#include<polarssl/havege.h>
#include<polarssl/config.h>
#include<polarssl/sha1.h>
#include<polarssl/aes.h>
#include "define.hh"

// Tenma, add for AESNI
#ifdef ENABLE_AESNI
#include <intel_aesni/iaesni.h>
#include <intel_aesni/iaes_asm_interface.h>
#endif

enum CryptoErrorCode
{
    scionCryptoSuccess = 0,
    scionCryptoError = -1,
    scionCryptoObjectNULLError
    // suggestion by Tenma, might use different type of errors
};

class SCIONCryptoLib{

    public :
        /**
            @brief Generates signature for PCB
            @param uint8_t* msg
            @param uint8_t* msgSize
            @param uint8_t* output
            @param uint16_t key_len
            @param rsa_context *pkey
            @return int 

            This function generates the signature that signs the 'msg' with size
            'msgSize.' The necessary information is in the key_len and *pkey. The
            'pkey' is the private key that is required in generating signatures,
            and the key_len specifies the length of the key. 

            The output is stored in the 'output'.
            
            Returns scionCryptoSuccess when the signature generation is
            successful. Returns scionCryptoError otherwise. 

        */
        static int genSig(uint8_t* msg, uint8_t msgSize, uint8_t* output, uint16_t key_len, rsa_context *pkey);

        /**
            @brief Verifies the singautre of the msg. 
            @param uint8_t* msg
            @param uint8_t* sig
            @param int msgSize
            @param rsa_context *pupKey
            @return int errorCode

            This function verifies the signature 'sig' agianst the msg. If the
            signature corresponds to the msg using the pubKey, the function
            returns scionCryptoSuccess. Otherwise, it returns scionCryptoError. 
        */
        static int verifySig(uint8_t* msg, uint8_t* sig, int msgSize, rsa_context *pubKey); 

        /**
            @brief Generates MAC 
            @param uint8_t* msg
            @param uint16_t msgSize
            @param aes_context* ctx
            @return uint32_t MAC

            This function generates MAC of the msg using the secret key 'ctx'. The
            generated MAC value is returned. 
        */
#ifdef ENABLE_AESNI
		static uint32_t genMAC(uint8_t* msg, uint16_t msgSize, keystruct* ks);
#else
		static uint32_t genMAC(uint8_t* msg, uint16_t msgSize, aes_context* ctx);
#endif
        
        /**
            @brief Extracts serial number from a x509_cert
            @param x509_cert* crt The cerificate file.
            @param char* p The pointer where the result is stored. 
        */
        static int GetSerialFromCert(x509_cert* crt, char* p);

	static void PrintPolarSSLError(int code);
};
#endif
