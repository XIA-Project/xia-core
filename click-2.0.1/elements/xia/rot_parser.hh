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

#ifndef ROT_PARSER_HH_
#define ROT_PARSER_HH_
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <map>
#include "tinyxml2.hh"

using namespace std;
using namespace tinyxml2;


enum ROTlogyError{
  ROTParseNoError=0,  // no errors
  ROTParseFail,  // parsing error
  ROTVerifyFail,  // verification error
  ROTNotExist  // rot is missing
};

/**
    @brief coreAD structure of ROT parser
    The structure with Core AD's certificate information.
*/
struct CoreAD{
  /** ADID of the Core AD */
  uint64_t aid;
  /** Certificate Length */
  int certLen;
  /** Certificate in raw bytes*/
  char* certificate;

};

/**
    @brief ROT Structure
    This struct represents the ROT file. The raw ROT file will be parsed
    by the rot parser into this structure.
    The detail of the ROT can be found in the SCION Implementation document.
*/
struct ROT{
    /** ROT version */
  int version;
    /** Expiration Time */
  struct tm expiredate;
    /** Issue Time */
  struct tm issuedate;
    /** Policy Threshold */
  int policyThreshold;
    /** Certificate Threshold */
  int certThreshold;
    /** Number of Core AD */
  int numCoreAD;
    /** TDID that owns this ROT*/
  uint64_t TDID;
    /** List of Core ADs */
  map<uint64_t, CoreAD> coreADs;
};

/**
    @brief Root Of Trust(ROT) file parsing element class.

    This class object is responsible for parsing the ROT file from the disk drive
    and load the information into the meomory. The given ROT file is in XML format
    and the initial parsing is done by the "tinyxml" library. When the initial
    parsing is done, then this element extracts the necessary information from the
    tinyxml parsing element, and passes the information to the calling function.

    Also, prior to passing certificates and public keys to the calling functions,
    this ROT parser verifies the validity of the ROT file by checking the
    signature of ROT. If this verification passes, then the calling function will
    get the certificates and other values from the ROT. If the verification fails,
    no information is passed to any servers and routers.
*/
class ROTParser{
 public :
    /**
        @brief Constructur of the ROT parser

        This constructor instantiates the ROT parsing element with the m_bIsInit
        value as false. This value will remain as fauls until the loadROTFile() or
        loadROT function is called.
    */
  ROTParser();
  // load methods, from file or char* buffer
  /**
        @brief Loads the ROT file from the disk drive.
        @param const char* path The path where the ROT file is located in the
        disk.
        @return Returns ROTNotExist on error, and ROTParseNoError when success.

        This function opens a file stream of the ROT file that is located at the
        'path'. The parameter 'path' is the relative path to the ROT file and this
        path is defined (most of the time) in the .conf file.

        When the file stream is successfully opened, the function will return
        ROTParseNoError. Otherwise, it will return ROTNotExist.
    */
  int loadROTFile(const char* path);
    /**
        @brief Loads the ROT file as constant string.
        @param const char* rotFile The ROT file in the format of char*.
        @return Returns ROTNotExist on error. ROTParseNoError on success.

        This function loads the ROT as a c string (char*). Instead of opening a
        file from the disk, the ROT file is passed in as a string. This string is
        directly passed to the tinyxml library to be parsed.

        When the initial parsing is done, the funciton will return
        ROTParseNoError. Otherwise returns ROTNotExist.
    */
  int loadROT(const char* rotFile);
  // Verify ROT file itself based on attached signatures and public keys
    /**
        @brief Verifies the ROT file
        @param const ROT &rot The ROT structure that holds the information of a
        parsed ROT file.
        @return Returns ROTParseFail on error. ROTParseNoError on success.

        This is a verification function of ROT. After the ROT file or the ROT
        String is successfully parsed in to a structure, this function validates
        the ROT by checking the signature and the public key. When the signature
        verification passes the function will return no error.

        @note This function must be called after the parsing is successfully done.
        Otherwise, the behavior is unknown and the program may crash.
    */
  int verifyROT(const ROT &rot);
  // parse ROT file or data buffer chunk
  // parsed vaiables are stored in rot element
    /**
        @brief Parses the ROT into the ROT structure.
        @param ROT &rot ROT structure that will hold the parsed ROT info.
        @return Returns ROTParseNoError on success. Error Code on error.

        This function stores the ROT information into the rot structure given as
        the parameter. Once the file or the ROT string is succesfully loaded into
        the tinyxml element, this function extracts the information from the
        tinyxml element into the ROT structure.

        After extracting all the information from the tinyxml, this function calls
        the verifyROT() function to verify the ROT information. Please refer to
        the verifyROT function for more details about the ROT verification.

        If the verification passes, this funciton returns ROTParseNoError
        indicating that the all the ROT parsing process is complete. If not, (in
        any error code from verifyROT()), the function returns an error code and
        the structure is removed.
    */
  int parse(ROT &rot);
 private:
    /** @brief true if parser is initiated, false otherwise */
  bool m_bIsInit;
    /** ROT file path */
  string m_sfilePath;
    /** XML Document of the ROT file */
  XMLDocument doc;
    /** Mapping between Certificate owners and the lenghts of the certificates */
  map<uint64_t, int> sigLengths;
    /** Mapping between Certificate owners and the certificates */
  map<uint64_t, uint8_t*> signatures;
};
#endif
