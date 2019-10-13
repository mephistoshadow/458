# 458
Overview of Code structure.

ARP part:
For ARP part, we first check if the packet is ARP request or reply. 
For ARP request, we first generate a new packet which type
is ARP reply then assign the target MAC address to it as the sender address. Because right now we have our destination MAC address.We need send back a ARP reply request to original host to tell him about the oringinal target host's MAC address.
For ARP reply, we check the ARP cache and to see all the packet who waiting on this ARP to know the target MAC address.Then we assign the the destination MAC address and the outgoing interface as the source address and send the outstanding packet as it should go.
For "sr_arpcache_sweepreqs" and "handle_arpreq" for every ARP request in the cache, we keep handle the request such as every second, we check the request has been sent 5 times or not, if it have been sent 5 times we send ICMP host unreachable. If it has not sent 5 times, we send the broad cast ARP request and waiting for reply. 


IP part:


ICMP part:





design decisions:
