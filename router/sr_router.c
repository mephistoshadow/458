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
    if (len != sizeof(sr_ethernet_hdr_t)) {
      printf("the lengh does not meet the minimum length of ethernet\n");

    }

    uint16_t e_type = ethertype(ethernet_hdr);
    if(e_type == ethertype_ip) {
      handle_ip();
    } else if (e_type == ethertype_arp) {
      handle_arp_total(sr,packet,len,interface);
    }

  }
  
  void handle_arp_total(struct sr_instance* sr,uint8_t * packet,unsigned int len,char* interface){
   sr_arp_hdr_t *arp_hdr = (sr_arp_hdr_t*) (packet + sizeof(sr_ethernet_hdr_t));
   if(arp_hdr ->ar_op == arp_op_request) {
    arp_request(sr,packet,len,interface);
  }else if(arp_hdr ->ar_op == arp_op_reply) {
   handle_arp_reply(sr,packet,len,interface);
 }

}

void arp_requet(struct sr_instance* sr,uint8_t * packet,unsigned int len,char* interface){
  struct sr_if *sr_interface = sr_get_interface(sr,interface);

  sr_ethernet_hdr_t *ethernet_hdr = (sr_ethernet_hdr_t*) packet;
  sr_arp_hdr_t *arp_hdr = (sr_arp_hdr_t*) (packet + sizeof(sr_ethernet_hdr_t));
      // get the new packet length.
  int length_new_packet = sizeof(sr_ethernet_hdr_t) + sizeof(sr_arp_hdr_t);
      // malloc the send back packet's memory;
  uint8_t  *back_packet = (uint8_t*)malloc(length_new_packet);
      // assign the new thernet header;
  sr_ethernet_hdr_t* new_ethernet_hdr = (sr_ethernet_hdr_t*) back_packet;
  memcpy(new_ethernet_hdr -> ether_dhost,ethernet_hdr -> ether_shost,sizeof(ether_shost));
  memcpy(new_ethernet_hdr -> ether_shost,sr_interfac -> addr,sizeof(ether_dhost));
  new_ethernet_hdr -> ether_type = ethernet_hdr -> ether_type;
      // assign the new arp header for send back.
  sr_arp_hdr_t *arp_hdr = (sr_arp_hdr_t*) (back_packet + sizeof(sr_ethernet_hdr_t));
  memcpy(new_ethernet_hdr -> ether_dhost,ethernet_hdr -> ether_shost,sizeof(ether_shost));




}




