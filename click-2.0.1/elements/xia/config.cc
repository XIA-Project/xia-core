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

#include <stdio.h>
#include <string.h>
#include <click/config.h>
#include "config.hh"

CLICK_DECLS

Config::Config()
    : m_iRegisteredPaths(NUM_REG),
      m_iPTime(PROP_TIME),
      m_iRTime(REG_TIME),
      m_iResetTime(RESET_TIME),
      m_iNumInterface(0),
      m_iLogLevel(LOG_LEVEL),
      m_iIsRegister(0),
      m_iCore(0),
      m_iPCBQueueSize(BEACON_QUEUE_SIZE),
      m_iPSQueueSize(PS_QUEUE_SIZE),
      m_uAdAid(0),
      m_uTdAid(0),
      m_uAddrBS(0),
      m_uAddrPS(0),
      m_uAddrCS(0),
      m_iPCBGenPeriod(0) {
  memset(m_csPrivateKeyFile, 0, MAX_FILE_LEN+1);
  memset(m_csCertificateFile, 0, MAX_FILE_LEN+1);
  memset(m_csROTFile, 0, MAX_FILE_LEN+1);
  memset(m_csMasterOfgKey, 0,  MASTER_KEY_LEN+1);
  memset(m_csLogFile, 0, MAX_FILE_LEN+1);
  memset(m_csPSLogFile, 0, MAX_FILE_LEN+1);
  memset(m_csCSLogFile, 0, MAX_FILE_LEN+1);
  memset(m_csBSLogFile, 0, MAX_FILE_LEN+1);
  memset(m_csSLogFile, 0, MAX_FILE_LEN+1);
  memset(m_csMasterADKey, 0, MASTER_KEY_LEN+1);
}

/* Parse a configuration file.  Return true if parsing succeeds */
bool Config::parseConfigFile(char *fileName) {
  // open the config file
  FILE *inputfile;
  if ((inputfile = fopen(fileName, "r")) == NULL) {
    click_chatter("Failed to open the config file!\n");
    return false;
  }

  // go through each line and try to interpret the line
  char buffer[MAXLINELEN];
  char configOption[MAXLINELEN];
  while (!feof(inputfile)) {
    memset(buffer, 0, MAXLINELEN);
    if (fgets(buffer, MAXLINELEN, inputfile) == NULL) {
      break;
    }
    // while(fgets(buffer, MAXLINELEN, inputfile)!=NULL) {

    // If we have a valid line...
    if (buffer[0] != '#') {
      // Read in the config option name
      memset(configOption, 0, MAXLINELEN);
      if (sscanf(buffer, "%s", configOption) != 1) {
        click_chatter("Failed to find a proper config option\n");
        fclose(inputfile);
        return false;
      }

      // Now, determine which config option matches
      if (!strcmp(configOption, "BeaconServer")) {
        sscanf(buffer, "BeaconServer %llu", &m_uAddrBS);
      } else if (!strcmp(configOption, "CertificateServer")) {
        sscanf(buffer, "CertificateServer %llu", &m_uAddrCS);
      } else if (!strcmp(configOption, "PathServer")) {
        sscanf(buffer, "PathServer %llu", &m_uAddrPS);
      } else if (!strcmp(configOption, "NumRegisteredPaths")) {
        sscanf(buffer, "NumRegisteredPaths %d", &m_iRegisteredPaths);
      } else if (!strcmp(configOption, "NumShortestUPs")) {
        sscanf(buffer, "NumShortestUPs %d", &m_iShortestUPs);
      } else if (!strcmp(configOption, "PrivateKeyFile")) {
        sscanf(buffer, "PrivateKeyFile %s", m_csPrivateKeyFile);
      } else if (!strcmp(configOption, "CertificateFile")) {
        sscanf(buffer, "CertificateFile %s", m_csCertificateFile);
      } else if (!strcmp(configOption, "ROT")) {
        sscanf(buffer, "ROT %s", m_csROTFile);
      } else if (!strcmp(configOption, "MasterOFGKey")) {
        sscanf(buffer, "MasterOFGKey %s", m_csMasterOfgKey);
      } else if (!strcmp(configOption, "RegisterTime")) {
        sscanf(buffer, "RegisterTime %d", &m_iRTime);
      } else if (!strcmp(configOption, "PropagateTime")) {
        sscanf(buffer, "PropagateTime %d", &m_iPTime);
      } else if (!strcmp(configOption, "ResetTime")) {
        // SL: what is the reset time for???
        // Currently, it's unused.
        sscanf(buffer, "ResetTime %d", &m_iResetTime);
      } else if (!strcmp(configOption, "ADAID")) {
        sscanf(buffer, "ADAID %llu", &m_uAdAid);
      } else if (!strcmp(configOption, "TDID")) {
        sscanf(buffer, "TDID %llu", &m_uTdAid);
      } else if (!strcmp(configOption, "InterfaceNumber")) {
        sscanf(buffer, "InterfaceNumber %d", &m_iNumInterface);
      } else if (!strcmp(configOption, "Interface")) {
        uint16_t ifid = 0;
        sscanf(buffer, "Interface %u", &ifid);
        ifid_set.push_back(ifid);
      } else if (!strcmp(configOption, "Log")) {
        sscanf(buffer, "Log %s", &m_csLogFile);
      } else if (!strcmp(configOption, "CSLog")) {
        sscanf(buffer, "CSLog %s", &m_csCSLogFile);
      } else if (!strcmp(configOption, "PSLog")) {
        sscanf(buffer, "PSLog %s", &m_csPSLogFile);
      } else if (!strcmp(configOption, "BSLog")) {
        sscanf(buffer, "BSLog %s", &m_csBSLogFile);
      } else if (!strcmp(configOption, "LogLevel")) {
        sscanf(buffer, "LogLevel %d", &m_iLogLevel);
      } else if (!strcmp(configOption, "RegisterPath")) {
        sscanf(buffer, "RegisterPath %d", &m_iIsRegister);
      } else if (!strcmp(configOption, "Core")) {
        sscanf(buffer, "Core %d", &m_iCore);
      } else if (!strcmp(configOption, "SLog")) {
        sscanf(buffer, "SLog %s", &m_csSLogFile);
      } else if (!strcmp(configOption, "MasterADKey")) {
        sscanf(buffer, "MasterADKey %s", &m_csMasterADKey);
      } else if (!strcmp(configOption, "PCBQueueSize")) {
        sscanf(buffer, "PCBQueueSize %d", &m_iPCBQueueSize);
      } else if (!strcmp(configOption, "PSQueueSize")) {
        sscanf(buffer, "PSQueueSize %d", &m_iPSQueueSize);
      } else if (!strcmp(configOption, "PCBGenPeriod")) {
        sscanf(buffer, "PCBGenPeriod %d", &m_iPCBGenPeriod);
      } else if (!strcmp(configOption, "SourceIPAddress")) {
        sscanf(buffer, "SourceIPAddress %s", &m_tunnelSource);
      } else if (!strcmp(configOption, "DestinationIPAddress")) {
        sscanf(buffer, "DestinationIPAddress %s", &m_tunnelDestination);
      } else {
        click_chatter("Unknown config option: %s!\n", configOption);
        fclose(inputfile);
        return false;
      }
    }
  }
  fclose(inputfile);
  return true;
}

int Config::getNumRegisterPath() {
  return m_iRegisteredPaths;
}

int Config::getNumRetUP() {
  return m_iShortestUPs;
}

int Config::getIsCore() {
  return m_iCore;
}

int Config::getIsRegister() {
  return m_iIsRegister;
}

int Config::getPCBQueueSize() {
  return m_iPCBQueueSize;
}

int Config::getPSQueueSize() {
  return m_iPSQueueSize;
}

uint64_t Config::getBeaconServerAddr() {
  return m_uAddrBS;
}

uint64_t Config::getPathServerAddr() {
  return m_uAddrPS;
}

uint64_t Config::getCertificateServerAddr() {
  return m_uAddrCS;
}

uint64_t Config::getAdAid() {
  return m_uAdAid;
}

uint16_t Config::getTdAid() {
  return m_uTdAid;
}

int Config::getPCBGenPeriod() {
  return m_iPCBGenPeriod;
}

int Config::getLogLevel() {
  return m_iLogLevel;
}

bool Config::getSwitchLogFilename(char *fn) {
  strncpy(fn, m_csSLogFile, MAX_FILE_LEN);
  return true;
}

bool Config::getLogFilename(char *fn) {
  strncpy(fn, m_csLogFile, MAX_FILE_LEN);
  return true;
}

bool Config::getCSLogFilename(char *fn) {
  strncpy(fn, m_csCSLogFile, MAX_FILE_LEN);
  return true;
}

bool Config::getPSLogFilename(char *fn) {
  strncpy(fn, m_csPSLogFile, MAX_FILE_LEN);
  return true;
}

bool Config::getPCBLogFilename(char *fn) {
  strncpy(fn, m_csBSLogFile, MAX_FILE_LEN);
  return true;
}

bool Config::getPrivateKeyFilename(char * fn) {
  strncpy(fn, m_csPrivateKeyFile, MAX_FILE_LEN);
  return true;
}

bool Config::getCertFilename(char * fn) {
  strncpy(fn, m_csCertificateFile, MAX_FILE_LEN);
  return true;
}

bool Config::getROTFilename(char *fn) {
  strncpy(fn, m_csROTFile, MAX_FILE_LEN);
  return true;
}

bool Config::getOfgmKey(char* key) {
  strncpy(key, m_csMasterOfgKey, MASTER_KEY_LEN);
  return true;
}

bool Config::getMasterADKey(char* key) {
  strncpy(key, m_csMasterADKey, MASTER_KEY_LEN);
  return true;
}

int Config::getK() {
  return m_iRegisteredPaths;
}

int Config::getPTime() {
  return m_iPTime;
}

int Config::getRTime() {
  return m_iRTime;
}

int Config::getResetTime() {
  return m_iResetTime;
}

std::vector<uint16_t> Config::getIFIDs() {
  return ifid_set;
}

const char * Config::getTunnelSource() {
  return m_tunnelSource;
}

const char * Config::getTunnelDestination() {
  return m_tunnelDestination;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(Config)
