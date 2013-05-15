#!/usr/bin/python

import socket

f = open('sites.xml')
sites = f.read().split(']>')[1].split('</SITE>')
for site in sites:
    hosts = site.split('<HOST')
    try:
        lon = hosts[0].split('LONGITUDE="')[1].split('"')[0]
        lat = hosts[0].split('LATITUDE="')[1].split('"')[0]
        for host in hosts[1:]:
            name = host.split('NAME="')[1].split('"')[0]
            try: 
                ip = socket.gethostbyname(name)
                print ip, lon, lat, name
            except:
                pass
    except:
        pass

