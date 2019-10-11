/**********************************************************************
 * file:  sr_router.c
 * date:  Mon Feb 18 12:50:42 PST 2002
 * Contact: casado@stanford.edu
 *
 * Description:
 *
 * This file contains all the functions that interact directly
 * with the routing table, as well as the main entry method
 * for routing.
 *
 **********************************************************************/



#include <stdlib.h>
#include  <string.h>

#include <stdio.h>
#include <assert.h>


#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "sr_arpcache.h"
#include "sr_utils.h"




/*---------------------------------------------------------------------
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Initialize the routing subsystem
 *
 *---------------------------------------------------------------------*/

void sr_init(struct sr_instance* sr)
{
    /* REQUIRES */
  assert(sr);

    /* Initialize cache and cache cleanup thread */
  sr_arpcache_init(&(sr->cache));

  pthread_attr_init(&(sr->attr));
  pthread_attr_setdetachstate(&(sr->attr), PTHREAD_CREATE_JOINABLE);
  pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
  pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
  pthread_t thread;

  pthread_create(&thread, &(sr->attr), sr_arpcache_timeout, sr);

    /* Add initialization code here! */

} /* -- sr_init -- */

/*---------------------------------------------------------------------
 * Method: sr_handlepacket(uint8_t* p,char* interface)
 * Scope:  Global
 *
 * This method is called each time the router receives a packet on the
 * interface.  The packet buffer, the packet length and the receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 * Note: Both the packet buffer and the character's memory are handled
 * by sr_vns_comm.c that means do NOT delete either.  Make a copy of the
 * packet instead if you intend to keep it around beyond the scope of
 * the method call.
 *
 *---------------------------------------------------------------------*/

  void sr_handlepacket(struct sr_instance* sr,uint8_t * packet,unsigned int len,char* interface){
  /* REQUIRES */
    assert(sr);
    assert(packet);
    assert(interface);


    printf("*** -> Received packet of length %d \n",len);
    sr_ethernet_hdr_t *ethernet_hdr = (sr_ethernet_hdr_t*) packet;
    if (sizeof(ethernet_hdr) != sizeof(sr_ethernet_hdr_t)) {
      fprintf(stderr, "the lengh does not meet the minimum length of ethernet\n");
    }

    print_hdr_eth(packet);

    if(ethertype(packet) == ethertype_ip) {
      printf("It's IP packet.\n");
      print_hdr_ip(packet + sizeof(sr_ethernet_hdr_t));
      handle_ip(sr,packet,len,interface);

    } else if (ethertype(packet) == ethertype_arp) {
      printf("It's ARP packet.\n");
      print_hdr_arp(packet + sizeof(sr_ethernet_hdr_t));
      handle_arp_total(sr,packet,len,interface);
    } else {
      printf("Packet unknown.\n");
    }

  }
  
  void handle_arp_total(struct sr_instance* sr,uint8_t * packet,unsigned int len,char* interface){

   sr_arp_hdr_t *arp_header = (sr_arp_hdr_t*) (packet + sizeof(sr_ethernet_hdr_t));
   struct sr_if* sr_interface = sr_get_interface_by_ip(sr,arp_header->ar_tip);
   if(sr_interface) {
    if(arp_header ->ar_op == arp_op_request) {
      arp_request(sr,packet,len,interface);
    }else if(arp_header ->ar_op == arp_op_reply) {
     handle_arp_reply(sr,packet,len,interface);
   }

 }


}


struct sr_rt* longest_matching_prefix(struct sr_instance* sr, uint32_t ip) {
    struct sr_rt* longest_prefix_entry = NULL;

    char ip_string[15];
    struct sr_rt* table_entry = sr->routing_table;
    while(table_entry) {
        if((table_entry->dest.s_addr & table_entry->mask.s_addr) == (ip & table_entry->mask.s_addr)) {
            if(!longest_prefix_entry || table_entry->mask.s_addr > longest_prefix_entry->mask.s_addr) {
                longest_prefix_entry = table_entry;
            }
        }
        table_entry = table_entry->next;
    }

    return longest_prefix_entry;
}

void send_icmp_packet(struct sr_instance* sr, uint8_t* packet, unsigned int len, char* interface, uint8_t type, uint8_t code){
    /* Get Ethernet header */
    sr_ethernet_hdr_t* eth_hdr = (sr_ethernet_hdr_t *) packet;

    /* Get IP header */
    sr_ip_hdr_t* ip_hdr = (sr_ip_hdr_t *) (packet + sizeof(sr_ethernet_hdr_t));

    /*get arp caehe*/
    struct sr_arpcache *sr_cache = &sr->cache;
    switch(type){
        case icmp_echo_reply:{
            /* Get ICMP header */
            printf("ICMP echo.\n");
            sr_icmp_hdr_t* icmp_hdr = (sr_icmp_hdr_t*)(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));


            /* Modify ethernet header */
            memcpy(eth_hdr->ether_dhost, eth_hdr->ether_shost, sizeof(uint8_t)*ETHER_ADDR_LEN);
            memcpy(eth_hdr->ether_shost, sr_get_interface(sr, interface)->addr, sizeof(uint8_t)*ETHER_ADDR_LEN);

            /* Modify IP header */
            uint32_t src_ip = ip_hdr->ip_src;
            ip_hdr->ip_src = ip_hdr->ip_dst;
            ip_hdr->ip_dst = src_ip;
            /* not necessary to calculate new  checksum */

            /* Modify ICMP header  */
            icmp_hdr->icmp_type = type;
            icmp_hdr->icmp_code = code;
            /*caculate new IP ckecksum*/
            memset(&(icmp_hdr->icmp_sum), 0, sizeof(uint16_t));
            icmp_hdr->icmp_sum = cksum(icmp_hdr, len - sizeof(sr_ethernet_hdr_t) - sizeof(sr_ip_hdr_t));
            struct sr_arpentry * arp_entry = sr_arpcache_lookup (sr_cache, ip_hdr->ip_dst);
            if (arp_entry) {
                sr_send_packet (sr, packet, len, interface);
            } else {
                struct sr_arpreq * req = sr_arpcache_queuereq(sr_cache, ip_hdr->ip_dst, packet, len, interface);
                handle_arpreq(sr, req);
            }
            break;
      
          }
        case icmp_time_exceeded:
        case icmp_dest_unreachable:{
          printf("ICMP destination unreachable.\n");
            unsigned int new_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t);
            uint8_t* new_packet = malloc(new_len);
            /*construct  ethernet header*/
            sr_ethernet_hdr_t* new_eth_hdr = (sr_ethernet_hdr_t*)new_packet;
            memset(new_eth_hdr->ether_shost, 0, ETHER_ADDR_LEN);
            memset(new_eth_hdr->ether_dhost, 0, ETHER_ADDR_LEN);
            /* set protocol type to IP */
            new_eth_hdr->ether_type = htons(ethertype_ip);
            /* construct IP hdr */
            sr_ip_hdr_t* new_ip_hdr = (sr_ip_hdr_t*)(new_packet + sizeof(sr_ethernet_hdr_t));
            new_ip_hdr->ip_v    = 4;
            new_ip_hdr->ip_hl   = sizeof(sr_ip_hdr_t) / 4;
            new_ip_hdr->ip_tos  = 0;
            new_ip_hdr->ip_len  = htons(sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t));
            new_ip_hdr->ip_id   = htons(0);
            new_ip_hdr->ip_off  = htons(IP_DF);
            new_ip_hdr->ip_ttl  = 255;
            new_ip_hdr->ip_p    = ip_protocol_icmp;
            if (code==icmp_dest_unreachable_port){
                new_ip_hdr->ip_src = ip_hdr->ip_dst;
            }else{
                new_ip_hdr->ip_src = sr_get_interface(sr, interface)->ip;
            }
            /* calculate new checksum */
            new_ip_hdr->ip_sum = 0;
            new_ip_hdr->ip_sum = cksum(new_ip_hdr, sizeof(sr_ip_hdr_t));

             /* construct type 3 ICMP hdr */
            sr_icmp_t3_hdr_t* icmp_hdr = (sr_icmp_t3_hdr_t*)(new_packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
            icmp_hdr->icmp_type = type;
            icmp_hdr->icmp_code = code;
            icmp_hdr->unused = 0;
            icmp_hdr->next_mtu = 0;
            memcpy(icmp_hdr->data, ip_hdr, ICMP_DATA_SIZE);
            icmp_hdr->icmp_sum = 0;
            icmp_hdr->icmp_sum = cksum(icmp_hdr, sizeof(sr_icmp_t3_hdr_t));
            struct sr_rt* rt_entry = longest_matching_prefix(sr, ip_hdr->ip_src);
            if(rt_entry) {
                struct sr_arpentry * arp_entry = sr_arpcache_lookup (sr_cache, ip_hdr->ip_dst);
                if (arp_entry) {
                    printf("Found the ARP entry in the cache\n");
                    struct sr_if *out_interface = sr_get_interface(sr, rt_entry->interface);

                    /* Modify ethernet header */
                    sr_ethernet_hdr_t *new_eth_hdr = (sr_ethernet_hdr_t *) new_packet;
                    memcpy(new_eth_hdr->ether_dhost, arp_entry->mac, sizeof(uint8_t)*ETHER_ADDR_LEN);
                    memcpy(new_eth_hdr->ether_shost, out_interface->addr, sizeof(uint8_t)*ETHER_ADDR_LEN);
                

                    sr_send_packet(sr, new_packet, len, out_interface->name);
                    free(arp_entry);
                } else {
                    struct sr_arpreq * req = sr_arpcache_queuereq(sr_cache, ip_hdr->ip_dst, packet, len, interface);
                    handle_arpreq(sr, req);
                }
            }

            free(new_packet);
            break;
        }
            
            
            
    }

}

void arp_request(struct sr_instance* sr,uint8_t * packet,unsigned int len,char* interface){
  struct sr_if *sr_interface = sr_get_interface(sr,interface);

  sr_ethernet_hdr_t *ethernet_hdr = (sr_ethernet_hdr_t*) packet;
  sr_arp_hdr_t *arp_header = (sr_arp_hdr_t*) (packet + sizeof(sr_ethernet_hdr_t));
      /* get the new packet length.*/
  int length_new_packet = sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t);
       /* malloc the send back packet memory.*/
  uint8_t  *back_packet = malloc(length_new_packet);
       /* assign the new thernet header.*/
  sr_ethernet_hdr_t* new_ethernet_hdr = (sr_ethernet_hdr_t*) back_packet;
  memcpy(new_ethernet_hdr -> ether_dhost,ethernet_hdr -> ether_shost,ETHER_ADDR_LEN);
  memcpy(new_ethernet_hdr -> ether_shost,sr_interface -> addr,ETHER_ADDR_LEN);
  new_ethernet_hdr -> ether_type = ethernet_hdr -> ether_type;
       /* assign the new arp header for send back.*/
  sr_arp_hdr_t *new_arp_header = (sr_arp_hdr_t*) (back_packet + sizeof(sr_ethernet_hdr_t));
   /* revert the hardware address for sender and reciever.*/
  memcpy(new_arp_header -> ar_sha, sr_interface -> addr,ETHER_ADDR_LEN);
  memcpy(new_arp_header -> ar_tha,  arp_header -> ar_sha,ETHER_ADDR_LEN);
   /* assign the rest variable for arp header.*/
  new_arp_header -> ar_hrd = arp_header -> ar_hrd;
  new_arp_header -> ar_pro = arp_header -> ar_pro;
  new_arp_header -> ar_hln = arp_header -> ar_hln;
  new_arp_header -> ar_pln = arp_header -> ar_pln;
   /* convert the op code to big endian.*/
  new_arp_header -> ar_op = htons(arp_op_reply);
   /* revert the ip address for sender and receiever.*/
  new_arp_header -> ar_sip = sr_interface -> ip;
  new_arp_header -> ar_tip = arp_header -> ar_sip;
  sr_send_packet(sr,back_packet,length_new_packet,sr_interface->name);
  free(back_packet);
}

void handle_arp_reply(struct sr_instance* sr,uint8_t * packet,unsigned int len,char* interface) {
  /*get the cache for sr.*/
  struct sr_arpcache *sr_cache = &sr->cache;
  sr_arp_hdr_t *arp_header = (sr_arp_hdr_t*) (packet + sizeof(sr_ethernet_hdr_t));
  uint32_t sender_ip = arp_header -> ar_sip;
  /*check the arp cache table to see if we have the request for our target ip and MAC address. Note, in the
  previoues steps, we have revert the target and source ip and MAC address, so this is the oringinal target address.*/
  struct sr_arpreq *request = sr_arpcache_insert(sr_cache,arp_header -> ar_sha,sender_ip);
  /* if request exists send the outstandig packet*/
  if(request) {
    struct sr_packet *packets = request -> packets;
    while(packets) {
      /**/
      /* set the ethernet header from the packets-> buf*/
      sr_ethernet_hdr_t *new_ether_hdr = (sr_ethernet_hdr_t *) packets->buf;
     
      /*cause the packets's buf does not have the MAC address so we need to set it manually*/
      memcpy(new_ether_hdr->ether_dhost, arp_header->ar_sha, ETHER_ADDR_LEN);
      struct sr_if *sr_interface = sr_get_interface(sr,packets->iface);
      memcpy(new_ether_hdr->ether_shost, sr_interface->addr, ETHER_ADDR_LEN);
      /*send the packet as it oringinal should go*/
      sr_send_packet(sr,packets->buf,packets->len,packets->iface);
      packets = packets->next;
    }
    
    /*destroy the arp request, cause it already has been sent out.*/
    sr_arpreq_destroy(sr_cache,request);
  }
  
  

}

void handle_ip(struct sr_instance* sr,uint8_t * packet,unsigned int len,char* interface) {
  assert(sr);
  assert(packet);
  assert(interface);

  printf("Receiving IP Package.\n");
  print_hdr_ip(packet + sizeof(sr_ethernet_hdr_t));

    /* Get the ethernet header. */
  /* sr_ethernet_hdr_t *eth_hdr = get_ethrnet_hdr(packet); */

    /* Get the ip header. */
  sr_ip_hdr_t *ip_hdr = (sr_ip_hdr_t*) (packet + sizeof(sr_ethernet_hdr_t));
    /* Error checking of assigning ip header.*/
  if (!ip_hdr) {
    fprintf(stderr, "Assigning ip header error.\n");
    exit(0);
  }

    /* Checking checksum. */
  uint16_t checksum = ip_hdr -> ip_sum;
  ip_hdr -> ip_sum = 0;
  if (cksum(ip_hdr, sizeof(sr_ip_hdr_t)) != checksum) {
    fprintf(stderr, "Wrong checksum.\n");
    exit(0);
  }
  ip_hdr -> ip_sum = checksum;

    /* Find if the destination of package is this router. */
  struct sr_if *dest_interface = sr_get_interface_by_ip(sr, ip_hdr -> ip_dst);

  
  if (dest_interface) {
    /* Packet destination is this router. */
    printf("Packet for this router.\n");

    switch (ip_hdr -> ip_p)
    {
      case ip_protocol_icmp:
      /*Check if it's ICMP echo request*/
        printf("It's ICMP.");

      sr_icmp_hdr_t *icmp_hdr = (sr_icmp_hdr_t*) (packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
      /* Check if ICMP is echo request. */  
      if(icmp_hdr -> icmp_type == icmp_echo_request) {
        send_icmp_packet(sr, packet, len, interface, icmp_echo_reply, 0);
      } else {
        printf("ICMP package cannot be handled.\n");
      }
      break;
      
      case ip_protocol_tcp:
       /* Send ICMP message type 3 code 3 (Port unreachable)*/ 
        printf("It's TCP.");
        send_icmp_packet(sr, packet, len, interface, icmp_dest_unreachable, icmp_dest_unreachable_port);
      break;

      case ip_protocol_udp:
       /* Send ICMP message type 3 code 3 (Port unreachable)  */  
        printf("It's UDP.");
        send_icmp_packet(sr, packet, len, interface, icmp_dest_unreachable, icmp_dest_unreachable_port);
      break;

      default:
        printf("Cannot handle packet protocol.\n");
      break;
    }
    } else {
      /* // Packet destination is elsewhere.*/
      printf("Packet not for this router.\n");

      ip_hdr -> ip_ttl --;
      if (ip_hdr -> ip_ttl < 1) {
       printf("Packet's TTl decrease to 0, drop the package.\n");
          /*Send ICMP message type 11, code 0 (Time Exceeded)*/
            send_icmp_packet(sr, packet, len, interface, icmp_time_exceeded, icmp_time_exceeded_transit);
          return;
        }

        /* Recalculate the checksum*/
        ip_hdr -> ip_sum = 0;
        ip_hdr -> ip_sum = cksum(ip_hdr, sizeof(sr_ip_hdr_t));


        /* Find the destination router*/
        struct sr_rt *match = sr_find_lpm(sr, ip_hdr -> ip_dst);
        if(match == NULL) {
          /* Send ICMP message type 3, code 0 (Destination net unreachable)*/
          printf("Cannot find destination.\n");
            send_icmp_packet(sr, packet, len, interface, icmp_dest_unreachable, icmp_dest_unreachable_net);

        } else {
        /* Destination has been found, check ARP cache */
        printf("Destination found.\n");

        struct sr_arpcache *sr_cache = &sr->cache;
        struct sr_arpentry *entry = sr_arpcache_lookup(sr_cache, ip_hdr -> ip_dst);

        if (entry) {
         /* Send frame to next hop*/ 
          printf("Found ARP entry in ARP cache, send it to next hop.\n");

          /* struct sr_if *dest_if = sr_get_interface(sr, match -> interface); */

          int status = sr_send_packet(sr, packet, len, match -> interface);
          if (status == -1) {
            fprintf(stderr, "Sending packet error.\n");
          }

        } else {
         /* Send ARP Request*/
          printf("Cannot found ARP entry in ARP cache, send ARP request.\n");
          struct sr_arpreq *req = sr_arpcache_queuereq(sr_cache ,ip_hdr -> ip_dst, packet, len, match -> interface);
          handle_arpreq(sr, req);
        }
      }
  }
}

/* Function that assign packet to ethrnet header. */
sr_ethernet_hdr_t * get_ethrnet_hdr(uint8_t * packet) {
  assert(packet);

  sr_ethernet_hdr_t *ethernet_hdr = (sr_ethernet_hdr_t*) packet;
    /* Error checking.*/
  if (!ethernet_hdr) {
    fprintf(stderr, "Assigning ethernet header error.\n");
    exit(0);
  }
  return ethernet_hdr;
}
