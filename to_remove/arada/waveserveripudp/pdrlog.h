/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 *
 * Rui Meireles (rui@cmu.edu)
 * Fall 2016
 */

#ifndef PDRLOG_H
#define PDRLOG_H

#include <map> // std::map
#include <iostream> // std::cout, ...

#include <cstdint> // uint*_t
#include <ctime> // std::time_t
#include <semaphore.h> // sem_t


struct PdrLogEntry{
  unsigned long ntx = 0;
  unsigned long nrx = 0;
}; // PdrLogEntry


typedef std::map<uint64_t, PdrLogEntry> PdrLogMap;

/**
 * This object stores a map of mac address to number of message transmssions
 * and receptions.
 */
class PdrLog {

public:

  PdrLog();
  ~PdrLog();

  void logTx(const uint64_t dstMac);
  void logRx(const uint64_t srcMac);
  void print(std::ostream &ofs, const std::time_t tstamp, bool reset);

private:

  void lock();
  void release();

  PdrLogMap m_log; /* the log per se*/
  sem_t m_logsem;

}; // PdrLog

#endif // PDRLOG_H
