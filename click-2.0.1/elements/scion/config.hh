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

#ifndef CLICK_2_0_1_ELEMENTS_SCION_CONFIG_H_
#define CLICK_2_0_1_ELEMENTS_SCION_CONFIG_H_
#include <stdint.h>
#include <vector>
#include "define.hh"

/**
    @brief Configuration File parsing class.

    This is the class of function that parses the ".conf" file for each AD and
    servers. Each servers and routers will call the constructors of this class
    Config() and passes the "xxx.conf" file to initiate the configureation.

    Each functions in this class will return or pass the reference to the
    caller.

    e.x) parser.getLogLevel() will return the logLevel that is specified in the
    configuration file.

    Likewise, parser.getMasterADKey(char* key) will pass the key to the parameter
    'key', expecting the char* has memory allocated outside the function.
*/
class Config {
 public:
  /**
   * @brief Initializes the empty Config object.
   *
   *  The initial Config object is empty or has defalt values for all the
   *  fields. This fields will be filled up using the parseConfigFile().
   *
   *  @note The default values have no significant meanings. In order to use a
   *  certain field, the value must be specified in the .conf file and should be
   *  parsed by calling parseConfigFile().
   *
   *  @note For servers and routers in each AD, not all the values is present in
   *  the config file. The parseConfigFile() will only put those values inside
   *  the configuration file.
   *
   *  For those values that are not present in the configuration file, the
   *  values will stay default.
   */
  Config();

 private:
  /**
      @brief 64 Bit Ad Id field
  */
  uint64_t m_uAdAid;
  /**
      @brief64 Bit Td Id field
  */
  uint16_t m_uTdAid;
  /**
      @brief 64 Bit Beacon server address
  */
  uint64_t m_uAddrBS;
  /**
      @brief 64 bit Path Server Address
  */
  uint64_t m_uAddrPS;
  /**
      @brief 64 Bit Certificate Server Address
  */
  uint64_t m_uAddrCS;
  /**
      @brief AD Private Key File name
  */
  char m_csPrivateKeyFile[MAX_FILE_LEN+1];
  /**
      @brief AD Certificate File name
  */
  char m_csCertificateFile[MAX_FILE_LEN+1];
  /**
      @brief AD Master Opaque Field Generation Key File name
  */
  char m_csMasterOfgKey[MASTER_KEY_LEN+1];
  /**
      @ brief AD Log File name
  */
  char m_csLogFile[MAX_FILE_LEN+1];
  /**
      @brief AD Path Server Log File name
  */
  char m_csPSLogFile[MAX_FILE_LEN+1];
  /**
      @brief AD Certificate Server Log File name
  */
  char m_csCSLogFile[MAX_FILE_LEN+1];
  /**
      @brief AD Beacon Server Log File name
  */
  char m_csBSLogFile[MAX_FILE_LEN+1];
  /**
      @brief Certificate Server Private Key File name
  */
  char m_csSLogFile[MAX_FILE_LEN+1];
  /**
      @brief Certificate Server Private Key File name
  */
  char m_csMasterADKey[MASTER_KEY_LEN+1];
  /**
      @brief Tunnel Source
  */
  char m_tunnelSource[16];
  /**
      @brief Tunneling destination
  */
  char m_tunnelDestination[16];
  /**
      @brief set of interface IDs
  */
  std::vector<uint16_t> ifid_set;
  /**
      @brief number of Interface IDs
  */
  int m_iNumInterface;
  /**
      @brief Number of paths to be registered
  */
  int m_iRegisteredPaths;
  /**
      @brief Number of shortest up paths
  */
  int m_iShortestUPs;
  /**
      @brief Propagation Time
  */
  int m_iPTime;
  /**
      @brief Registration Time
  */
  int m_iRTime;
  /**
      @brief Time to reset the PCB tables
  */
  int m_iResetTime;
  /**
      @brief Log level (defines the depth of Logging)
  */
  int m_iLogLevel;
  /**
      @brief 1 if this AD registers path.
  */
  int m_iIsRegister;
  /**
      @brief Flag to represent Core AD
  */
  int m_iCore;
  /**
      @brief PCB Queue Size for the Beacon Server
  */
  int m_iPCBQueueSize;
  /**
      @brief Path Server Queue Size for Paths
  */
  int m_iPSQueueSize;
  /**
      @brief Time period for generating PCBs
  */
  int m_iPCBGenPeriod;

 public:
  /**
     @brief Fills the values of the Config() object with the values from the
     configuration file.
     @param char* filename Path of the configuration file
     @return Returns true when the file is loaded successfully. Returns
     false otherwise.

     This function parses the configuration file line by line and determines
     what information is present in the file. For each line, it compares with
     the existing field and puts the information from each line to the fields.

     Each line in the configuration file startes with a type tag that specifies
     information of the line. If the tag type does not match any fields, then
     the function returns false, exiting the parsing phase.

  */
  bool parseConfigFile(char* filename);
  /**
      @brief Returns the number of registration paths
      @return Number of path to be registered.

      This function returns the number of paths that can be registered from the
      beacon server. The beacon server uses this value to determine how many
      paths will be registered to the TDC.
  */
  int getNumRegisterPath();

  /**
      @brief Returns the number of UP paths to keep.
      @return The number of UP paths to keep.

      This function returns the number of UP paths to keep. It defines the
      capacity of the UP path queue. This value is the limit of number of
      up paths that each entity (whichever) can keep.
  */
  int getNumRetUP();

  /**
      @brief Determines if this AD is registering paths to TDC.
      @return Returns 1 when this AD is registering paths. 0
      if not.
      @note Not all ADs are registring paths to the TDC.

      Returns the isRegister flag.
      If this value is 1 the beacon server(s) of this AD will register paths to TDC.
      If 0, the beacon server(s) will just propagate PCB to downstream.

  */
  int getIsRegister();
  /**
      @brief Determines if the caller AD is in TDC.
      @return Returns 1 when the caller AD is in TCD. 0 otherwise.

      This function tells the calling AD if it is a TDC AD or not. If the
      returned value is 1 then this AD is a member of TDC, and 0 if it is not.
  */
  int getIsCore();
  /**
      @brief Returns the maxinum number of PCB that the beacon server will keep.
      @return The capacity of the PCB table of the beacon server.

      The beacon server(s) can only keep limited number of PCBs at the same
      time. This value determines the maximum number of PCBs that the calling
      AD's beacon servers can keep.

      @note: There is no limitation of how large this value can be, but too large
      value will may cause memory issues or performance issues.
  */
  int getPCBQueueSize();
  /**
      @biref Returns the queue size for the Path Server.
      @return The maximum number of paths that the calling
      Path server can keep at the same time.

      Similar to PCB Queue, the path servers only can keep limited number of
      up/down paths at the same time. This value specifies the max number.
  */
  int getPSQueueSize();
  /**
      @brief Returns the SCION address of the Beacon Server
      @return SCION address of the Beacon Server.

      Returns the SCION address of the Beacon server of the calling AD.

      @note: Multiple Beacon Servers are not supported at this time.
  */
  uint64_t getBeaconServerAddr();
  /**
      @brief Returns the SCION address of the Path Server
      @return Returns the SCION address of the Path Server.

      @note Multiple Path Servers are not supported at this time.
  */
  uint64_t getPathServerAddr();
  /**
      @brief Returns the SCION Address of the Certificate Server.
      @return SCION Address of the Certificate Server.

      @note Multiple Certificate Servers are not supported at this time.
  */
  uint64_t getCertificateServerAddr();
  /**
      @brief Returns the AD identification number of the calling AD.
      @return ADAID of the calling AD.
  */
  uint64_t getAdAid();
  /**
      @brief Returns the TD identification number of the calling AD.
      @return TDAID of the calling AD.
  */
  uint16_t getTdAid();
  /**
      @brief Returns the PCB generation rate in seconds.
      @return The rate of PCB generation.
      @note This function is only called from the TDC Beacon Server.
  */
  int getPCBGenPeriod();
  /**
      @brief Returns the Log Level of this AD or Server
      @return Log Level of the calling server or router.

      @note Log Level represents how much information will be logged during the
      uptime of each servers and routers. The Log Levels are defined in
      scionprint.hh.

      Please refer to the implementation document for more detail about the Log Level.
  */
  int getLogLevel();
  /**
      @brief Gets the path of the log file name for switch.
      @param char* fn The buffer where the log file path is stored

      The path of the log file for switch is passed to the parameter char* fn.
  */
  bool getSwitchLogFilename(char *fn);

  bool getLogFilename(char *fn);

  /**
      @brief Gets the path of the log file name for Certificate Server.
      @param char* fn The buffer where the log file path is stored

      The path of the log file for Certificate Server is passed to the parameter char* fn.
  */
  bool getCSLogFilename(char *fn);

  /**
      @brief Gets the path of the log file name for Path Server.
      @param char* fn The buffer where the log file path is stored

      The path of the log file for Path Server is passed to the parameter char* fn.
  */
  bool getPSLogFilename(char *fn);

  /**
      @brief Gets the path of the log file name for Beacon Server.
      @param char* fn The buffer where the log file path is stored

      The path of the log file for Beacon Server is passed to the parameter char* fn.
  */
  bool getPCBLogFilename(char *fn);

  /**
      @brief Gets the path of the private key file of the calling AD.
      @param char* fn The buffer where the key file path is stored

      The private key file that stores the private key in generating signature
      will be passed to the char* fn.
  */
  bool getPrivateKeyFilename(char * fn);

  /**
      @brief Gets the path of Certificate File of the calling AD.
      @param char* fn The buffer where the certificate file path is stored.

      The path of the certificate file will be passed to the char* fn.
  */
  bool getCertFilename(char * fn);

  /**
      @brief Gets the path of the Opaque Field Generation Master Key file.
      @param char* key The buffer where path the Opaque Field Generation Master Key
      file is stored.

      The path of the OfgmKey, that is used in generating short term Opaque field
      generation key,  will be passed to char* key.
  */
  bool getOfgmKey(char* key);

  /**
      @brief Gets the path of the MasterADKey file.
      @param char* key The buffer where the path of Master AD Key file is
      stored.

      The path of the Master AD Key will be passed to char* key.
  */
  bool getMasterADKey(char* key);

  /**
      @brief Returns the number of paths to be registered.
      The value K represents the number of paths that this AD will register to
      the TDC. If the isRegister is 0 then this value will be ignored.
  */
  int getK();

  /**
      @brief Returns the propagation time (in seconds)
      The Beacon Server does propagation in timely fashion. This value
      represents the time interval for Beacon Propagation.
  */
  int getPTime();

  /**
      @brief Returns the path registration time (in seconds)
      The Beacon Server Registers Path every time interval. This value
      represents the time interval for path registeration.
  */
  int getRTime();

  /**
      @brief Returns the reset time (in seconds)
      Every server will clear out their Queues after time t. This value
      represents the interval t.
  */
  int getResetTime();

  std::vector<uint16_t> getIFIDs();

  /** @brief Undocumented */
  const char * getTunnelSource();

  /** @brief Undocumented */
  const char * getTunnelDestination();
};


#endif  // CLICK_2_0_1_ELEMENTS_SCION_CONFIG_H_
