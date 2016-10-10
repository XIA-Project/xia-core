How to configure OBU/RSU to support IP communication:

1. run "cli"
2. run "config ip no-of-bridges 2"
3. run "config ip brwifi-ipv4-address 192.168.3.x", where x is a number of your choice in [1,254]
4. run "exit" to exit cli. You won't need to repeat this configuration step

5. create an executable file on /tmp/usb with the following content:
#!/bin/sh

iwpriv wifi0vap0 ipcchpermit 1
echo 1 > /proc/sys/net/ipv4/conf/default/forwarding 
echo 1 > /proc/sys/net/ipv4/conf/all/forwarding     
echo 1 > /proc/sys/net/ipv4/conf/default/proxy_arp
echo 1 > /proc/sys/net/ipv4/conf/all/proxy_arp


Execute this file to enable IP communication. This will need to be done every time you reboot the device.
Test configuration using ping.