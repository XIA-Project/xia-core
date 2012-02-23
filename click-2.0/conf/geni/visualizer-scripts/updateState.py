#!/usr/bin/python

import MySQLdb
import time
import sys

dbUser='xia'
dbPassword='xi@'
dbName='xia'
tableName = 'visuals'
updateFormat = "UPDATE %s SET renderAttributes='text=%s' WHERE statusHandle='%s'"

def main(argv=None):
    if argv is None:
        argv = sys.argv[1:]
    myHost = argv[0]
    dbHost = argv[1]
    conn = MySQLdb.connect(host=dbHost,
                           user=dbUser,
                           passwd=dbPassword,
                           db=dbName)
    myVisual = myHost + '-State'
    
    line = sys.stdin.readline()
    while line:
        value = line
        sql = updateFormat % (tableName, value, myVisual)
        conn.query(sql)
        line = sys.stdin.readline()

if __name__ == '__main__':
    main()
