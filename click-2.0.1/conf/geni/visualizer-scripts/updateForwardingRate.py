#!/usr/bin/python

import MySQLdb
import time
import sys

dbUser='xia'
dbPassword='xi@'
dbName='xia'

statisticsNames = ['Total', 'CID', 'SID', 'HID'] 

valueFormat = "(NULL, '%s', '%s', '%f')"
insertFormat = "insert into statistics values %s"
timeFormat='%Y-%m-%d %H:%M:%S'
updateFormat = "UPDATE status SET status = '%s' WHERE handle = '%s'"

def valueToStatus(value, myThinStatusHandle, myThickStatusHandle):
    '''Compute status corresponding to provided value.'''
    if value > 10:
        return { myThinStatusHandle : 'hidden',
                 myThickStatusHandle : 'backward' }
    elif value > 1:
        return { myThinStatusHandle : 'backward',
                 myThickStatusHandle : 'hidden' }
    else:
        return { myThinStatusHandle : 'normal',
                 myThickStatusHandle : 'hidden' }

def updateStatus(conn, status):
    '''Send new status vector to provided database connection.'''
    for handle in status.keys():
        sql = updateFormat % (status[handle], handle)
        conn.query(sql)

def parseValues(line):
    '''Parse a line full of floating point numbers, separated by spaces.

    Return a list of floats, or None on error.'''
    strings = line.split(' ')
    result = []
    for s in strings:
        try:
            val = int(s)
        except ValueError:
            print "Bad number, ignoring input line:", line
            return None
        result.append(val)
    return result

def updateValues(conn, names, values):
    '''Write new values to DB.'''
    if (len(values) > 0) and (len(values) == len(names)):
        nowString = time.strftime(timeFormat, time.gmtime())
        valueParts = []
        for i in range(len(values)):
            valueParts.append(valueFormat % (names[i], nowString, values[i]))
        sql = insertFormat % (','.join(valueParts))
        conn.query(sql)

def main(argv=None):
    # Initialize from arguments.
    if argv is None:
        argv = sys.argv[1:]
    myLink = argv[0]
    myThinStatusHandle = myLink+'-thin'
    myThickStatusHandle = myLink+'-thick'
    lastStatus = None
    dbHost = argv[1]
    names = []
    for stat in statisticsNames:
        names.append(myLink + '-' + stat)

    # Open DB connection
    conn = MySQLdb.connect(host=dbHost,
                           user=dbUser,
                           passwd=dbPassword,
                           db=dbName)

    # Read values and update database.
    line = sys.stdin.readline()
    while line:
        try:
            # Update statistics table
            values = parseValues(line)
            if (values is not None) and (len(values) > 0) and (len(values) == len(names)):
                updateValues(conn, names, values)
            else:
                print 'Bad input line, ignoring:', line

            # Update status table, if changed
            newStatus = valueToStatus(values[0], myThinStatusHandle, myThickStatusHandle)
            if newStatus != lastStatus:
                updateStatus(conn, newStatus)
                lastStatus = newStatus
        except ValueError:
            print "Bad number, ignoring input line:", line
        line = sys.stdin.readline()

if __name__ == '__main__':
    main()
