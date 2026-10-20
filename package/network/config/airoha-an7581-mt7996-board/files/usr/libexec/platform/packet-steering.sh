#!/bin/sh

. /lib/functions.sh
. /lib/functions/system.sh

mode="$1"

steering_flows="$(uci -q get 'network.@globals[0].steering_flows')"
opts=""
case "$steering_flows" in
	''|*[!0-9]*) ;;
	*) [ "$steering_flows" -gt 0 ] && opts="-l $steering_flows" ;;
esac

/usr/libexec/network/packet-steering.uc $opts "$mode"

case "$(board_name)" in
gemtek,xr1710g|gemtek,xr1710g-ubi) ;;
*) exit 0 ;;
esac

cpu_count=0
for cpu_path in /sys/devices/system/cpu/cpu[0-9]*; do
	[ -d "$cpu_path" ] || continue
	[ "$(cat "$cpu_path/online" 2>/dev/null)" = "0" ] && continue
	cpu_count=$((cpu_count + 1))
done

# Release only this policy's RX worker when packet steering is disabled.
if [ "$mode" = "0" ]; then
	for task in /proc/[0-9]*/sched; do
		read -r qdma0_name qdma0_details 2>/dev/null < "$task" || continue
		[ "$qdma0_name" = 'napi/qd0r0' ] || continue
		tid=${task%/sched}
		tid=${tid##*/}
		taskset -pc "$(cat /sys/devices/system/cpu/online)" "$tid" >/dev/null 2>&1
	done
	exit 0
fi

# Keep CPU0 available for the GIC and Ethernet hard IRQs. Spread the MT7996
# threaded-NAPI workers instead of letting netifd group them onto one CPU.
[ "$cpu_count" -gt 1 ] || exit 0

next_cpu=1
tx_cpu=1
[ "$cpu_count" -gt 2 ] && tx_cpu=2

for comm in /proc/[0-9]*/task/[0-9]*/comm; do
	[ -r "$comm" ] || continue
	read -r name < "$comm"
	task="${comm%/comm}"
	tid="${task##*/}"
	case "$name" in
		napi/phy*)
			taskset -pc "$next_cpu" "$tid" >/dev/null 2>&1
			next_cpu=$((next_cpu + 1))
			[ "$next_cpu" -lt "$cpu_count" ] || next_cpu=1
			;;
		mt76-tx\ phy*)
			taskset -pc "$tx_cpu" "$tid" >/dev/null 2>&1
			;;
	esac
done

# APDRV1_QDMA_CPU_STEERING: the shared QDMA NAPI device is not matched by
# generic netdev steering. IRQ numbers are discovered from handler names.
# These CPU masks do not change any link, bridge, address or route.
qdma_cpu=0
for irq in $(awk '$NF ~ /^airoha_eth[.][0-7]$/ {gsub(":", "", $1); print $1}' /proc/interrupts); do
    affinity=/proc/irq/$irq/smp_affinity
    [ -w "$affinity" ] || continue
    printf '%x' "$((1 << (qdma_cpu % cpu_count)))" > "$affinity"
    qdma_cpu=$((qdma_cpu + 1))
done
# wan is this AP's verified active 10GbE uplink. Keep the other ports and
# MT7996 steering policy managed by the existing platform implementation.
for queue in /sys/class/net/wan/queues/rx-*/rps_cpus; do
    [ -w "$queue" ] || continue
    printf '%x' "$(((1 << cpu_count) - 1))" > "$queue"
done

# Linux6.18 proc/comm reports the original kthread full name; proc/sched
# exposes the task comm set by the RX driver. Match that exact queue identity.
# AP QDMA0 queue0 RX shares a dummy netdev and is missed by generic steering.
# Keep its polling work away from CPU0 when LAN2 RPS targets CPU0 exclusively.
qdma0_rps_cpu0=1
qdma0_rps_queues=0
for queue in /sys/class/net/lan2/queues/rx-*/rps_cpus; do
	[ -f "$queue" ] || continue
	qdma0_rps_queues=$((qdma0_rps_queues + 1))
	[ "$(cat "$queue")" = 1 ] || qdma0_rps_cpu0=0
done
qdma0_rx_mask=$(((1 << cpu_count) - 1))
if [ "$cpu_count" -gt 1 ] && [ "$qdma0_rps_queues" -gt 0 ] && [ "$qdma0_rps_cpu0" = 1 ]; then
	qdma0_rx_mask=$(((1 << cpu_count) - 2))
fi
for task in /proc/[0-9]*/sched; do
	read -r qdma0_name qdma0_details 2>/dev/null < "$task" || continue
	[ "$qdma0_name" = 'napi/qd0r0' ] || continue
	tid=${task%/sched}
	tid=${tid##*/}
	qdma0_rx_mask_hex=$(printf '%x' "$qdma0_rx_mask")
	taskset -p "$qdma0_rx_mask_hex" "$tid" >/dev/null 2>&1
done
