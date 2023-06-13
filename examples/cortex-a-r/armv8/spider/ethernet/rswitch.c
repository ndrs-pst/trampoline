#include "tpl_os.h"
#include "utils.h"
#include "rswitch.h"
#include "rswitch_regs.h"
#include "err_codes.h"
#include <string.h>

#define PORT_TSNA_N     3
#define PORT_GWCA_N     2
#define TOT_PORT_NUM    (PORT_TSNA_N + PORT_GWCA_N)

#define RSWITCH_GWCA_IDX_TO_HW_NUM(i)   ((i) + PORT_TSNA_N)
#define RSWITCH_HW_NUM_TO_GWCA_IDX(i)   ((i) - PORT_TSNA_N)

#define TSNA0_PORT_NUM   0
#define TSNA1_PORT_NUM   1
#define TSNA2_PORT_NUM   2
#define GWCA0_PORT_NUM   3
#define GWCA1_PORT_NUM   4

#define TSN_PORT_IN_USE   TSNA0_PORT_NUM
#define GWCA_PORT_IN_USE  GWCA1_PORT_NUM

/* GWCA */
enum rswitch_gwca_mode {
    GWMC_OPC_RESET,
    GWMC_OPC_DISABLE,
    GWMC_OPC_CONFIG,
    GWMC_OPC_OPERATION,
};
#define GWMS_OPS_MASK   GWMC_OPC_OPERATION
#define GWVCC_VEM_SC_TAG    (0x3 << 16)
#define GWMTIRM_MTIOG       BIT(0)
#define GWMTIRM_MTR         BIT(1)
#define GWARIRM_ARIOG       BIT(0)
#define GWARIRM_ARR         BIT(1)

#define GWDCC_BALR          BIT(24)
#define GWDCC_DCP(q, idx)   ((q + (idx * 2)) << 16)
#define GWDCC_DQT           BIT(11)
#define GWDCC_ETS           BIT(9)
#define GWDCC_EDE           BIT(8)
#define GWDCC_OFFS(chain)   (GWDCC0 + (chain) * 4)

enum DIE_DT {
    /* Frame data */
    DT_FSINGLE = 0x80,
    DT_FSTART = 0x90,
    DT_FMID = 0xA0,
    DT_FEND = 0xB8,

    /* Chain control */
    DT_LEMPTY = 0xC0,
    DT_EEMPTY = 0xD0,
    DT_LINKFIX = 0x00,
    DT_LINK = 0xE0,
    DT_EOS = 0xF0,
    /* HW/SW arbitration */
    DT_FEMPTY = 0x40,
    DT_FEMPTY_IS = 0x10,
    DT_FEMPTY_IC = 0x20,
    DT_FEMPTY_ND = 0x38,
    DT_FEMPTY_START = 0x50,
    DT_FEMPTY_MID = 0x60,
    DT_FEMPTY_END = 0x70,

    DT_MASK = 0xF0,
    DIE = 0x08,     /* Descriptor Interrupt Enable */
};

struct rswitch_desc {
    uint16 info_ds;	/* Descriptor size */
    uint8 die_dt;	/* Descriptor interrupt enable and type */
    uint8  dptrh;	/* Descriptor pointer MSB */
    uint32 dptrl;	/* Descriptor pointer LSW */
} __attribute__((packed));

struct rswitch_ts_desc {
    uint16 info_ds;	/* Descriptor size */
    uint8 die_dt;	/* Descriptor interrupt enable and type */
    uint8  dptrh;	/* Descriptor pointer MSB */
    uint32 dptrl;	/* Descriptor pointer LSW */
    uint32 ts_nsec;
    uint32 ts_sec;
} __attribute__((packed));

struct rswitch_ext_desc {
    uint16 info_ds;	/* Descriptor size */
    uint8 die_dt;	/* Descriptor interrupt enable and type */
    uint8  dptrh;	/* Descriptor pointer MSB */
    uint32 dptrl;	/* Descriptor pointer LSW */
    uint64 info1;
} __attribute__((packed));

struct rswitch_ext_ts_desc {
    uint16 info_ds;	/* Descriptor size */
    uint8 die_dt;	/* Descriptor interrupt enable and type */
    uint8  dptrh;	/* Descriptor pointer MSB */
    uint32 dptrl;	/* Descriptor pointer LSW */
    uint64 info1;
    uint32 ts_nsec;
    uint32 ts_sec;
} __attribute__((packed));

#define NUM_DEVICES         3
#define NUM_CHAINS_PER_NDEV 2

#define RSWITCH_MAX_NUM_CHAINS  128
#define RSWITCH_NUM_IRQ_REGS    (RSWITCH_MAX_NUM_CHAINS / BITS_PER_TYPE(uint32))

#define MAC_ADDRESS_LEN     6
struct rswitch_etha {
    uint8 port_num;
    uint32 base_addr;
    uint8 mac_addr[MAC_ADDRESS_LEN];
    int speed;
} etha_props[PORT_TSNA_N] = 
{
    { .port_num = TSNA0_PORT_NUM, .base_addr = RSWITCH_ETHA0_ADDR },
    { .port_num = TSNA1_PORT_NUM, .base_addr = RSWITCH_ETHA1_ADDR },
    { .port_num = TSNA2_PORT_NUM, .base_addr = RSWITCH_ETHA2_ADDR },
};

enum rswitch_etha_mode {
    EAMC_OPC_RESET,
    EAMC_OPC_DISABLE,
    EAMC_OPC_CONFIG,
    EAMC_OPC_OPERATION,
};
#define EAMS_OPS_MASK   EAMC_OPC_OPERATION
#define EAVCC_VEM_SC_TAG    (0x3 << 16)

#define MPIC_PIS_MII	0x00
#define MPIC_PIS_GMII	0x02
#define MPIC_PIS_XGMII	0x04
#define MPIC_LSC_SHIFT	3
#define MPIC_LSC_10M	(0 << MPIC_LSC_SHIFT)
#define MPIC_LSC_100M	(1 << MPIC_LSC_SHIFT)
#define MPIC_LSC_1G	(2 << MPIC_LSC_SHIFT)
#define MPIC_LSC_2_5G	(3 << MPIC_LSC_SHIFT)
#define MPIC_LSC_5G	(4 << MPIC_LSC_SHIFT)
#define MPIC_LSC_10G	(5 << MPIC_LSC_SHIFT)
#define MPIC_PSMCS_SHIFT	16
#define MPIC_PSMCS_MASK		GENMASK(22, MPIC_PSMCS_SHIFT)
#define MPIC_PSMCS(val)		((val) << MPIC_PSMCS_SHIFT)
#define MPIC_PSMHT_SHIFT	24
#define MPIC_PSMHT_MASK		GENMASK(26, MPIC_PSMHT_SHIFT)
#define MPIC_PSMHT(val)		((val) << MPIC_PSMHT_SHIFT)
#define MPSM_MFF_C45		BIT(2)
#define MLVC_PLV	BIT(16)
#define MDIO_READ_C45		0x03
#define MDIO_WRITE_C45		0x01
#define MII_ADDR_C45		(1<<30)
#define MII_DEVADDR_C45_SHIFT	16
#define MII_REGADDR_C45_MASK	GENMASK(15, 0)
#define MMIS1_PAACS             BIT(2) /* Address */
#define MMIS1_PWACS             BIT(1) /* Write */
#define MMIS1_PRACS             BIT(0) /* Read */
#define MMIS1_CLEAR_FLAGS       0xf
#define MPSM_PSME		BIT(0)
#define MPSM_MFF_C45		BIT(2)
#define MPSM_PDA_SHIFT		3
#define MPSM_PDA_MASK		GENMASK(7, MPSM_PDA_SHIFT)
#define MPSM_PDA(val)		((val) << MPSM_PDA_SHIFT)
#define MPSM_PRA_SHIFT		8
#define MPSM_PRA_MASK		GENMASK(12, MPSM_PRA_SHIFT)
#define MPSM_PRA(val)		((val) << MPSM_PRA_SHIFT)
#define MPSM_POP_SHIFT		13
#define MPSM_POP_MASK		GENMASK(14, MPSM_POP_SHIFT)
#define MPSM_POP(val)		((val) << MPSM_POP_SHIFT)
#define MPSM_PRD_SHIFT		16
#define MPSM_PRD_MASK		GENMASK(31, MPSM_PRD_SHIFT)
#define MPSM_PRD_WRITE(val)	((val) << MPSM_PRD_SHIFT)
#define MPSM_PRD_READ(val)	((val) & MPSM_PRD_MASK >> MPSM_PRD_SHIFT)

#define TX_RX_RING_SIZE     32
#define PKT_BUF_SZ          1584
#define PKT_BUF_SZ_ALIGN    1792 /* 0x700 to keep elements aligned to the value
                                    specified by RSWITCH_ALIGN */
#define RSWITCH_ALIGN       1 /* kenel uses 128, so 7 bits (=log_2(128)) are enough
                               * to represent this value, so we round up to 1 byte. */

/* These elements should either be rswitch_ext_desc or rswitch_ext_ts_desc
 * depending if gptp is used or not. */
struct rswitch_ext_ts_desc rx_ring[TX_RX_RING_SIZE];
struct rswitch_ext_desc tx_ring[TX_RX_RING_SIZE];

uint8 rx_data_buff[TX_RX_RING_SIZE][PKT_BUF_SZ_ALIGN] __attribute__((aligned(RSWITCH_ALIGN)));
uint8 tx_data_buff[TX_RX_RING_SIZE][PKT_BUF_SZ_ALIGN] __attribute__((aligned(RSWITCH_ALIGN)));

struct rswitch_gwca_chain {
    uint8 chain_index;
    uint8 dir_tx;
    // bool gptp;
    union {
        struct rswitch_ext_desc *ring;
        struct rswitch_ext_ts_desc *ts_ring;
    };
    uint32 num_ring;
    uint32 cur;
    uint32 dirty;
    uint8 *data_buffers[TX_RX_RING_SIZE];
};

struct rswitch_gwca {
    uint8 port_num;
    uint32 base_addr;
    struct rswitch_gwca_chain rx_chain;
    struct rswitch_gwca_chain tx_chain;
    uint32 irq_queue;
    int speed;
} gwca_props[PORT_GWCA_N] = {
    { .port_num = GWCA0_PORT_NUM, .base_addr = RSWITCH_GWCA0_ADDR },
    { .port_num = GWCA1_PORT_NUM, .base_addr = RSWITCH_GWCA1_ADDR },
};

struct rswitch_device {
    struct rswitch_desc base_descriptors[NUM_CHAINS_PER_NDEV];
    struct rswitch_etha *etha;
    struct rswitch_gwca *gwca;
    uint32 fwd_base_addr;
    uint32 coma_base_addr;
} rsw_dev = {
    .fwd_base_addr = RSWITCH_FWD_ADDR,
    .coma_base_addr = RSWITCH_COMA_ADDR,
    .etha = &etha_props[TSN_PORT_IN_USE],
    .gwca = &gwca_props[GWCA_PORT_IN_USE],
};

/* MFWD */
#define FWPC0_LTHTA     BIT(0)
#define FWPC0_IP4UE     BIT(3)
#define FWPC0_IP4TE     BIT(4)
#define FWPC0_IP4OE     BIT(5)
#define FWPC0_L2SE      BIT(9)
#define FWPC0_IP4EA     BIT(10)
#define FWPC0_IPDSA     BIT(12)
#define FWPC0_IPHLA     BIT(18)
#define FWPC0_MACSDA    BIT(20)
#define FWPC0_MACHLA    BIT(26)
#define FWPC0_MACHMA    BIT(27)
#define FWPC0_VLANSA    BIT(28)

#define FWPC0(i)            (FWPC00 + (i) * 0x10)
#define FWPC1(i)            (FWPC10 + (i) * 0x10)
#define FWPC0_DEFAULT       (FWPC0_LTHTA | FWPC0_IP4UE | FWPC0_IP4TE | \
                            FWPC0_IP4OE | FWPC0_L2SE | FWPC0_IP4EA | \
                            FWPC0_IPDSA | FWPC0_IPHLA | FWPC0_MACSDA | \
                            FWPC0_MACHLA |	FWPC0_MACHMA | FWPC0_VLANSA)
#define FWPC1_DDE           BIT(0)
#define	FWPBFC(i)           (FWPBFCi + (i) * 0x10)
#define FWPBFCSDC(j, i)     (FWPBFCSDC00 + (i) * 0x10 + (j) * 0x04)

/* Interrupts */
#define GWCA0_RX_TX_IRQ       312
#define GWCA1_RX_TX_IRQ       320
#define GWCA0_RX_TS_IRQ       328
#define GWCA1_RX_TS_IRQ       330
#define COMA_ERR_IRQ          290
#define GWCA0_ERR_IRQ         291
#define GWCA1_ERR_IRQ         292
#define ETHA0_ERR_IRQ         293
#define ETHA1_ERR_IRQ         294
#define ETHA2_ERR_IRQ         295

#define RSWITCH_TIMEOUT_MS	1000
static int rswitch_reg_wait(uint32 addr, uint32 mask, uint32 expected)
{
    int i;

    for (i = 0; i < RSWITCH_TIMEOUT_MS; i++) {
        if ((reg_read32(addr) & mask) == expected)
            return 0;

        ms_delay(1);
    }

    return ERR_TIMEOUT;
}

static void rswitch_modify(uint32 addr, uint32 clear, uint32 set)
{
    uint32 val = reg_read32(addr) & ~clear;
    reg_write32(val | set, addr);
}

static int rswitch_mii_set_access(struct rswitch_etha *etha, uint8 read,
                                    uint32 phyad, uint32 devad, uint32 regad,
                                    uint16 *data)
{
    int pop = read ? MDIO_READ_C45 : MDIO_WRITE_C45;
    uint32 val;
    int ret;

    /* Clear completion flags */
    reg_write32(MMIS1_CLEAR_FLAGS, etha->base_addr + MMIS1);

    /* Submit address to PHY (MDIO_ADDR_C45 << 13) */
    val = MPSM_PSME | MPSM_MFF_C45;
    reg_write32((regad << 16) | (devad << 8) | (phyad << 3) | val,
                etha->base_addr + MPSM);

    CHECK_RET(rswitch_reg_wait(etha->base_addr + MMIS1, MMIS1_PAACS, MMIS1_PAACS));

    /* Clear address completion flag */
    rswitch_modify(etha->base_addr + MMIS1, MMIS1_PAACS, MMIS1_PAACS);

    /* Read/Write PHY register */
    if (read) {
        reg_write32((pop << 13) | (devad << 8) | (phyad << 3) | val,
                        etha->base_addr + MPSM);

        CHECK_RET(rswitch_reg_wait(etha->base_addr + MMIS1, MMIS1_PRACS, MMIS1_PRACS));

        /* Read data */
        ret = (reg_read32(etha->base_addr + MPSM) & MPSM_PRD_MASK) >> 16;
        *data = ret;

        /* Clear read completion flag */
        rswitch_modify(etha->base_addr + MMIS1, MMIS1_PRACS, MMIS1_PRACS);
    } else {
        reg_write32((*data << 16) | (pop << 13) | (devad << 8) | (phyad << 3) | val,
                    etha->base_addr + MPSM);

        ret = rswitch_reg_wait(etha->base_addr + MMIS1, MMIS1_PWACS, MMIS1_PWACS);
    }

    return 0;
}

static int rswitch_mii_read(struct rswitch_etha *etha, uint32 phyaddr, 
                                uint32 devad, uint32 regad, uint32 *data)
{
    uint16 tmp;
    int ret;
    ret = rswitch_mii_set_access(etha, TRUE, phyaddr, devad, regad, &tmp);
    *data = tmp;

    return ret;
}

static int rswitch_mii_write(struct rswitch_etha *etha, uint32 phyaddr, 
                                uint32 devad, uint32 regad, uint32 *data)
{
    uint16 tmp = (uint16) *data;
    return rswitch_mii_set_access(etha, FALSE, phyaddr, devad, regad, &tmp);
}

static int rswitch_agent_clock_is_enabled(int port)
{
    uint32 val = reg_read32(rsw_dev.coma_base_addr + RCEC);

    if (val & RCEC_RCE)
        return (val & BIT(port)) ? TRUE : FALSE;
    else
        return FALSE;
}

static void rswitch_agent_clock_ctrl(int port, int enable)
{
    uint32 val;

    if (enable) {
        val = reg_read32(rsw_dev.coma_base_addr + RCEC);
        reg_write32(val | RCEC_RCE | BIT(port),
                    rsw_dev.coma_base_addr + RCEC);
    } else {
        val = reg_read32(RCDC);
        reg_write32(val | BIT(port), RCDC);
    }
}

// mac is supposed to be an array of 6 bytes
static void rswitch_etha_read_mac_address(struct rswitch_etha *eth_dev)
{
    uint32 mrmac0 = reg_read32(eth_dev->base_addr + MRMAC0);
    uint32 mrmac1 = reg_read32(eth_dev->base_addr + MRMAC1);

    eth_dev->mac_addr[0] = (mrmac0 >>  8) & 0xFF;
    eth_dev->mac_addr[1] = (mrmac0 >>  0) & 0xFF;
    eth_dev->mac_addr[2] = (mrmac1 >> 24) & 0xFF;
    eth_dev->mac_addr[3] = (mrmac1 >> 16) & 0xFF;
    eth_dev->mac_addr[4] = (mrmac1 >>  8) & 0xFF;
    eth_dev->mac_addr[5] = (mrmac1 >>  0) & 0xFF;
}

/* This function sets only data in the global strucure, not on the hardware */
static void rswitch_set_mac_address(struct rswitch_etha *eth_dev)
{
    int i;

    // Values from MCAL
    static const uint8 predefined_mac_address[PORT_TSNA_N][6] = {
        {0x74U, 0x90U, 0x50U, 0x00U, 0x00U, 0x00U},
        {0x74U, 0x90U, 0x50U, 0x00U, 0x00U, 0x01U},
        {0x74U, 0x90U, 0x50U, 0x00U, 0x00U, 0x02U},
    };

    // Set only if not already valid (at least 1 value != 0)
    for (i = 0; i < MAC_ADDRESS_LEN; i++) {
        if (eth_dev->mac_addr[i] != 0) {
            return;
        }
    }

    /* The following works only because TSN port's numbers starts from 0 as
     * if they were indexes. */
    for (i = 0; i < MAC_ADDRESS_LEN; i++) {
        eth_dev->mac_addr[i] = predefined_mac_address[eth_dev->port_num][i];
    }
}

static void rswitch_soft_reset(void)
{
    reg_write32(RRC_RR, rsw_dev.coma_base_addr + RRC);
    reg_write32(RRC_RR_CLR, rsw_dev.coma_base_addr + RRC);

    /* Clock initialization and module reset have already been done before
     * in utils.c */
}

static int rswitch_gwca_change_mode(struct rswitch_gwca *gwca,
                                    enum rswitch_gwca_mode mode)
{
    int ret;

    /* Enable clock */
    if (!rswitch_agent_clock_is_enabled(gwca->port_num))
        rswitch_agent_clock_ctrl(gwca->port_num, 1);

    reg_write32(mode, gwca->base_addr + GWMC);

    ret = rswitch_reg_wait(gwca->base_addr + GWMS, GWMS_OPS_MASK, mode);

    /* Disable clock */
    if (mode == GWMC_OPC_DISABLE)
        rswitch_agent_clock_ctrl(gwca->port_num, 0);

    return ret;
}

static int rswitch_gwca_mcast_table_reset(struct rswitch_gwca *gwca)
{
    reg_write32(GWMTIRM_MTIOG, gwca->base_addr + GWMTIRM);
    return rswitch_reg_wait(gwca->base_addr + GWMTIRM, GWMTIRM_MTR, GWMTIRM_MTR);
}

static int rswitch_gwca_axi_ram_reset(struct rswitch_gwca *gwca)
{
    reg_write32(GWARIRM_ARIOG, gwca->base_addr + GWARIRM);
    return rswitch_reg_wait(gwca->base_addr + GWARIRM, GWARIRM_ARR, GWARIRM_ARR);
}

static void rswitch_gwca_set_rate_limit(struct rswitch_gwca *gwca)
{
    uint32 gwgrlulc, gwgrlc;

    switch (gwca->speed) {
    case 1000:
        gwgrlulc = 0x0000005f;
        gwgrlc = 0x00010260;
        break;
    default:
        debug_msg("%s: This rate is not supported (%d)\n", __func__, gwca->speed);
        return;
    }

    reg_write32(gwgrlulc, gwca->base_addr + GWGRLULC);
    reg_write32(gwgrlc, gwca->base_addr + GWGRLC);
}

static int rswitch_gwca_hw_init(struct rswitch_device *rswitch)
{
    int ret, i;
    uint64 descriptors_addr = (uint32) rswitch->base_descriptors;
    struct rswitch_gwca *gwca = rswitch->gwca;

    /* Note: this is setting the base descriptors for all the devices but
     * perhaps we should just set the one we are using. Setting for all
     * might impact what Linux/Uboot has already configured on its side. */
    for (i = 0; i < NUM_CHAINS_PER_NDEV * NUM_DEVICES; i++) {
        rswitch->base_descriptors[i].die_dt = DT_EOS;
    }

    CHECK_RET(rswitch_gwca_change_mode(gwca, GWMC_OPC_DISABLE));
    CHECK_RET(rswitch_gwca_change_mode(gwca, GWMC_OPC_CONFIG));
    CHECK_RET(rswitch_gwca_mcast_table_reset(gwca));
    CHECK_RET(rswitch_gwca_axi_ram_reset(gwca));
    
    /* Full setting flow */
    reg_write32(GWVCC_VEM_SC_TAG, gwca->base_addr + GWVCC);
    reg_write32(0, gwca->base_addr + GWTTFC);
    reg_write32(0x00, gwca->base_addr + GWDCBAC1);
    reg_write32((descriptors_addr >> 32) & 0xFFFFFFFF, gwca->base_addr + GWDCBAC0);

    gwca->speed = 1000;
    rswitch_gwca_set_rate_limit(gwca);

    CHECK_RET(rswitch_gwca_change_mode(gwca, GWMC_OPC_DISABLE));
    CHECK_RET(rswitch_gwca_change_mode(gwca, GWMC_OPC_OPERATION));

    return 0;
}

static void rswitch_gwca_chain_format(struct rswitch_device *rswitch,
                                      struct rswitch_gwca_chain *c)
{
    struct rswitch_ext_desc *ring;
    struct rswitch_desc *base_desc;
    int tx_ring_size = sizeof(*ring) * c->num_ring;
    int i;

    memset(c->ring, 0, tx_ring_size);
    for (i = 0, ring = c->ring; i < c->num_ring; i++, ring++) {
        if (!c->dir_tx) {
            ring->info_ds = PKT_BUF_SZ;
            ring->dptrl = (uint32) c->data_buffers[i];
            ring->dptrh = 0x00;
            ring->die_dt = DT_FEMPTY | DIE;
        } else {
            ring->die_dt = DT_EEMPTY | DIE;
        }
    }

    /* Close the loop */
    ring->dptrl = (uint32) c->ring;
    ring->dptrh = 0x00;
    ring->die_dt = DT_LINKFIX;

    /* Configure the base descriptor */
    base_desc = &rswitch->base_descriptors[c->chain_index];
    base_desc->die_dt = DT_LINKFIX;
    base_desc->dptrl = (uint32) c->ring;
    base_desc->dptrh = 0x00;

    /* FIXME: GWDCC_DCP */
    reg_write32(GWDCC_BALR | (c->dir_tx ? GWDCC_DQT : 0) | GWDCC_EDE,
                rswitch->gwca->base_addr + GWDCC_OFFS(c->chain_index));
}

static __unused void rswitch_gwca_chain_ts_format(struct rswitch_device *rswitch,
                                         struct rswitch_gwca_chain *c)
{
    struct rswitch_ext_ts_desc *ring;
    struct rswitch_desc *base_desc;
    int tx_ring_size = sizeof(*ring) * c->num_ring;
    int i;

    memset(c->ring, 0, tx_ring_size);
    for (i = 0, ring = c->ts_ring; i < c->num_ring; i++, ring++) {
        if (!c->dir_tx) {
            ring->info_ds = PKT_BUF_SZ;
            ring->dptrl = (uint32) c->data_buffers[i];
            ring->dptrh = 0x00;
            ring->die_dt = DT_FEMPTY | DIE;
        } else {
            ring->die_dt = DT_EEMPTY | DIE;
        }
    }

    /* Close the loop */
    ring->dptrl = (uint32) c->ring;
    ring->dptrh = 0x00;
    ring->die_dt = DT_LINKFIX;

    /* Configure the base descriptor */
    base_desc = &rswitch->base_descriptors[c->chain_index];
    base_desc->die_dt = DT_LINKFIX;
    base_desc->dptrl = (uint32) c->ring;
    base_desc->dptrh = 0x00;

    /* FIXME: GWDCC_DCP */
    reg_write32(GWDCC_BALR | (c->dir_tx ? GWDCC_DQT : 0) | GWDCC_ETS | GWDCC_EDE,
                rswitch->gwca->base_addr + GWDCC_OFFS(c->chain_index));
}

static int rswitch_bpool_config(struct rswitch_device *rswitch)
{
    uint32 val;

    val = reg_read32(rswitch->coma_base_addr + CABPIRM);
    if (val & CABPIRM_BPR)
        return 0;

    reg_write32(CABPIRM_BPIOG, rswitch->coma_base_addr + CABPIRM);
    return rswitch_reg_wait(rswitch->coma_base_addr + CABPIRM, CABPIRM_BPR, CABPIRM_BPR);
}

static void rswitch_fwd_init(struct rswitch_device *rswitch)
{
    int i;
    int eth_port_num = rswitch->etha->port_num;
    int gwca_port_num = rswitch->gwca->port_num;
    int gwca_idx = RSWITCH_HW_NUM_TO_GWCA_IDX(gwca_port_num);

    /* Note: this configures FWD for all the ports, but this might conflict
     * with previous configurations... */
    for (i = 0; i < TOT_PORT_NUM; i++) {
        reg_write32(FWPC0_DEFAULT, rswitch->fwd_base_addr + FWPC0(i));
        reg_write32(0, rswitch->fwd_base_addr + FWPBFC(i));
    }

    /* Static routing between the specified ETHA and GWCA ports. */
    reg_write32(rswitch->gwca->rx_chain.chain_index,
               rswitch->fwd_base_addr + FWPBFCSDC(gwca_idx, eth_port_num));
    reg_write32(BIT(rswitch->gwca->port_num),
                rswitch->fwd_base_addr + FWPBFC(eth_port_num));

    /* For GWCA */
    reg_write32(FWPC0_DEFAULT, rswitch->fwd_base_addr + FWPC0(gwca_port_num));
    reg_write32(FWPC1_DDE, rswitch->fwd_base_addr + FWPC1(gwca_port_num));
    reg_write32(0, rswitch->fwd_base_addr + FWPBFC(gwca_port_num));
    reg_write32(eth_port_num, rswitch->fwd_base_addr + FWPBFC(gwca_port_num));
}

static __unused void rswitch_get_data_irq_status(struct rswitch_device *rswitch, uint32 *dis)
{
    int i;

    for (i = 0; i < RSWITCH_NUM_IRQ_REGS; i++) {
        dis[i] = reg_read32(rswitch->gwca->base_addr + GWDIS0 + i * 0x10);
    }
}

static void rswitch_enadis_data_irq(struct rswitch_gwca *gwca, int index,
                                    int enable)
{
    uint32 offs = (enable ? GWDIE0 : GWDID0) + (index / 32) * 0x10;
    uint32 tmp = 0;

    /* For VPF? */
    if (enable)
        tmp = reg_read32(gwca->base_addr + offs);

    reg_write32(BIT(index % 32) | tmp, gwca->base_addr + offs);
}

static void rswitch_enable_irqs(void)
{
    /* Enables IRQs on the core side */
    enable_int(GWCA1_RX_TX_IRQ);
    enable_int(GWCA1_RX_TS_IRQ);
    enable_int(GWCA1_ERR_IRQ);
    enable_int(ETHA0_ERR_IRQ);
    enable_int(COMA_ERR_IRQ);
}

int rswitch_init(void)
{
    int i, ret;

    // Enable clocks to all the agents
    for (i = 0; i < TOT_PORT_NUM; i++) {
        rswitch_agent_clock_ctrl(i, 1);
    }

    /* Read MAC addresses (since it's only reading it does not affect other
     * ports that we are not using) */
    for (i = 0; i < NUM_DEVICES; i++) {
        rswitch_etha_read_mac_address(&etha_props[i]);
    }

    rswitch_soft_reset();

    CHECK_RET(rswitch_gwca_hw_init(&rsw_dev));

    /* Copy speed property from GWCA */
    rsw_dev.etha->speed = rsw_dev.gwca->speed;

    rswitch_set_mac_address(rsw_dev.etha);

    rsw_dev.gwca->rx_chain.chain_index = 0;
    rsw_dev.gwca->rx_chain.ts_ring = rx_ring;
    for (i = 0; i < TX_RX_RING_SIZE; i++) {
        rsw_dev.gwca->rx_chain.data_buffers[i] = rx_data_buff[i];
    }
    rsw_dev.gwca->rx_chain.num_ring = TX_RX_RING_SIZE;
    rsw_dev.gwca->rx_chain.dir_tx = 0;
    rswitch_gwca_chain_ts_format(&rsw_dev, &(rsw_dev.gwca->rx_chain));
    rsw_dev.gwca->irq_queue = 0;

    rsw_dev.gwca->tx_chain.chain_index = 1;
    rsw_dev.gwca->tx_chain.ring = tx_ring;
    for (i = 0; i < TX_RX_RING_SIZE; i++) {
        rsw_dev.gwca->tx_chain.data_buffers[i] = tx_data_buff[i];
    }
    rsw_dev.gwca->tx_chain.num_ring = TX_RX_RING_SIZE;
    rsw_dev.gwca->tx_chain.dir_tx = 1;
    rswitch_gwca_chain_format(&rsw_dev, &(rsw_dev.gwca->tx_chain));
    rsw_dev.gwca->irq_queue = 1;

    CHECK_RET(rswitch_bpool_config(&rsw_dev));

    rswitch_fwd_init(&rsw_dev);

    rswitch_enable_irqs();

    return 0;
}

static int rswitch_etha_change_mode(struct rswitch_etha *etha,
                                    enum rswitch_etha_mode mode)
{
    int ret;

    /* Enable clock */
    if (!rswitch_agent_clock_is_enabled(etha->port_num))
        rswitch_agent_clock_ctrl(etha->port_num, 1);

    reg_write32(mode, etha->base_addr + EAMC);

    ret = rswitch_reg_wait(etha->base_addr + EAMS, EAMS_OPS_MASK, mode);

    /* Disable clock */
    if (mode == EAMC_OPC_DISABLE)
        rswitch_agent_clock_ctrl(etha->port_num, 0);

    return ret;
}

static void rswitch_rmac_setting(struct rswitch_etha *etha)
{
    uint32 val;

    switch (etha->speed) {
    case 10:
        val = MPIC_LSC_10M;
        break;
    case 100:
        val = MPIC_LSC_100M;
        break;
    case 1000:
        val = MPIC_LSC_1G;
        break;
    default:
        return;
    }

    reg_write32(MPIC_PIS_GMII | val, etha->base_addr + MPIC);
}

static void rswitch_etha_enable_mii(struct rswitch_etha *etha)
{
    rswitch_modify(etha->base_addr + MPIC, MPIC_PSMCS_MASK | MPIC_PSMHT_MASK,
                    MPIC_PSMCS(0x05) | MPIC_PSMHT(0x06));
    rswitch_modify(etha->base_addr + MPSM, 0, MPSM_MFF_C45);
}

static uint8 rswitch_etha_wait_link_verification(struct rswitch_etha *etha)
{
    /* Request Link Verification */
    reg_write32(MLVC_PLV, etha->base_addr + MLVC);
    return rswitch_reg_wait(etha->base_addr + MLVC, MLVC_PLV, 0);
}

static int rswitch_etha_hw_init(struct rswitch_etha *etha)
{
    int ret;

    /* Change to CONFIG Mode */
    CHECK_RET(rswitch_etha_change_mode(etha, EAMC_OPC_DISABLE));
    CHECK_RET(rswitch_etha_change_mode(etha, EAMC_OPC_CONFIG));

    reg_write32(EAVCC_VEM_SC_TAG, etha->base_addr + EAVCC);

    rswitch_rmac_setting(etha);
    rswitch_etha_enable_mii(etha);

    /* Change to OPERATION Mode */
    CHECK_RET(rswitch_etha_change_mode(etha, EAMC_OPC_OPERATION));

    /* Link Verification */
    return rswitch_etha_wait_link_verification(etha);
}

static int rswitch_phy_init(struct rswitch_etha *etha)
{
    uint32 reg_data;
    int ret;

    /* Reset */
    CHECK_RET(rswitch_mii_read(etha, etha->port_num, 1, 0xC04A, &reg_data));
    reg_data |= BIT(15);
    CHECK_RET(rswitch_mii_write(etha, etha->port_num, 1, 0xC04A, &reg_data));
    CHECK_RET(rswitch_mii_write(etha, etha->port_num, 1, 0xC04A, &reg_data)); /* MCAL does it twice... */

    /* Check mode */
    CHECK_RET(rswitch_mii_read(etha, etha->port_num, 1, 0xC04A, &reg_data));
    if ((reg_data & 0x7) != 0x4 ) { /* 4 stands for SGMII */
        reg_data &= ~0x7;
        reg_data |= BIT(15) | 0x4;
        CHECK_RET(rswitch_mii_write(etha, etha->port_num, 1, 0xC04A, &reg_data));

        /* Run SERDES Init */
        CHECK_RET(rswitch_mii_read(etha, etha->port_num, 1, 0x800F, &reg_data));
        reg_data |= BIT(15) | BIT(13);
        CHECK_RET(rswitch_mii_write(etha, etha->port_num, 1, 0x800F, &reg_data));
        
        reg_data = 0x0U;
        do {
            CHECK_RET(rswitch_mii_read(etha, etha->port_num, 1, 0x800F, &reg_data));
        } while (reg_data & BIT(15));

        /* Auto SERDES Init Disable */
        reg_data &= ~BIT(13);
        CHECK_RET(rswitch_mii_write(etha, etha->port_num, 1, 0x800F, &reg_data));
    }

    return 0;
}

int rswitch_open(void)
{
    int ret;

    CHECK_RET(rswitch_etha_hw_init(rsw_dev.etha));
    CHECK_RET(rswitch_phy_init(rsw_dev.etha));

    /* Enable RX */
    rswitch_modify(rsw_dev.gwca->base_addr + GWTRC0, 0,
                   BIT(rsw_dev.gwca->rx_chain.chain_index));

    /* Enable interrupt */
    rswitch_enadis_data_irq(rsw_dev.gwca, rsw_dev.gwca->tx_chain.chain_index,
                            TRUE);
    rswitch_enadis_data_irq(rsw_dev.gwca, rsw_dev.gwca->rx_chain.chain_index,
                            TRUE);

    return 0;
}