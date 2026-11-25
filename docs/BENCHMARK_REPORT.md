# Gemtek XR1710G 固件实测与横向对比报告（2026-11-24 Phase G 更新）

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
  net.core.tcp_congestion_control = bbr
  net.core.wmem_max = 16777216
  net.core.rmem_max = 16777216
  net.core.tcp_wmem = 4096 131072 16777216
  ```
- **测速工具与指标**：手机端 `iperf3` 对 `192.168.3.2:5298` 运行 10 秒 TCP 测试（`P1` 单流 / `P4` 四流 / `P8` 八流 / `P16` 十六流），同步采集 AP 端 `iw station dump`、`mt76/tx_stats`、`/proc/stat` 单核利用率及 `/sys/kernel/debug/ppe` 硬件绑定流表数量（`ppe_bnd`）。

---

### 2. 社区固件横向对比结果（一加 13 实测）

| 固件版本 | 内核 | `bridger` 二层卸载 | 6 GHz 功率 / 关键调优 | `ppe_bnd` 硬件流 | AP CPU 最忙核 / `cpu0` | 下行 `P1` / `P4` / `P16` (Mbps) | 上行 `P1` / `P4` / `P16` (Mbps) |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Pure-AP (`Phase G`)**<br>`20261123-pure-ap-phaseG` | `6.18.60` | **开启**<br>(`cls_bpf`/`cls_flower`/`act_vlan`) | `30.00 dBm`<br>`npu_tx_band_map=2` + `radio2 eBF=0` | **`4 ~ 34`** | **`~1%`**<br>(全硬件卸载) | `1889.51` / **`2780.21 ~ 3049.98`** / **`2819.35 ~ 3049.98`** | — / **`2245.39`** (`MCS 10 = 61.6%`) / — |
| **Pure-AP Baseline (`Phase B v2`)**<br>`20261122-pure-ap-phaseB` | `6.18.60` | **开启** | `30.00 dBm` / `-42 dBm` | `4 ~ 34` | `0.46% ~ 1.62%`<br>(最忙核 `12.5%`) | `1793.15` / `2364.49~2452.98` / `2632.88~2713.32` | `1594.17` / `1808.16~1847.91` / `1890.13~1913.75` |
| **YYH (`xr1710g_260831`)**<br>OpenWrt `r2363-e88fbe28ea` | `6.18.44` | **缺失**<br>(无 `bridger` 及 `tc` 卸载模块) | `30.00 dBm` | `0` | **`86.7% ~ 94.0%`**<br>(`cpu0` 软中断打满) | `41.73` / `1399.61` / `1456.67` | `792.94` / `775.13` / `793.40` |
| **`xmiqz` LEDE (`R26.11.6`)**<br>`xr1710g-lede-default-2026-11-r24` | `6.18.60` | **缺失**<br>(无 `bridger` / `cls_flower` / `act_vlan`) | 被 `US` 规则锁死 **`12.00 dBm`** / `-58 dBm` | `0` | **`74.4% ~ 94.2%`**<br>(`cpu0` / `cpu2` 打满) | `43.72` / `1066.38` / `924.22`<br>(二次复测 `1144.49` / `1145.03`) | `792.35` / `808.47` / `798.63` |

#### 性能差距根因解析

1. **为什么 Pure-AP 基线版能跑满 `2.71 ~ 3.05 Gbps` 且 AP CPU 仅 `~1%`？**
   - 当 XR1710G 作为**二层纯 AP（`br-lan` 网桥）**使用时，流量不经过三层 NAT 路由表，普通 `nftables flow_offloading` 无法接管二层网桥帧。
   - 本固件完整保留并拉起 **`bridger` eBPF 桥接加速器** + 内核 `cls_bpf` / `cls_flower` / `act_vlan` 模块，把无线 ↔ 有线网桥流直接下发到 Airoha **PPE 硬件转发引擎**（`ppe_bnd = 4 ~ 34`），`cpu0` 占用率维持在 **`0.46% ~ 1.62%`**。
2. **为什么 YYH (`260831`) 只有 `1.40 ~ 1.50 Gbps`？**
   - `YYH 260831` 构建中未包含 `bridger` 以及 `cls_bpf.ko` / `cls_flower.ko` / `act_vlan.ko`，纯 AP 桥接流量 `ppe_bnd = 0`，全部依赖软件 CPU 转发，`cpu0` 直接顶到 **`86.7% ~ 91.0%`**（24 dBm 下 `93.5% ~ 94.0%`），吞吐卡死在 ~1.4–1.5 Gbps、上行 ~0.78 Gbps。
3. **为什么 `xmiqz` LEDE (`R26.11.6`) 只有 `1.07 ~ 1.15 Gbps`？**
   - 同样缺少 `bridger` 与 `cls_flower` / `act_vlan`（`ppe_bnd = 0`，`cpu0`/`cpu2` 达 `74.4% ~ 94.2%`）。
   - 同时其 `US` 监管域规则将 6 GHz `320 MHz` 发射功率限制在 **`12.00 dBm`**（`iw dev ... set txpower fixed 3000` 无法覆盖），导致近场手机 RSSI 仅 `-56 ~ -59 dBm`（基线版为 `-42 dBm`），进一步拉低高阶调制能力。

---

### 3. Phase G（`9998` + `9999`）专项优化与 A/B 实测（`3,049.98 Mbps` DL / `2,245.39 Mbps` UL）

#### 3.1 `9998`：运行时 `npu_tx_band_map` 将 5G/6G 全切至 PCIe0 Gen3 x2 (`15.752 Gb/s`)
- 默认状态下 MT7996 把 6 GHz 放在 PCIe2（Gen2 x1，理论上限 `4.000 Gb/s`），而 PCIe0（Gen3 x2，`15.752 Gb/s`）富余极大。
- 补丁 `9998-wifi-mt76-mt7996-npu-6g-pcie0-wdma0.patch` 提供 `0644` 运行时模块参数：
  - `npu_tx_band_map=0`：原厂默认（保留开机安全回退）
  - `npu_tx_band_map=1`：6G → PCIe0 (WDMA0)，2.4G + 5G → PCIe2 (WDMA1)
  - **`npu_tx_band_map=2`（推荐持久化）**：**5G + 6G 上下行全走 PCIe0 Gen3 x2 (`15.752 Gb/s`)**，仅 2.4G 走 PCIe2 Gen2 x1 (`4.000 Gb/s`)，经 `devmem` 实测 `P0_TX18` 环实时搬运、`P2_TX21` 静默。

#### 3.2 `9999` + 6 GHz `eBF` 锁死 `MCS 7` 根因定位与方案 A 验证
- 补丁 `9999-wifi-mt76-mt7996-fix-eht320-bf-he-bw320-and-rc-sync.patch` 完整修复了 `mt7996_mcu_sta_bfer_eht()` 的 BW320→BW160 `fallthrough` 维度写入、`ncol <= nrow` 钳位与 `ng16`/`codebook` 解析，并修复了 `mt7996_mcu_sta_he_tlv()` 遗漏 `BW320` 与 `add_rate_ctrl` 未同步 `STA_REC_BF` 的缺陷，实机 `dmesg` 确认下发 `gt80=0x33/0x11`。
- 但在一加 13（`PHY-CAP[3]=0x00`: `trig=0/0, ng16=0/0, cb=0/0`）近距实测中发现 **MT7996 固件 6 GHz eBF 异常**：只要 `EHT` 反馈首次出现（`All: 1, EHT: 1`）并激活 `eBF`，反馈计数便卡死在 `1` 不再刷新，硬件却持续对后续所有 PPDU 套用陈旧波束矩阵，把 6 GHz 下行从 **`MCS 9/10` (`2.82–3.05 Gbps`)** 压死在 **`MCS 7 (96.6–98.5%) / 2.26–2.33 Gbps`**。

| 测试阶段（Phase G，一加 13 6 GHz 320 MHz） | 配置要点 | `eBF / EHT` 状态 | 下行实测 (Mbps) | 上行 `ul_p4` (Mbps) | 主导 MCS 分布 |
| :--- | :--- | :--- | ---: | ---: | :--- |
| **R1 默认** (`-33 dBm`) | `map=0, sku=1, bfbo=1, 30 dBm` | 末尾首次触发 `EHT: 1` | **`2819.35`** (`P16`) | `1699.78` | `MCS 9` (52.9–59.7%) |
| **R2–R4（eBF 触发后）** | `eht_su_beamformer=1` | `eBF` 激增至 `19,908`，`EHT` 卡死 `1` | `2166 ~ 2331`（锁死） | `2095.28` | **`MCS 7` (82.7–98.5%)** |
| **方案 A（关 6G eBF，近距）** | `radio2 eht_su_beamformer=0`，`map=2`，`30 dBm` | **`eBF = 0, EHT = 0`** | **`3049.98`** (`P4`/`P8`) | **`2245.39`** | **DL: `MCS 10` 58.6% + `MCS 9` 26.6% + `MCS 11` 3.8%**<br>**UL: `MCS 10` 61.6% + `MCS 11` 3.9%** |

> **关键配置提示**：在 OpenWrt `mac80211` 架构下，`eht_su_beamformer=0` / `he_su_beamformer=0` **必须写在设备节 `wireless.radio2`**（仅写在接口节 `default_radio2` 不会传入 `hostapd-phy0.2.conf`）。

---

---

### 3.5 Phase I (P0) & Phase J (P0+P1) 稳定性与运维增强回归验证（2026-11-25）

在完整保留 Phase G（`npu_tx_band_map=2` + 6 GHz EHT320 硬件卸载引擎）的基础上，分两阶段采纳并实测验证了 OpenWrt 主线、`openwrt/mt76` 11 月上游及社区（`yahuisme` / `M1s4k1`）精选修复：

- **Phase I（P0：`mt76` / NPU / RRO 核心稳定补丁包）**：
  - 解包启用原双层包装未生效的 `128/129/132/134`（Full-Reset/L1 SER NPU 重启与重试、注册前保护、合并 `461aea7afc` 默认超时），移除未停 NPU 强清 Bus Master 的死补丁 `133`；
  - 原位加固 `126-v2`（保留 refill-gated `dma_idx` + `dma_wmb()` + `READ_ONCE()` + `MAX_SKB_FRAGS`/OOM 安全回收）；
  - 按拓扑序合入 24 个 `openwrt/mt76` 11 月核心补丁（含 `028951c4bf` HW-RRO `qos_ctl`、`3c17c4d96e` GCMP-256/BIGTK PN 校验、`742a17ccb2`/`4bbb6457ff`/`e3cc87292f` TWT/PS 保护非 MLD 终端、`59f00daa97`/`a4bc3dd95b` 消除 WTBL 竞态等）；
  - 合入 `yahuisme` `913`（NPU 多描述符 TXFREE 线性化 + OOM `skb_headlen` 兜底）与 `914`（RRO 丢包/OOM 页面归还）。
- **Phase J（P0+P1：零报错开机 + 10GbE 诊断 + single-wiphy 修复）**：
  - `procd/001`：协调 AN7581 28s 硬件看门狗，开机 `dmesg` WDT 错误彻底归零；
  - `iwinfo 998/999`：修复 single-wiphy 分频段独立扫描与按频率精确匹配的功率列表（`28 / 30 / 30 dBm`）；
  - OpenWrt 主线 `844a34e63f`：启用 `arm,cortex-a53-pmu` 节点；
  - `747-02` + `9999-40`：RTL8261BE `ethtool` 实报真实协商速率 + `ethtool -S` 29 项 GDM 硬件计数器；
  - 清理双层死补丁 `929-03`（保持稳定运行的 PCIe Gen3 IDAC 校准参数不变）。

| 验证阶段 | `dmesg` 错误/告警 | 客户端在线 | 6 GHz EHT320 实测（中距 `RSSI -39 dBm`） | 专项验收 |
| :--- | :---: | :---: | :--- | :--- |
| **Phase I (`P0`)** | 仅旧版 WDT 1 条，零内核/mt76 异常 | 2.4G: 35 / 5G: 2 / 6G: 1 | DL P16: `2,275.13 Mbps`（retry `1.95%`）<br>UL P4: `1,322.11 Mbps`（retry `0.34%`） | 三频功率 `28/30/30 dBm`，零 NPU/RRO/txfree 异常 |
| **Phase J (`P0+P1`)** | **`NONE`（100% 零报错）** | 2.4G: 36 / 5G: 5 / 6G: 1 | DL P8: **`2,690.27 Mbps`**（**MPDU retry `0.75%`**，`MCS9 75.4%`）<br>UL P4: **`1,850.99 Mbps`**（retry `1.58%`，`MCS9 89.4%`） | WDT 报错清零；PMU 就绪；`ethtool -S wan` 实测 `1.02 GB RX / 2.78 GB TX` **0 error / 0 drop** |

---

### 4. 发射功率梯度测试（Pure-AP Phase B v2）

在 OnePlus 13 近场工况下（`fnOS` 已开启 `BBR + fq + 16 MB`），对比 AP 6 GHz 不同 `txpower` 设置：

| 功率档位 | `dl_p1` (Mbps) | `dl_p4` (Mbps) | `dl_p16` (Mbps) | `ul_p4` (Mbps) | 主导 MCS / 备注 |
| :---: | ---: | ---: | ---: | ---: | :--- |
| **`30 dBm`（默认 / 穿墙）** | `1793.15` | `2452.98` (回滚复测 `2364.49`) | **`2632.88`** (回滚复测 **`2713.32`**) | **`1808.16`** (回滚复测 **`1847.91`**) | `MCS8/9`，`tx_retry = 1.19%~6.20%` |
| **`24 dBm`（近场平衡）** | `1830.88` | **`2569.73`** | `2588.40` | `1754.92` | `MCS8/9`，`dl_p4 retry` 降至 `2.67%` |
| **`22 dBm`（极近场低失真）** | **`1913.97`** | `2520.61` | `2630.45` | — | 单流 `dl_p1` 表现最佳，`MCS8/9` |

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
- **从其它分支（如 `gemtek,xr1710g-ubi` / LEDE）切换回本固件**：因 `board_name` 字符串与 `compat_version` 差异，`sysupgrade` 需加 `-F -n`：
  ```sh
  sysupgrade -F -v -n /tmp/immortalwrt-nijigencg-20261125-pure-ap-phasej-p0p1-airoha-an7581-gemtek_xr1710g-squashfs-sysupgrade.itb
  ```

---

<a id="english-summary"></a>
## English Summary

### Testbed & Methodology

- **AP**: Gemtek XR1710G (Airoha AN7581 + MediaTek MT7996 tri-band Wi-Fi 7 + dual 10GbE), configured as a pure L2 bridge AP (`br-lan`, no DHCP server).
- **Client**: OnePlus 13 (`PJZ110`, 2x2 Wi-Fi 7 `320 MHz`, 6 GHz Channel 37 / `6135 MHz`, WPA3-SAE).
- **Wired Peer**: `fnOS` (`192.168.3.2`, 10GbE) tuned with `BBR + fq + 16 MB wmem/rmem/tcp_wmem`.

### Head-to-Head Firmware Comparison

| Firmware | Kernel | `bridger` L2 Offload | 6 GHz TX Power / Tuning | `ppe_bnd` Entries | AP CPU (`cpu0` / Peak) | DL `P1` / `P4` / `P16` (Mbps) | UL `P1` / `P4` / `P16` (Mbps) |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Pure-AP (`Phase G`)** | `6.18.60` | **Enabled** (`cls_bpf`/`cls_flower`/`act_vlan`) | `30 dBm` / `npu_tx_band_map=2`, `radio2 eBF=0` | **`4 ~ 34`** | **`~1%`** | `1890` / **`2780~3050`** / **`2819~3050`** | — / **`2245`** (`MCS 10`) / — |
| **Pure-AP (`Phase B v2`)** | `6.18.60` | **Enabled** | `30 dBm` / `-42 dBm` | `4 ~ 34` | `0.46% ~ 1.62%` (max `12.5%`) | `1793` / `2364~2453` / `2633~2713` | `1594` / `1808~1848` / `1890~1914` |
| **YYH (`xr1710g_260831`)** | `6.18.44` | Missing | `30 dBm` | `0` | **`86.7% ~ 94.0%`** | `42` / `1400` / `1457` | `793` / `775` / `793` |
| **`xmiqz` LEDE (`R26.11.6`)** | `6.18.60` | Missing (`cls_flower`/`act_vlan` absent) | Clamped to **`12 dBm`** (`US`) / `-58 dBm` | `0` | **`74.4% ~ 94.2%`** | `44` / `1066` / `924` (`1145`) | `792` / `808` / `799` |

### Key Takeaways

1. **Phase G reaches `3,049.98 Mbps` DL (`MCS 10 = 58.6%`) / `2,245.39 Mbps` UL (`MCS 10 = 61.6%`)** with `~1%` AP CPU usage by combining `bridger` + Airoha PPE hardware offload, runtime `npu_tx_band_map=2` (moving both 5G and 6G onto PCIe0 Gen3 x2 `15.752 Gb/s`), and disabling 6 GHz `radio2` eBF (`eht_su_beamformer=0` / `he_su_beamformer=0`) to avoid MT7996 firmware stalling at `EHT: 1` / `MCS 7 (98.5%)` with OnePlus 13.
2. **`bridger` + Airoha PPE is mandatory for Pure-AP Wi-Fi 7 throughput**: Without `bridger` and its `tc` modules (`cls_bpf`, `cls_flower`, `act_vlan`), L2 bridge traffic saturates AN7581 `cpu0` at 87–94% and caps speeds at ~1.1–1.5 Gbps DL / ~0.8 Gbps UL.
3. **Identical SPI-NAND UBI 2.0 layout** (`ubi` at `0x00700000`, size `0x1b700000`, 439 MiB) across `naoki66`, this Pure-AP baseline, `YYH`, and `xmiqz` LEDE; cross-branch flashing uses `sysupgrade -F -v -n`.
