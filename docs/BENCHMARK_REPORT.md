# Gemtek XR1710G 固件实测与横向对比报告（2026-11-22）

[中文报告](#中文报告) | [English Summary](#english-summary)

---

<a id="中文报告"></a>
## 中文报告

### 1. 测试环境与测试方法

- **AP 设备**：Gemtek XR1710G（Airoha AN7581 四核 A53 + MT7996 三频 Wi-Fi 7 + 双 10GbE RTL8261BE），Pure-AP 二层桥接模式（IP `192.168.3.4/24`，上游接 10GbE 交换机与主路由 `192.168.3.1`，关闭 `dnsmasq` / `odhcpd`）。
- **无线客户端**：一加 13（`PJZ110`，Wi-Fi 7 2x2 MIMO，`320 MHz`），连接 AP 6 GHz 频段（SSID `FuckHuawei-6G`，Channel 37 / `6135 MHz`，WPA3-SAE）。
- **有线对端服务器**：`fnOS`（`192.168.3.2`，10GbE 链路），内核已持久化优化为 **`BBR + fq + 16 MB socket/TCP buffer`**：
  ```ini
  # /etc/modules-load.d/99-wifi7-bbr.conf: tcp_bbr, sch_fq
  # /etc/sysctl.d/99-zz-wifi7-bbr.conf
  net.core.default_qdisc = fq
  net.ipv4.tcp_congestion_control = bbr
  net.core.wmem_max = 16777216
  net.core.rmem_max = 16777216
  net.ipv4.tcp_wmem = 4096 131072 16777216
  ```
- **测速工具与指标**：手机端 `iperf3` 对 `192.168.3.2:5298` 运行 10 秒 TCP 测试（`P1` 单流 / `P4` 四流 / `P16` 十六流），同步采集 AP 端 `iw station dump`、`mt76/tx_stats`、`/proc/stat` 单核利用率及 `/sys/kernel/debug/ppe` 硬件绑定流表数量（`ppe_bnd`）。

---

### 2. 社区固件横向对比结果（一加 13 实测）

| 固件版本 | 内核 | `bridger` 二层卸载 | 6 GHz 功率 / 信号 | `ppe_bnd` 硬件流 | AP CPU 最忙核 / `cpu0` | 下行 `P1` / `P4` / `P16` (Mbps) | 上行 `P1` / `P4` / `P16` (Mbps) |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Pure-AP Baseline (`Phase B v2`)**<br>`20261122-pure-ap-phaseB` | `6.18.60` | **开启**<br>(`cls_bpf`/`cls_flower`/`act_vlan`) | `30.00 dBm` / `-42 dBm` | **`4 ~ 34`** | **`0.46% ~ 1.62%`**<br>(最忙核 `12.5%`) | `1793.15` / `2364.49~2452.98` / **`2632.88~2713.32`** | `1594.17` / `1808.16~1847.91` / **`1890.13~1913.75`** |
| **YYH (`xr1710g_260831`)**<br>OpenWrt `r2363-e88fbe28ea` | `6.18.44` | **缺失**<br>(无 `bridger` 及 `tc` 卸载模块) | `30.00 dBm` | `0` | **`86.7% ~ 94.0%`**<br>(`cpu0` 软中断打满) | `41.73` / `1399.61` / `1456.67` | `792.94` / `775.13` / `793.40` |
| **`xmiqz` LEDE (`R26.11.6`)**<br>`xr1710g-lede-default-2026-11-r24` | `6.18.60` | **缺失**<br>(无 `bridger` / `cls_flower` / `act_vlan`) | 被 `US` 规则锁死 **`12.00 dBm`** / `-58 dBm` | `0` | **`74.4% ~ 94.2%`**<br>(`cpu0` / `cpu2` 打满) | `43.72` / `1066.38` / `924.22`<br>(二次复测 `1144.49` / `1145.03`) | `792.35` / `808.47` / `798.63` |

#### 性能差距根因解析

1. **为什么 Pure-AP 基线版能跑满 `2.36 ~ 2.71 Gbps` 且 AP CPU 仅 `~1%`？**
   - 当 XR1710G 作为**二层纯 AP（`br-lan` 网桥）**使用时，流量不经过三层 NAT 路由表，普通 `nftables flow_offloading` 无法接管二层网桥帧。
   - 本固件完整保留并拉起 **`bridger` eBPF 桥接加速器** + 内核 `cls_bpf` / `cls_flower` / `act_vlan` 模块，把无线 ↔ 有线网桥流直接下发到 Airoha **PPE 硬件转发引擎**（`ppe_bnd = 4 ~ 34`），`cpu0` 占用率维持在 **`0.46% ~ 1.62%`**。
2. **为什么 YYH (`260831`) 只有 `1.40 ~ 1.50 Gbps`？**
   - `YYH 260831` 构建中未包含 `bridger` 以及 `cls_bpf.ko` / `cls_flower.ko` / `act_vlan.ko`，纯 AP 桥接流量 `ppe_bnd = 0`，全部依赖软件 CPU 转发，`cpu0` 直接顶到 **`86.7% ~ 91.0%`**（24 dBm 下 `93.5% ~ 94.0%`），吞吐卡死在 ~1.4–1.5 Gbps、上行 ~0.78 Gbps。
3. **为什么 `xmiqz` LEDE (`R26.11.6`) 只有 `1.07 ~ 1.15 Gbps`？**
   - 同样缺少 `bridger` 与 `cls_flower` / `act_vlan`（`ppe_bnd = 0`，`cpu0`/`cpu2` 达 `74.4% ~ 94.2%`）。
   - 同时其 `US` 监管域规则将 6 GHz `320 MHz` 发射功率限制在 **`12.00 dBm`**（`iw dev ... set txpower fixed 3000` 无法覆盖），导致近场手机 RSSI 仅 `-56 ~ -59 dBm`（基线版为 `-42 dBm`），进一步拉低高阶调制能力。

---

### 3. 发射功率梯度测试（Pure-AP Phase B v2）

在 OnePlus 13 近场工况下（`fnOS` 已开启 `BBR + fq + 16 MB`），对比 AP 6 GHz 不同 `txpower` 设置：

| 功率档位 | `dl_p1` (Mbps) | `dl_p4` (Mbps) | `dl_p16` (Mbps) | `ul_p4` (Mbps) | 主导 MCS / 备注 |
| :---: | ---: | ---: | ---: | ---: | :--- |
| **`30 dBm`（默认 / 穿墙）** | `1793.15` | `2452.98` (回滚复测 `2364.49`) | **`2632.88`** (回滚复测 **`2713.32`**) | **`1808.16`** (回滚复测 **`1847.91`**) | `MCS8/9`，`tx_retry = 1.19%~6.20%` |
| **`24 dBm`（近场平衡）** | `1830.88` | **`2569.73`** | `2588.40` | `1754.92` | `MCS8/9`，`dl_p4 retry` 降至 `2.67%` |
| **`22 dBm`（极近场低失真）** | **`1913.97`** | `2520.61` | `2630.45` | — | 单流 `dl_p1` 表现最佳，`MCS8/9` |

> 说明：穿墙及通用家用建议保持默认 `30 dBm`；若 AP 与终端仅隔 1–2 米且追求单流/4 流极致稳态，可将 6 GHz 功率微调至 `22~24 dBm` 降低功放互调失真。

---

### 4. Phase C 实验补丁 (`9998`) A/B 验证结论（为何不合入基线）

我们在开发分支测试过 `Phase C`（补丁 `9998-wifi-mt76-mt7996-eht320-bfer-and-runtime-tuning.patch`，引入 `eht320_mmps_disable`、`tx_amsdu_max_num`、`muru_ofdma_ul_en` 调试开关）：

| 实验分组 | 下行 `P1` (Mbps) | 下行 `P4` (Mbps) | 下行 `P16` (Mbps) | 上行 `P4` (Mbps) | `eBF / fb_eht` | 关键现象与判定 |
| :--- | ---: | ---: | ---: | ---: | :---: | :--- |
| **`C0` (`Phase C` 默认)** | `1931.89` (`24dBm: 2074.90`) | `1946.05` (`24dBm: 2220.11`) | `2209.04` (`24dBm: 2267.49`) | `912.74` | `0 / 0` | 受限在 `MCS7`，整体慢于 `Phase B v2`（`2.45–2.71 Gbps`, `MCS8/9`） |
| **`C1` (`eht320_mmps_disable=1`)** | `1089.35` | `1216.74` | `1226.81` | `1049.89` | **`2434 / 27`** | 证实 OnePlus 13 通告 `HE-DYNAMIC-SM-PS` 是 `eBF=0` 诱因；强行忽略后虽开启显式波束成形，但探测开销导致 `MCS5` 暴涨至 `22~31%`，速度腰斩（**负优化**） |
| **`C2` (`tx_amsdu_max_num=4`)** | `1641.59` | `2213.61` (`24dBm: 2300.01`) | `2300.33` | — | `0 / 0` | 将重传压至 `0.70%~0.72%`，但在 `24 dBm` 下与 `ofdma_ul=1` 叠加触发 **`C4` 降速（`886~1184 Mbps`）** |
| **`C3` (`muru_ofdma_ul_en=1`)** | — | `2277.88` | — | `1827.55` (`ul_p16: 1893.26`) | `0 / 0` | 上行恢复，下行仍锁定在 `MCS7` |

**最终决策**：保留干净、高速、低重传的 **`Phase B v2`（`20261122-pure-ap-phaseB`）** 作为 GitHub 正式基线，不合并引起 `MCS7` 降档与 `C1`/`C4` 回退的 `9998` 实验补丁。

---

### 5. SPI-NAND 分区布局与跨固件刷写兼容性

通过解析各版本 `.itb` 内置 FDT（`/images/fdt-1`），确认 **naoki66 / 本 Pure-AP 基线 (`gemtek,xr1710g`) / YYH (`gemtek,xr1710g-ubi`) / `xmiqz` LEDE** 均使用**完全一致的 Airoha SPI-NAND UBI 2.0 分区布局**：

| 分区名称 (`label`) | 起始地址 (`offset`) | 大小 (`size`) | 说明 |
| :--- | :---: | :---: | :--- |
| `vendor` | `0x00000000` | `0x00600000` (6 MiB) | 厂商引导区 |
| `chainloader` | `0x00600000` | `0x00100000` (1 MiB) | U-Boot Chainloader |
| **`ubi`** | **`0x00700000`** | **`0x1b700000` (439 MiB)** | 主 UBI 容器（含 `fit`、`ubootenv`、`ubootenv2`、`factory`、`rootfs_data`） |
| `reserved_bmt` | `0x1be00000` | `0x04200000` (66 MiB) | 坏块管理保留区 |

- **同分支（`gemtek,xr1710g`）升级**：直接使用 LuCI 页面或 `sysupgrade -v <itb>`。
- **从其它分支（如 `gemtek,xr1710g-ubi` / LEDE）切换回本固件**：因 `board_name` 字符串与 `compat_version` 差异，`sysupgrade` 需加 `-F`（不建议保留原路由器分支的防火墙/DHCP 全量配置，建议仅保留 `network` / `wireless` / `system` / `dropbear` / `shadow` 或使用干净配置升级 `-n`）：
  ```sh
  sysupgrade -F -v -n /tmp/immortalwrt-nijigencg-20261122-pure-ap-phaseb-airoha-an7581-gemtek_xr1710g-squashfs-sysupgrade.itb
  ```
- **Pure-AP 默认网络说明**：本固件完全移除 `dnsmasq` 与 `odhcpd`，不会在局域网内下发 DHCP；刷写并恢复纯 AP 配置时请确保 `br-lan` 设有静态管理 IP（例如 `192.168.3.4/24`）及正确的上级网关。

---

<a id="english-summary"></a>
## English Summary

### Testbed & Methodology

- **AP**: Gemtek XR1710G (Airoha AN7581 + MediaTek MT7996 tri-band Wi-Fi 7 + dual 10GbE), configured as a pure L2 bridge AP (`br-lan`, no DHCP server).
- **Client**: OnePlus 13 (`PJZ110`, 2x2 Wi-Fi 7 `320 MHz`, 6 GHz Channel 37 / `6135 MHz`, WPA3-SAE).
- **Wired Peer**: `fnOS` (`192.168.3.2`, 10GbE) tuned with `BBR + fq + 16 MB wmem/rmem/tcp_wmem`.

### Head-to-Head Firmware Comparison

| Firmware | Kernel | `bridger` L2 Offload | 6 GHz TX Power / RSSI | `ppe_bnd` Entries | AP CPU (`cpu0` / Peak Core) | DL `P1` / `P4` / `P16` (Mbps) | UL `P1` / `P4` / `P16` (Mbps) |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Pure-AP Baseline (`Phase B v2`)** | `6.18.60` | **Enabled** (`cls_bpf`/`cls_flower`/`act_vlan`) | `30 dBm` / `-42 dBm` | **`4 ~ 34`** | **`0.46% ~ 1.62%`** (max `12.5%`) | `1793` / `2364~2453` / **`2633~2713`** | `1594` / `1808~1848` / **`1890~1914`** |
| **YYH (`xr1710g_260831`)** | `6.18.44` | Missing | `30 dBm` | `0` | **`86.7% ~ 94.0%`** | `42` / `1400` / `1457` | `793` / `775` / `793` |
| **`xmiqz` LEDE (`R26.11.6`)** | `6.18.60` | Missing (`cls_flower`/`act_vlan` absent) | Clamped to **`12 dBm`** (`US`) / `-58 dBm` | `0` | **`74.4% ~ 94.2%`** | `44` / `1066` / `924` (`1145` 2nd run) | `792` / `808` / `799` |

### Key Takeaways

1. **`bridger` + Airoha PPE is mandatory for Pure-AP Wi-Fi 7 throughput**: Without `bridger` and its `tc` modules (`cls_bpf`, `cls_flower`, `act_vlan`), L2 wireless-to-LAN bridge frames miss hardware flow offload (`ppe_bnd = 0`), saturating AN7581 `cpu0` at 87–94% and capping speeds at ~1.1–1.5 Gbps DL / ~0.8 Gbps UL.
2. **Pure-AP `Phase B v2` hits up to `2.71 Gbps` DL / `1.91 Gbps` UL** with only `~1%` AP CPU usage, and passes 5-gate build verification (FIT integrity, strict POSIX permissions on `ext4`, driver checks, QEMU cold-boot smoke test, and `sysupgrade -T`).
3. **Experimental `Phase C` (`9998`) was intentionally dropped**: Disabling `HE-DYNAMIC-SM-PS` tracking (`eht320_mmps_disable=1`) activates `eBF` (`1979–2434`), but incurs sounding/rate-drop overhead (`MCS5` 22–31%), halving throughput to `1089–1227 Mbps`.
4. **Identical SPI-NAND UBI 2.0 layout** (`ubi` at `0x00700000`, size `0x1b700000`, 439 MiB) across `naoki66`, this Pure-AP baseline, `YYH`, and `xmiqz` LEDE; cross-branch flashing uses `sysupgrade -F -v -n`.
