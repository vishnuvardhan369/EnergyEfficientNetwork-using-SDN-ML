#include "net/rpl/rpl.h"
#include "net/rpl/rpl-private.h"
#include "net/ipv6/uip-ds6-nbr.h"
#include "sys/energest.h"

#define RPL_OF_OCP 0xFF
#define MAX_ETX 2500.0f
#define MAX_HOP_COUNT 15.0f
#define MAX_ENERGY 50000000.0f
#define MAX_DEGREE 10.0f

static uint32_t get_residual_energy(void) {
  static uint32_t last_energy = 0;
  uint32_t cpu = energest_type_time(ENERGEST_TYPE_CPU);
  uint32_t lpm = energest_type_time(ENERGEST_TYPE_LPM);
  uint32_t tx = energest_type_time(ENERGEST_TYPE_TRANSMIT);
  uint32_t rx = energest_type_time(ENERGEST_TYPE_LISTEN);
  uint32_t total = (cpu + lpm + tx + rx) * 1000;
  uint32_t consumed = total - last_energy;
  last_energy = total;
  return MAX_ENERGY > consumed ? MAX_ENERGY - consumed : 0;
}

static uint8_t get_degree(void) {
  int count = 0;
  uip_ds6_nbr_t *nbr;
  for(nbr = uip_ds6_nbr_head(); nbr != NULL; nbr = uip_ds6_nbr_next(nbr)) {
    count++;
  }
  return count > MAX_DEGREE ? MAX_DEGREE : count;
}

static void reset(rpl_dag_t *dag) {
}

static rpl_rank_t calculate_rank(rpl_parent_t *parent, rpl_rank_t base_rank) {
  if(!parent) {
    return INFINITE_RANK;
  }
  uint16_t etx = 256; /* Fixed ETX due to unavailable parent fields */
  uint8_t hop_count = parent->rank / 256 + 1;
  uint32_t energy = get_residual_energy();
  uint8_t degree = get_degree();

  float norm_etx = etx / MAX_ETX;
  float norm_hop = hop_count / MAX_HOP_COUNT;
  float norm_energy = (MAX_ENERGY - energy) / MAX_ENERGY;
  float norm_degree = degree / MAX_DEGREE;

  float rank_score = (0.328573f * norm_etx) +
                     (0.158970f * norm_hop) +
                     (0.288030f * norm_energy) +
                     (0.224427f * norm_degree);

  rpl_rank_t rank = (rpl_rank_t)(rank_score * 256);
  return rank < 256 ? 256 : rank;
}

static rpl_parent_t *best_parent(rpl_parent_t *p1, rpl_parent_t *p2) {
  rpl_rank_t rank1 = calculate_rank(p1, 0);
  rpl_rank_t rank2 = calculate_rank(p2, 0);
  return (rank1 < rank2) ? p1 : p2;
}

static void update_metric_container(rpl_instance_t *instance) {
  /* Rely on client logs for metrics */
}

rpl_of_t rpl_mlof = {
  reset,
  NULL,
  best_parent,
  NULL,
  calculate_rank,
  update_metric_container,
  RPL_OF_OCP
};