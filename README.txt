# 458
Overview of Code structure.

ARP part:



IP part:


ICMP part:
For sending ICMP packets, we build a general interface function to send all kinds of ICMP messages. The most important part of this function is to set new Source IP and destination IP.  For echo reply, we simply switch the  IP addresses of the original packet. For time exceed and destination unreachable, we construct a new packet and use source IP of original packet ad new destination IP, and find the interface of router as new source IP.  Then we use arp cache to get mac addresses. If we can't find the destination mac address then we add it to the arp request queue.




design decisions:
