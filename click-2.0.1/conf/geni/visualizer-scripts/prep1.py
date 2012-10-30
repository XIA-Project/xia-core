import MySQLdb
import time
import socket
import random

#dbHost='utah.xiaslice.emulab-net.emulab.net'
dbHost='ganel.gpolab.bbn.com'
dbUser='xia'
dbPassword='xi@'
dbName='xia'

sqlCommands = [
"UPDATE status SET status='hidden'",
"UPDATE status SET status='normal' where handle in ('H0', 'H1')",
"UPDATE status SET status='normal' where handle in ('R0', 'R1')",
"UPDATE status SET status='normal' where handle in ('CMU', 'Portland')",
"UPDATE status SET status='normal' where handle like '%-Name'",
"UPDATE visuals SET renderAttributes='text=Forwarding' where statusHandle like 'R%-State'",
"UPDATE status SET status='normal' where handle like 'R%-State'",
"UPDATE status SET status='normal' where handle like '%-Graph'",
"UPDATE status SET status='normal' where handle like '%-thin'",
"TRUNCATE statistics"
]


def main():
    conn = MySQLdb.connect(host=dbHost,
                           user=dbUser,
                           passwd=dbPassword,
                           db=dbName)
    for sql in sqlCommands:
        conn.query(sql)


if __name__ == '__main__':
    main()
