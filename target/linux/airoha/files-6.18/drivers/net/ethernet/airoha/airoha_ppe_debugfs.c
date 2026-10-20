// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025 AIROHA Inc
 * Author: Lorenzo Bianconi <lorenzo@kernel.org>
 */

#include "airoha_regs.h"
#include "airoha_eth.h"

static void airoha_debugfs_ppe_print_tuple(struct seq_file *m,
					   void *src_addr, void *dest_addr,
					   u16 *src_port, u16 *dest_port,
					   bool ipv6)
{
	__be32 n_addr[IPV6_ADDR_WORDS];

	if (ipv6) {
		ipv6_addr_cpu_to_be32(n_addr, src_addr);
		seq_printf(m, "%pI6", n_addr);
	} else {
		seq_printf(m, "%pI4h", src_addr);
	}
	if (src_port)
		seq_printf(m, ":%d", *src_port);

	seq_puts(m, "->");

	if (ipv6) {
		ipv6_addr_cpu_to_be32(n_addr, dest_addr);
		seq_printf(m, "%pI6", n_addr);
	} else {
		seq_printf(m, "%pI4h", dest_addr);
	}
	if (dest_port)
		seq_printf(m, ":%d", *dest_port);
}

static int airoha_ppe_debugfs_foe_show(struct seq_file *m, void *private,
				       bool bind)
{
	static const char *const ppe_type_str[] = {
		[PPE_PKT_TYPE_IPV4_HNAPT] = "IPv4 5T",
		[PPE_PKT_TYPE_IPV4_ROUTE] = "IPv4 3T",
		[PPE_PKT_TYPE_BRIDGE] = "L2B",
		[PPE_PKT_TYPE_IPV4_DSLITE] = "DS-LITE",
		[PPE_PKT_TYPE_IPV6_ROUTE_3T] = "IPv6 3T",
		[PPE_PKT_TYPE_IPV6_ROUTE_5T] = "IPv6 5T",
		[PPE_PKT_TYPE_IPV6_6RD] = "6RD",
	};
	static const char *const ppe_state_str[] = {
		[AIROHA_FOE_STATE_INVALID] = "INV",
		[AIROHA_FOE_STATE_UNBIND] = "UNB",
		[AIROHA_FOE_STATE_BIND] = "BND",
		[AIROHA_FOE_STATE_FIN] = "FIN",
	};
	struct airoha_ppe *ppe = m->private;
	u32 ppe_num_entries = airoha_ppe_get_total_num_entries(ppe);
	int i;

	for (i = 0; i < ppe_num_entries; i++) {
		const char *state_str, *type_str = "UNKNOWN";
		void *src_addr = NULL, *dest_addr = NULL;
		u16 *src_port = NULL, *dest_port = NULL;
		struct airoha_foe_mac_info_common native_l2 = {};
		struct airoha_foe_mac_info_common *l2;
		unsigned char h_source[ETH_ALEN] = {};
		struct airoha_foe_stats64 stats = {};
		unsigned char h_dest[ETH_ALEN];
		struct airoha_foe_entry *hwe;
		u32 type, state, ib2, data;
		bool ipv6 = false;

		hwe = airoha_ppe_foe_get_entry(ppe, i);
		if (!hwe)
			continue;

		state = FIELD_GET(AIROHA_FOE_IB1_BIND_STATE, hwe->ib1);
		if (!state)
			continue;

		if (bind && state != AIROHA_FOE_STATE_BIND)
			continue;

		state_str = ppe_state_str[state % ARRAY_SIZE(ppe_state_str)];
		type = FIELD_GET(AIROHA_FOE_IB1_BIND_PACKET_TYPE, hwe->ib1);
		if (type < ARRAY_SIZE(ppe_type_str) && ppe_type_str[type])
			type_str = ppe_type_str[type];

		seq_printf(m, "%05x %s %7s", i, state_str, type_str);

		switch (type) {
		case PPE_PKT_TYPE_IPV4_HNAPT:
		case PPE_PKT_TYPE_IPV4_DSLITE:
			src_port = &hwe->ipv4.orig_tuple.src_port;
			dest_port = &hwe->ipv4.orig_tuple.dest_port;
			fallthrough;
		case PPE_PKT_TYPE_IPV4_ROUTE:
			src_addr = &hwe->ipv4.orig_tuple.src_ip;
			dest_addr = &hwe->ipv4.orig_tuple.dest_ip;
			break;
		case PPE_PKT_TYPE_IPV6_ROUTE_5T:
			src_port = &hwe->ipv6.src_port;
			dest_port = &hwe->ipv6.dest_port;
			fallthrough;
		case PPE_PKT_TYPE_IPV6_ROUTE_3T:
		case PPE_PKT_TYPE_IPV6_6RD:
			src_addr = &hwe->ipv6.src_ip;
			dest_addr = &hwe->ipv6.dest_ip;
			ipv6 = true;
			break;
		default:
			break;
		}

		if (src_addr && dest_addr) {
			seq_puts(m, " orig=");
			airoha_debugfs_ppe_print_tuple(m, src_addr, dest_addr,
						       src_port, dest_port, ipv6);
		}

		switch (type) {
		case PPE_PKT_TYPE_IPV4_HNAPT:
		case PPE_PKT_TYPE_IPV4_DSLITE:
			src_port = &hwe->ipv4.new_tuple.src_port;
			dest_port = &hwe->ipv4.new_tuple.dest_port;
			src_addr = &hwe->ipv4.new_tuple.src_ip;
			dest_addr = &hwe->ipv4.new_tuple.dest_ip;
			seq_puts(m, " new=");
			airoha_debugfs_ppe_print_tuple(m, src_addr, dest_addr,
						       src_port, dest_port,
						       ipv6);
			break;
		default:
			break;
		}

		if (airoha_is_7581(ppe->eth) && type == PPE_PKT_TYPE_BRIDGE) {
			data = hwe->l2b.data;
			ib2 = hwe->l2b.ib2;
			native_l2.dest_mac_hi = hwe->l2b.dest_mac_hi;
			native_l2.dest_mac_lo = hwe->l2b.dest_mac_lo;
			native_l2.src_mac_hi = hwe->l2b.src_mac_hi;
			native_l2.vlan1 = hwe->l2b.vlan1;
			native_l2.vlan2 = hwe->l2b.vlan2;
			native_l2.etype = hwe->l2b.etype;
			l2 = &native_l2;
			*((__be16 *)&h_source[4]) = cpu_to_be16(hwe->l2b.src_mac_lo);
		} else if (type >= PPE_PKT_TYPE_IPV6_ROUTE_3T) {
			data = hwe->ipv6.data;
			ib2 = hwe->ipv6.ib2;
			l2 = &hwe->ipv6.l2;
		} else {
			data = hwe->ipv4.data;
			ib2 = hwe->ipv4.ib2;
			l2 = &hwe->ipv4.l2.common;
			*((__be16 *)&h_source[4]) =
				cpu_to_be16(hwe->ipv4.l2.src_mac_lo);
		}

		airoha_ppe_foe_entry_get_stats(ppe, i, &stats);

		*((__be32 *)h_dest) = cpu_to_be32(l2->dest_mac_hi);
		*((__be16 *)&h_dest[4]) = cpu_to_be16(l2->dest_mac_lo);
		*((__be32 *)h_source) = cpu_to_be32(l2->src_mac_hi);

		seq_printf(m, " eth=%pM->%pM etype=%04x data=%08x"
			      " vlan=%d,%d ib1=%08x ib2=%08x"
			      " packets=%llu bytes=%llu\n",
			   h_source, h_dest, l2->etype, data,
			   l2->vlan1, l2->vlan2, hwe->ib1, ib2,
			   stats.packets, stats.bytes);
	}

	return 0;
}

static int airoha_ppe_debugfs_foe_all_show(struct seq_file *m, void *private)
{
	return airoha_ppe_debugfs_foe_show(m, private, false);
}
DEFINE_SHOW_ATTRIBUTE(airoha_ppe_debugfs_foe_all);

static int airoha_ppe_debugfs_foe_bind_show(struct seq_file *m, void *private)
{
	return airoha_ppe_debugfs_foe_show(m, private, true);
}
DEFINE_SHOW_ATTRIBUTE(airoha_ppe_debugfs_foe_bind);

static int airoha_ppe_debugfs_config_show(struct seq_file *m, void *private)
{
	struct airoha_ppe *ppe = m->private;
	struct airoha_eth *eth = ppe->eth;
	unsigned int i;

	/* These registers select the CPU/QDMA path for GDM2 PPE misses.
	 * PPPoE Discovery reaches this path before a FOE exists.
	 */
	seq_printf(m, "npu_attached: %u\n",
		   !!rcu_access_pointer(eth->npu));
	seq_printf(m, "gdm2_fwd_cfg: %08x\n",
		   airoha_fe_rr(eth, REG_GDM_FWD_CFG(AIROHA_GDM2_IDX)));
	seq_printf(m, "fe_wan_port: %08x\n",
		   airoha_fe_rr(eth, REG_FE_WAN_PORT));
	seq_printf(m, "fe_vip_port_en: %08x\n",
		   airoha_fe_rr(eth, REG_FE_VIP_PORT_EN));
	seq_printf(m, "fe_ifc_port_en: %08x\n",
		   airoha_fe_rr(eth, REG_FE_IFC_PORT_EN));

	for (i = 0; i < eth->soc->num_ppe; i++) {
		u32 cport = airoha_fe_rr(
			eth, REG_PPE_DFT_CPORT(i, AIROHA_GDM2_IDX));

		seq_printf(m, "ppe%u_flow_cfg: %08x\n", i,
			   airoha_fe_rr(eth, REG_PPE_PPE_FLOW_CFG(i)));
		seq_printf(m, "ppe%u_table_cfg: %08x\n", i,
			   airoha_fe_rr(eth, REG_PPE_TB_CFG(i)));
		seq_printf(m, "ppe%u_gdm2_default_cpu_port: %u\n", i,
			   FIELD_GET(DFT_CPORT_MASK(AIROHA_GDM2_IDX), cport));
	}

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(airoha_ppe_debugfs_config);

int airoha_ppe_debugfs_init(struct airoha_ppe *ppe)
{
	ppe->debugfs_dir = debugfs_create_dir("ppe", NULL);
	debugfs_create_file("config", 0444, ppe->debugfs_dir, ppe,
			    &airoha_ppe_debugfs_config_fops);
	debugfs_create_file("entries", 0444, ppe->debugfs_dir, ppe,
			    &airoha_ppe_debugfs_foe_all_fops);
	debugfs_create_file("bind", 0444, ppe->debugfs_dir, ppe,
			    &airoha_ppe_debugfs_foe_bind_fops);

	return 0;
}
