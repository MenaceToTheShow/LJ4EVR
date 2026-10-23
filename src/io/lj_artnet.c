/* lj_artnet.c - Art-Net 4 output and the input hand-off (io/lj_artnet.h). W5 CONTRACT STUB: the artnet track replaces
 * every body (docs/w5-contracts.md seam 1). Until then every link stays on sACN and nothing opens port 6454. */
#include <string.h>
#include "io/lj_artnet.h"

int  lj_artnet_open(const lj_sacn_cfg *cfg) { (void)cfg; return 0; }
void lj_artnet_set(int link, const uint8_t slots[LJ_LINK_CH]) { (void)link; (void)slots; }
void lj_artnet_pump(uint64_t now_us) { (void)now_us; }
void lj_artnet_close(void) {}
void lj_artnet_stats_get(lj_artnet_stats *out) { if (out) memset(out, 0, sizeof *out); }
int  lj_artnet_nodes(lj_artnet_node *out, int max) { (void)out; (void)max; return 0; }
void lj_artnet_feed(const uint8_t *pkt, size_t n, uint32_t src_ip, uint64_t now_us) { (void)pkt; (void)n; (void)src_ip; (void)now_us; }

int lj_artnet_pack_dmx(uint8_t out[LJ_ARTNET_DMX_LEN], uint8_t seq, uint8_t physical, uint16_t port_addr, const uint8_t slots[LJ_LINK_CH])
{ (void)out; (void)seq; (void)physical; (void)port_addr; (void)slots; return -1; }
int lj_artnet_pack_poll(uint8_t *out, size_t cap) { (void)out; (void)cap; return -1; }
int lj_artnet_opcode(const uint8_t *pkt, size_t n) { (void)pkt; (void)n; return -1; }
int lj_artnet_parse_dmx(const uint8_t *pkt, size_t n, uint16_t *port_addr, uint8_t *seq, uint8_t slots[LJ_LINK_CH], int *nslots)
{ (void)pkt; (void)n; (void)port_addr; (void)seq; (void)slots; (void)nslots; return -1; }
int lj_artnet_parse_reply(const uint8_t *pkt, size_t n, lj_artnet_node *out) { (void)pkt; (void)n; (void)out; return -1; }

void lj_artnet_cfg_load(lj_ini *hardware_ini, lj_sacn_cfg *cfg)
{
    int i;
    (void)hardware_ini;
    if (!cfg) return;
    for (i = 0; i < LJ_LINKS; i++) { cfg->proto[i] = LJ_OUT_SACN; cfg->artnet_addr[i] = (uint16_t)(i + 1); }
    cfg->artnet_in = 0;
}
void lj_artnet_cfg_save(lj_ini *hardware_ini, const lj_sacn_cfg *cfg) { (void)hardware_ini; (void)cfg; }
const char *lj_artnet_cfg_validate(const lj_sacn_cfg *cfg) { (void)cfg; return NULL; }
