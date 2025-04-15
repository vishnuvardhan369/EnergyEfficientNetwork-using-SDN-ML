//weak client

#include "contiki.h"
#include "net/rpl/rpl.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ip/uip-udp-packet.h"
#include "sys/energest.h"
#include "sys/node-id.h"
#include <stdio.h>

#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678
#define SEND_INTERVAL (60 * CLOCK_SECOND)
#define MAX_PAYLOAD_LEN 40
#define INITIAL_ENERGY 32400000
#define CURRENT_CPU 1.8
#define CURRENT_TX 10.0
#define CURRENT_RX 19.7
#define VOLTAGE 3.0
#define TICKS_PER_SECOND 32768

static struct uip_udp_conn *client_conn;
static uip_ipaddr_t server_ipaddr;

PROCESS(udp_client_weak_process, "UDP client weak process");
AUTOSTART_PROCESSES(&udp_client_weak_process);

static void set_global_address(void) {
  uip_ipaddr_t ipaddr;
  uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);
  uip_ip6addr(&server_ipaddr, 0xaaaa, 0, 0, 0, 0, 0x00ff, 0xfe00, 1);
}

static void send_packet(void *ptr) {
  static char buf[MAX_PAYLOAD_LEN];
  static int seq_id = 0;

  seq_id++;
  buf[0] = 'H';
  buf[1] = ' ';
  buf[2] = '0' + (seq_id % 10);
  buf[3] = 0;

  rpl_dag_t *dag = rpl_get_any_dag();
  rpl_parent_t *parent = dag ? dag->preferred_parent : NULL;
  uip_ds6_nbr_t *nbr = parent ? rpl_get_nbr(parent) : NULL;
  uint16_t etx = (nbr && parent) ? nbr->link_metric : 0;
  uint8_t hop_count = dag ? dag->rank / 256 : 0;

  static uint64_t last_cpu = 0, last_tx = 0, last_rx = 0;
  energest_flush();
  uint64_t cpu = energest_type_time(ENERGEST_TYPE_CPU);
  uint64_t tx = energest_type_time(ENERGEST_TYPE_TRANSMIT);
  uint64_t rx = energest_type_time(ENERGEST_TYPE_LISTEN);
  uint64_t total_energy = (((cpu - last_cpu) * CURRENT_CPU +
                           (tx - last_tx) * CURRENT_TX +
                           (rx - last_rx) * CURRENT_RX) * VOLTAGE * 1000) / TICKS_PER_SECOND / 1000;
  static uint64_t used_energy = 0;
  used_energy += total_energy;
  uint64_t residual_energy = INITIAL_ENERGY > used_energy ? INITIAL_ENERGY - used_energy : 0;
  last_cpu = cpu;
  last_tx = tx;
  last_rx = rx;

  int node_degree = uip_ds6_nbr_num();

  printf("CW,%u,%u,%u,%lu,%d,%d\n",
         node_id, etx, hop_count, (unsigned long)residual_energy, node_degree, seq_id);

  uip_udp_packet_sendto(client_conn, buf, 3, &server_ipaddr, UIP_HTONS(UDP_SERVER_PORT));
}

PROCESS_THREAD(udp_client_weak_process, ev, data) {
  static struct etimer periodic;
  PROCESS_BEGIN();

  set_global_address();
  client_conn = udp_new(NULL, UIP_HTONS(UDP_SERVER_PORT), NULL);
  udp_bind(client_conn, UIP_HTONS(UDP_CLIENT_PORT));

  etimer_set(&periodic, SEND_INTERVAL);

  while(1) {
    PROCESS_WAIT_EVENT();
    if(etimer_expired(&periodic)) {
      send_packet(NULL);
      etimer_reset(&periodic);
    }
  }

  PROCESS_END();
}