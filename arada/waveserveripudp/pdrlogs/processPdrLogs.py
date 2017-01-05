#!/usr/bin/env python

"""
  This script reads mul of pdr logs and summarizes them.

  Rui Meireles, Fall 2016
"""

# imports
import sys, os, fnmatch

# global variables
_IN_DIRNAME = "logs"
_OUT_FNAME = "pdr-log-all.txt"


if __name__ == "__main__":
  
  # the all encompassing dictionary
  mainDic = {}
  
  for fname in os.listdir(_IN_DIRNAME):

    if not fnmatch.fnmatch(fname, 'pdrlog-*.txt'):
      continue

    with open(os.path.join(_IN_DIRNAME, fname), 'r') as infile: # process it
      
      
      # first line is of the form # PDR log for MAC **:**:**:**:**:**"
      firstLine = infile.readline()
      
      sidx = len("# PDR log for MAC ")
      eidx = sidx + 17
      mymac = firstLine[sidx:eidx]
 
      if len(mymac) != 17: # malformed mac
        continue # get out of here!
        
      mymac = mymac.upper()

      for line in infile:
      
        if line[0] == '#': # skip comments
          continue
      
        tstamp, omac, ntx, nrx = line.split()
        
        tstamp = int(tstamp)
        omac = omac.upper()
        ntx = int(ntx)
        nrx = int(nrx)
        
        # fill in the forward direction
        fwdKey = (mymac, omac) # forward direction
      
        if fwdKey not in mainDic:
          mainDic[fwdKey] = {}

        if tstamp not in mainDic[fwdKey]:
          mainDic[fwdKey][tstamp] = {}
        
        mainDic[fwdKey][tstamp]['#tx'] = ntx

        # and the reverse direction as well
        revKey = (omac, mymac) # reverse direction
        
        if revKey not in mainDic:
          mainDic[revKey] = {}

        if tstamp not in mainDic[revKey]:
          mainDic[revKey][tstamp] = {}
        
        mainDic[revKey][tstamp]['#rx'] = nrx

  # have read everything, now write it out
  with open(_OUT_FNAME, 'w') as outfile:

    print >>outfile, "macpair tstamp ntx nrx pdr"

    keys = mainDic.keys()
    keys.sort()
    
    for key in keys:
    
      smac = key[0][9:] # strip the common part to make it shorter
      dmac = key[1][9:]
    
      tstamps = mainDic[key].keys()
      tstamps.sort()
    
      for tstamp in tstamps:

        entry = mainDic[key][tstamp]
 
        if '#tx' not in entry or '#rx' not in entry:
          continue # not enough data
        
        ntx = entry['#tx']
        nrx = entry['#rx']
        
        if ntx > 0:
          pdr = float(nrx)/ntx
        else:
          pdr = 2 # received something out of nothing, mark as error
        
        print >>outfile, "%s->%s %u %u %u %f" % \
            (smac, dmac, tstamp, ntx, nrx, pdr)

  print "Done!"
