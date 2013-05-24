#!/usr/bin/python

from random import choice

machines = open('names','r').read().split('\n')[11:]

client_a = choice(machines)
client_b = client_a
while client_b == client_a:
    client_b = choice(machines)

machines = open('names','r').read().split('\n')[:11]
open('machines','w').write("\n".join(machines+[client_a]+[client_b]))

client_a = client_a.split('#')[1]
client_b = client_b.split('#')[1]

bb= """[backbone]
        Denver: Seattle, SF, KC
        Seattle: Denver, SF
        SF: Seattle, Denver, LA
        LA: SF, Houston
        Houston: LA, KC, Atlanta
        KC: Denver, Houston, Indianapolis
        Atlanta: Houston, Indianapolis, DC
        Indianapolis: KC, Atlanta, Cleveland
        Cleveland: Indianapolis, NYC
        NYC: Cleveland, DC
        DC: Atlanta, NYC"""

clients = """[clients]
        %s: %s
        %s: %s""" % (client_a, client_b, client_b, client_a)

open('tunneling.topo','w').write('%s\n%s' % (bb, clients))
