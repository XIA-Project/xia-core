/*
** Copyright 2011 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**    http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
/*!
  @file Xkeys.h
  @brief header for internal helper functions
*/
#ifndef _Xkeys_h
#define _Xkeys_h

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <sys/types.h>
#include <sys/stat.h>
//#include "Xsecurity.h"
#ifndef XIA_SHA_DIGEST_STR_LEN
#define XIA_SHA_DIGEST_STR_LEN SHA_DIGEST_LENGTH*2+1
#endif

#ifdef __cplusplus
extern "C" {
#endif

void sha1_hash_to_hex_string(unsigned char *digest, int digest_len, char *hex_string, int hex_string_len);

// Remove keys associated with the given SID
extern int XremoveSID(const char *sid);

// Create a key pair and return SID based on hash of pubkey
extern int XmakeNewSID(char *randomSID, int randomSIDlen);

// Check if keys matching the given SID exist
extern int XexistsSID(const char *sid);

#ifdef __cplusplus
}
#endif
#endif
