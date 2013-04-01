#include <stdio.h>
#include <string.h>
#include <click/config.h>
#include "config.hh"
#include "trace.hh"


CLICK_DECLS
/* Parse a configuration file.  Return true if parsing succeeds */
bool Config::parseConfigFile(char *fileName) {
	
	//open the config file
	FILE* inputfile; ;

	if( (inputfile = fopen( fileName, "r" ))== NULL ) {
        Trace::print(Trace::ERR, "Failed to open the config file!\n");
		return false;
	}

	// go through each line and try to interpret the line
	char buffer[MAXLINELEN];
    char configOption[MAXLINELEN];
    while( !feof( inputfile ) ) {
    	memset(buffer, 0, MAXLINELEN);
		if(fgets(buffer, MAXLINELEN, inputfile)==NULL) {
			break;
    	}
	//while(fgets(buffer, MAXLINELEN, inputfile)!=NULL) {
		
    	// If we have a valid line...
		if (buffer[0] != '#') {	
      	// Read in the config option name
      		memset(configOption, 0, MAXLINELEN);
      		if (sscanf(buffer, "%s", configOption) != 1) {
        		Trace::print(Trace::ERR, "Failed to find a proper config option\n");
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
      		} else if (!strcmp(configOption, "MasterOFGKey")) {
        		sscanf(buffer, "MasterOFGKey %s", m_csMasterOfgKey);
      		} else if (!strcmp(configOption, "RegisterTime")) {
        		sscanf(buffer, "RegisterTime %d", &m_iRTime);
      		} else if (!strcmp(configOption, "PropagateTime")) {
        		sscanf(buffer, "PropagateTime %d", &m_iPTime);
      		} else if (!strcmp(configOption, "ResetTime")) {
			//SL: what is the reset time for???
			//Currently, it's unused.
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
		  	} else if(!strcmp(configOption, "Log")){
				sscanf(buffer, "Log %s", &m_csLogFile);
		  	} else if(!strcmp(configOption, "CSLog")){
				sscanf(buffer, "CSLog %s", &m_csCSLogFile);
		  	} else if(!strcmp(configOption, "PSLog")){
				sscanf(buffer, "PSLog %s", &m_csPSLogFile);
		  	} else if(!strcmp(configOption, "BSLog")){
				sscanf(buffer, "BSLog %s", &m_csBSLogFile);
		  	} else if(!strcmp(configOption, "LogLevel")){
				sscanf(buffer, "LogLevel %d", &m_iLogLevel);
		  	} else if(!strcmp(configOption, "RegisterPath")){ 
				sscanf(buffer,"RegisterPath %d", &m_iIsRegister); 
		  	} else if(!strcmp(configOption, "Core")){ 
				sscanf(buffer,"Core %d", &m_iCore); 
		  	} else if(!strcmp(configOption, "SLog")){
				sscanf(buffer,"SLog %s", &m_csSLogFile);
		  	} else if(!strcmp(configOption, "MasterADKey")){
				sscanf(buffer,"MasterADKey %s", &m_csMasterADKey);
		  	} else if(!strcmp(configOption, "PCBQueueSize")){
				sscanf(buffer,"PCBQueueSize %d", &m_iPCBQueueSize);
		  	} else if(!strcmp(configOption, "PSQueueSize")){
				sscanf(buffer,"PSQueueSize %d", &m_iPSQueueSize);
		  	} else if(!strcmp(configOption, "PCBGenPeriod")){
				sscanf(buffer,"PCBGenPeriod %d", &m_iPCBGenPeriod);
		  	} else if(!strcmp(configOption, "SourceIPAddress")) {
				sscanf(buffer,"SourceIPAddress %s", &m_tunnelSource);
		  	} else if(!strcmp(configOption, "DestinationIPAddress")) {
				sscanf(buffer,"DestinationIPAddress %s", &m_tunnelDestination);
		  	} else {
				Trace::print(Trace::ERR, "Unknown config option: %s!\n", configOption);
				fclose(inputfile);
				return false;
		  	}
		}	
	}  
  fclose(inputfile);
  return true;
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(Config)
