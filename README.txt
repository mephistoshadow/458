# 458
Overview of Code structure:
In sr_router.c, we generally flow the workflow from the tutorial slides. To make our structure clear, we have 2 functions to handle IP  and ARP packet.  We build a function to handle sending all kinds of ICMP message.
ARP part:
For ARP part, we first check if the packet is ARP request or reply. 
For ARP request, we first generate a new packet which type
is ARP reply then assign the target MAC address to it as the sender address. Because right now we have our destination MAC address.We need send back a ARP reply request to original host to tell him about the oringinal target host's MAC address.
For ARP reply, we check the ARP cache and to see all the packet who waiting on this ARP to know the target MAC address.Then we assign the the destination MAC address and the outgoing interface as the source address and send the outstanding packet as it should go.
For "sr_arpcache_sweepreqs" and "handle_arpreq" for every ARP request in the cache, we keep handle the request such as every second, we check the request has been sent 5 times or not, if it have been sent 5 times we send ICMP host unreachable. If it has not sent 5 times, we send the broad cast ARP request and waiting for reply. 


IP part:
1. Check the checksum
2. Find if the destination of the package is this router
	a. Package destination is this router
		a1. If it's ICMP echo request, send ICMP echo reply
		a2. If it's TCP or UDP, send ICMP port unreachable
	b. Package destination elsewhere
		b1. Decrease TTL
		b2. If TTL < 1, send ICMP time exceeded
		b3. Recalculate the checksum
		b4. Find longest prefix match, if cannot find longest prefix match send ICMP destination net unreachable
		b5. Find arp entry, if cannot find arp entry in cache, broadcast ARP request
		b6. Send it to next hop

For the destination checking part (part 2), I don't know how to use the helper function sr_get_interface, so I wrote another helper function that helps me get interface by ip address.
For the package forwarding part (part b6), I thought sending the original package with a TTL decrease is ok. However, by debugging with me teammates, I found out I have to change the hosts for the ethernet header in order to forward the package.



ICMP part:
For sending ICMP packets, we build a general interface function to send all kinds of ICMP messages. The most important part of this function is to set new Source IP and destination IP.  For echo reply, we simply switch the  IP addresses of the original packet. For time exceed and destination unreachable, we construct a new packet and use source IP of original packet ad new destination IP, and find the interface of router as new source IP.  Then we use arp cache to get mac addresses. If we can't find the destination mac address then we add it to the arp request queue.


Decisions: First we seperate the packet into two cases.One is for IP packet the other is For ARP packet. Then For ARP and IP we solve it individually and accomplish it's own functionality.We spend sometime to figuare out which case we need use big endian and which case we use little endian when we assign variable's value. Because if we use wrong order, it causes the hard situation to debug.
