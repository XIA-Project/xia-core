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


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <click/config.h>
#include <polarssl/x509.h>
#include <polarssl/config.h>
#include <polarssl/sha1.h>
#include <polarssl/havege.h>
#include <polarssl/base64.h>
#include "scioncryptolib.hh"
#include "rot_parser.hh"

CLICK_DECLS
using namespace tinyxml2;

ROTParser::ROTParser()
  : m_bIsInit(false) {
    // Nothing to do.
  }


/*
 * loadROTFile
 * Loads ROT from a given path read from config file
 */
int ROTParser::loadROTFile(const char* path) {
  if (doc.LoadFile(path)) {
    printf("Fatal Error: Error Loading ROT File.\n");
    return ROTNotExist;
  }
  m_bIsInit = true;
  m_sfilePath = path;
  return ROTParseNoError;
}


/*
 * loadROTData
 * Loads ROT from a string buffer
 */
int ROTParser::loadROT(const char* rot) {
  if (doc.Parse(rot)) {
          printf("Fatal Error: Error Loading ROT Stucture.\n");
    return ROTNotExist;
  }
  m_bIsInit = true;
  return ROTParseNoError;
}



int ROTParser::parse(ROT &rot) {
  if (!m_bIsInit) {
    printf("ROT file not initiated.\n");
    return ROTNotExist;
  }

  // read <header>
  XMLElement *ptr = doc.RootElement()->FirstChildElement("header");
  // use stringstream here, could be replaced by more efficient design
  stringstream s;

  s<<ptr->FirstChildElement("version")->GetText()<<" ";
  s<<ptr->FirstChildElement("TDID")->GetText()<<" ";
  s<<ptr->FirstChildElement("policyThreshold")->GetText()<<" ";
  s<<ptr->FirstChildElement("certificateThreshold")->GetText();

  s>>rot.version;
  s>>rot.TDID;
  s>>rot.policyThreshold;
  s>>rot.certThreshold;

  // read expire and issue dates
  memset(&rot.issuedate, 0, sizeof(struct tm));
  memset(&rot.expiredate, 0, sizeof(struct tm));
  strptime(ptr->FirstChildElement("issueDate")->GetText(),
           "%b %d %H:%M:%S %Y", &(rot.issuedate));
  strptime(ptr->FirstChildElement("expireDate")->GetText(),
           "%b %d %H:%M:%S %Y %Z", &rot.expiredate);

  // read the list of core ADs; i.e., <coreADs>
  ptr = ptr->NextSiblingElement();
  XMLElement *ptr2 = ptr->FirstChildElement(); // coreAD

  rot.numCoreAD = 0;
  while (ptr2 != NULL) {
    CoreAD *newCoreAD = new CoreAD;
    stringstream s2;
    s2<<ptr2->FirstChildElement("AID")->GetText()<<" ";
    s2<<ptr2->FirstChildElement("len")->GetText();

    s2>>newCoreAD->aid;
    s2>>newCoreAD->certLen;

    // get certificates
    const char* cert = ptr2->FirstChildElement("cert")->GetText();
    newCoreAD->certificate = (char*)malloc(newCoreAD->certLen);
    memcpy(newCoreAD->certificate, cert, newCoreAD->certLen);

    // remove outdated AD information if it exists
    map<uint64_t,CoreAD>::iterator it;
    if ((it = rot.coreADs.find(newCoreAD->aid)) != rot.coreADs.end()) {
      free(it->second.certificate);
      rot.coreADs.erase(it);
    }

    rot.coreADs[newCoreAD->aid] = *newCoreAD;
    delete newCoreAD;
    ptr2 = ptr2->NextSiblingElement();
    rot.numCoreAD++;
  }
  // then check # of signature
  if (rot.numCoreAD<rot.certThreshold) {
    printf("Fatal Error: insufficient # of certificates in ROT.\n");
    return ROTParseFail;
  }
  return ROTParseNoError;
}


int ROTParser::verifyROT(const ROT& rot) {

  if (!m_bIsInit) {
        printf("ROT file not initiated.\n");
    return ROTParseFail;
  }

  XMLElement *ptr = doc.RootElement()->FirstChildElement("header");
  ptr = ptr->NextSiblingElement();
  ptr = ptr->NextSiblingElement();
  // coreAD elements
  XMLElement *ptr2 = ptr->FirstChildElement("coreAD");

  while (ptr2!=NULL) {
    uint64_t aid = strtoull(ptr2->FirstChildElement("AID")->GetText(), NULL, 10);
    // get sig length
    int sigLen = atoi(ptr2->FirstChildElement("len")->GetText());
    sigLengths[aid] = sigLen;
    uint8_t* ptr = (uint8_t*) malloc(sigLen);
    // insert signature into sugnatures list
    signatures.insert(pair<uint64_t, uint8_t*>(aid, ptr));
    memcpy(signatures.find(aid)->second, ptr2->FirstChildElement("sign")->GetText(), sigLen);
    ptr2 = ptr2->NextSiblingElement();
  }

  // compute the sha1 hash for remindering rot file first
  unsigned char sha1Hash[20] = {};
  memset(sha1Hash, 0, 20);
  // read file
  FILE* rotFile = fopen(m_sfilePath.c_str(), "r");
  if (rotFile == NULL) {
    printf("Fatal Error: error opening rot file.\n");
    return ROTNotExist;
  }
  fseek(rotFile, 0, SEEK_END);
  int rotLen = (int) ftell(rotFile);
  rewind(rotFile);

  char *msg = (char*)malloc(rotLen);
  int msgLen, offset = 0;
  char buffer[1024];

  while (!feof(rotFile)) {
    fgets(buffer, 1024, rotFile);
    strncpy(msg+offset, buffer, strlen(buffer));
    offset += strlen(buffer);
    if (strstr(buffer, "</coreADs>") != NULL) {
          // signature is created up to this line
      break;
    }
  }
  msgLen = offset;
  fclose(rotFile);
  sha1((const unsigned char*)msg, msgLen, sha1Hash);

  // then check # of signature = # of certs
  if (signatures.size() != rot.numCoreAD) {
    printf("ROT Verification Failed: # of signatures is not equal to # of certificates.\n");
    return ROTVerifyFail;
  }

  map<uint64_t, uint8_t*>::iterator itr;
  uint8_t output[512];
  int ret = 0;

  // signature verification one by one
  for (itr = signatures.begin(); itr!=signatures.end(); ++itr) {
    // decode base64 encoding signatures
          size_t len = 512;
    memset(output, 0, 512);
    ret = base64_decode(output, &len, itr->second, sigLengths.find(itr->first)->second);
    if (ret == POLARSSL_ERR_BASE64_BUFFER_TOO_SMALL) {
      printf("decode error, code = %d, len = %d\n", ret ,len);
      return ROTVerifyFail;
    }

    // read certificates objects
    x509_cert cert;
    memset(&cert, 0, sizeof(x509_cert));
    ret = x509parse_crt(&cert,
      (const unsigned char*)rot.coreADs.find(itr->first)->second.certificate,
      rot.coreADs.find(itr->first)->second.certLen);
    // read error?
    if (ret < 0) {
      printf("ERR: failed!  x509parse_crt returned %d\n", ret);
      x509_free(&cert);
      return ROTVerifyFail;
    }
    x509_cert crt;
    x509_cert *cur = &cert;
    // Check the signature by certificates issued by ADs in TDC
    while (cur != NULL) {
      if ((ret = rsa_pkcs1_verify(&cur->rsa, RSA_PUBLIC, SIG_RSA_SHA1, 20,
                                  sha1Hash, output)) != 0) {
            printf("ERR: signature verification failed.\n");
        SCIONCryptoLib::PrintPolarSSLError(ret);
        x509_free(&cert);
        // SL: this is just for testing ROT version change...
        // Temporarily returns true...
        #ifdef _SL_ROT_TEST
        return ROTParseNoError;
        #else
        return ROTVerifyFail;
        #endif
      }
      cur = cur->next;
    }
    x509_free(&cert);
  }
  printf("ROT Verification Done.\n");
  return ROTParseNoError;
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(ROT_Parser)

