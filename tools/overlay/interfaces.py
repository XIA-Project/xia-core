# install python-netifaces package for this functionailty
import netifaces

def get_interfaces():
    interfaces = netifaces.interfaces()
    interfaces.remove("lo")
    return interfaces

def get_ip_addr(interface):
    return netifaces.ifaddresses(interface)[netifaces.AF_INET][0]['addr']

def get_mac_addr(interface):
    return netifaces.ifaddresses(interface)[netifaces.AF_LINK][0]['addr']
