/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 *
 * Rui Meireles (rui@cmu.edu)
 * Fall 2016
 */

#include <sstream> // stringstream
#include <stdexcept> // std::runtime_error()

#include <cerrno> // errno
#include <cstring> // strerror()
#include <exception> // terminate()
#include <iomanip> // std::hex, etc

#include "pdrlog.h"


PdrLog::PdrLog() : m_log(), m_logsem() {

  // initialize semaphore protecting concurrent access to log
  if (sem_init(&m_logsem, 0, 1) == -1) {
    std::stringstream ss;
    ss << "In PdrLog::~PdrLog() sem_init(): " << strerror(errno) << std::endl;
    throw std::runtime_error(ss.str());
  }
}

PdrLog::~PdrLog(){

  // destroy the semaphore
  if (sem_destroy(&m_logsem) != 0){
    std::stringstream ss;
    ss << "In PdrLog::~PdrLog() sem_close(): " << strerror(errno) << std::endl;
    throw std::runtime_error(ss.str());
  }
}

/**
 * Log a message transmission.
 */
void PdrLog::logTx(const uint64_t dstMac){

  this->lock();

  if (m_log.find(dstMac) == m_log.end()){
    m_log[dstMac].ntx = 1;
  } else{
    m_log[dstMac].ntx++;
  }

  this->release();
}

/**
 * Log a message reception.
 */
void PdrLog::logRx(const uint64_t srcMac){

  this->lock();

  if (m_log.find(srcMac) == m_log.end()){
    m_log[srcMac].nrx = 1;
  } else{
    m_log[srcMac].nrx++;
  }
  
  this->release();
}

/**
 * Print the pdr log using the following format: tstamp mac ntx nrx
 * Optionally reset the log at the end.
 */
void PdrLog::print(std::ostream &ofs, const std::time_t tstamp, bool reset){

  this->lock();

  for (PdrLogMap::const_iterator logitr = m_log.begin(); \
      logitr != m_log.end(); ++logitr){
    
    ofs << std::dec << tstamp << " ";

    uint8_t* mac = (uint8_t*) &(logitr->first);
    
    for (int i=0; i < 6; i++){
      ofs << std::hex << std::setfill('0') << std::setw(2) << \
          static_cast<unsigned>(mac[i]);

      if (i < 5){
        ofs << ":";
      }
    }
    
    const PdrLogEntry& logEntry = logitr->second;
    ofs << " " << std::dec << logEntry.ntx << " " << logEntry.nrx << std::endl;
  }
  
  if (reset){ // reset the log if that's the case
    m_log.clear();
  }
  
  this->release();
  
}

/**
 * Acquire log lock. Beware of deadlocks!
 */
void PdrLog::lock(){
  
  if (sem_wait(&m_logsem)){
    std::stringstream ss;
    ss << "In PdrLog::lock() sem_wait(): " << strerror(errno) << std::endl;
    throw std::runtime_error(ss.str());
  }
}

/**
 * Release lock. Beware of deadlocks!
 */
void PdrLog::release(){

  if (sem_post(&m_logsem)){
    std::stringstream ss;
    ss << "In PdrLog::release() sem_post(): " << strerror(errno) << std::endl;
    throw std::runtime_error(ss.str());
  }
}
