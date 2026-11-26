# ImmortalWrt for Gemtek XR1710G — Pure-AP Wi-Fi 7 Baseline

[中文说明](#中文说明) | [English](#english)

---

<a id="中文说明"></a>
## 中文说明

### 固件简介

本仓库基于 [naoki66/ImmortalWrt-for-Gemtek-brightspeed](https://github.com/naoki66/ImmortalWrt-for-Gemtek-brightspeed)（上游提交 `475b6b67ca` / Linux `6.18.60`），面向 **Gemtek XR1710G**（Airoha AN7581 四核 A53 + MediaTek MT7996 三频 Wi-Fi 7 + 双 10GbE RTL8261BE）打造的**永久纯 AP（Pure-AP-Only）高吞吐二层无线桥接基线固件**：

- **预编译固件下载**：见本仓库 [Releases (`xr1710g-pure-ap-phaseJ-p0p1-20261125`)](https://github.com/nijigenCG/ImmortalWrt-for-Gemtek-brightspeed/releases/tag/xr1710g-pure-ap-phaseJ-p0p1-20261125)（`.itb` `20.54 MiB`，`SHA256: 4b8ac7d234f495e1f3dcfd6082fe46461ea6b82d4a4a5b9bfd8c9b206c008c4a`）。
- **Phase J (`P0+P1`) 在 Phase G 峰值吞吐引擎基础上新增的稳定性与诊断加固**：
  - **P0（`mt76` / NPU / RRO 稳定性大修）**：解包修复上游原双层包装未生效的 `128/129/132/134`（Full-Reset/L1 SER NPU 自愈）；加固 `126-v2` NPU RX 环防越界/防死锁；合入 `openwrt/mt76` 11 月 24 个核心修复（HW-RRO `qos_ctl` 修 CCMP 重放误丢包、GCMP-256/BIGTK PN 保护、TWT/MLD PS-sync 保护非 MLD IoT 设备、WTBL 竞态与 L1 SER 信道恢复）；合入 `yahuisme` `913`（NPU 多描述符 TXFREE 线性化防越界读）与 `914`（RRO 页面防泄漏）；移除风险死补丁 `133`。
  - **P1（零报错开机与 10GbE / 射频运维增强）**：`procd/001` 协调 AN7581 28s 硬件看门狗彻底清零开机 `dmesg` 报错；`iwinfo 998/999` 修复 single-wiphy 分频段独立扫描与频率精确功率列表；主线 `844a34e63f` 启用 `arm,cortex-a53-pmu`；`747-02` + `9999-40` 支持 RTL8261BE 实报真实协商速率与 `ethtool -S` 29 项 GDM 硬件统计；清理双层死补丁 `929-03`。
- **设计理念**：只做极致稳定、低 CPU 占用的有线 ↔ 无线二层高速桥接 AP，不与上级软路由/光猫拨号抢 DHCP、路由、代理资源：
  - **完整保留硬件转发与 Wi-Fi 7 引擎**：
    - Airoha **NPU (`0.1111`)** + **PPE / PPE2 / L2B** 硬件转发，配合 **`bridger` eBPF 桥接加速器**（含 `cls_bpf` / `cls_flower` / `act_vlan`），实现无线 ↔ 有线二层流表纯硬件卸载。
    - `mt76` / `mt7996e` 三频 Wi-Fi 7（2.4G / 5G 160 MHz / 6G `320 MHz` EHT + `luci-app-mlo`），**6 GHz 完整支持 `30 dBm` 发射功率**；Phase G 新增运行时 **`npu_tx_band_map`**（支持把 5G/6G 上下行全切至 **PCIe0 Gen3 x2 `15.752 Gb/s`**）及 **EHT 320 MHz `STA_REC_BF` / `STA_REC_HE_V2` / RC-BF 同步修复**。
    - **精简纯 AP 管理与诊断**：`luci-base`（`luci-theme-bootstrap` 轻量网页管理）、`uhttpd`（HTTP `:80`）、`dropbear` SSH（`:22`）、最小化 `firewall4` 网桥防火墙、Airoha 硬件管理套件（风扇控制 `luci-app-airoha-fancontrol`、NPU `luci-app-airoha-npu`、恢复与出厂工具），内置 `iperf3`、`ethtool-full`、`iw`、`tcpdump`、`i2c-tools`。
  - **永久精简冗余路由器组件**：
    - 彻底移除 `ppp*`、`dnsmasq*`、`odhcpd*`、`kmod-nft-fullcone`、`luci-app-firewall`、`kmod-nf-nathelper`、`luci-theme-argon`、`ruby` 及所有 VPN / 代理 / DDNS / UPnP 插件，杜绝 DHCP 冲突与后台杂音。

---

### 实测对比测试总结（一加 13 6 GHz `320 MHz` ↔ 10GbE `fnOS`）

完整测试环境、多功率矩阵（`30/24/22/20 dBm`）、Phase G 6 GHz eBF 锁死 `MCS 7` 排查与 PCIe0 分流实测分析请见：[docs/BENCHMARK_REPORT.md](docs/BENCHMARK_REPORT.md)。

| 固件版本 | 内核 | `bridger` 二层卸载 | 6 GHz 发射功率 / 关键调优 | PPE 绑定数 (`ppe_bnd`) | AP CPU 占用 (`cpu0` / 峰值核) | 下行 `P1` / `P4` / `P16` (Mbps) | 上行 `P1` / `P4` / `P16` (Mbps) |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **本 Pure-AP (`Phase J`, P0+P1 + IRQ 分核)** | `6.18.60` | **开启** (`cls_bpf`/`cls_flower`/`act_vlan`) | **`30.00 dBm`** / `npu_tx_band_map=2` + **四核 IRQ 分流**（`-7~-9 dBm`） | **`4 ~ 34`** | **`~1%`** (硬件全卸载，**`dmesg` 零报错**) | `1823` / **`3237`** (`P8:` **`3419`**) / **`3326`** | — / **`3183`** / **`3344`** (`P8`, 1s 峰值 **`3519`**) |
| **本 Pure-AP (`Phase G`)** | `6.18.60` | **开启** (`cls_bpf`/`cls_flower`/`act_vlan`) | **`30.00 dBm`** / `npu_tx_band_map=2` + `radio2 eBF=0` | **`4 ~ 34`** | **`~1%`** (硬件全卸载) | `1890` / **`2780 ~ 3050`** / **`2819 ~ 3050`** | — / **`2245`** (`MCS 10 = 61.6%`) / — |
| **Pure-AP (`Phase B v2`)** | `6.18.60` | **开启** | `30.00 dBm` / `-42 dBm` | `4 ~ 34` | `0.46% ~ 1.62%` (峰值 `12.5%`) | `1793` / `2364 ~ 2453` / `2633 ~ 2713` | `1594` / `1808 ~ 1848` / `1890 ~ 1914` |
| **YYH (`xr1710g_260831`)** | `6.18.44` | **缺失** | `30.00 dBm` | `0` | `86.7% ~ 94.0%` (`cpu0` 跑满) | `42` / `1400` / `1457` | `793` / `775` / `793` |
| **`xmiqz` LEDE (`R26.11.6`)** | `6.18.60` | **缺失** (`cls_flower`/`act_vlan` 缺失) | 被 `US` 锁死 **`12.00 dBm`** / `-58 dBm` | `0` | `74.4% ~ 94.2%` (`cpu0`/`cpu2` 跑满) | `44` / `1066` / `924` (复测 `1145`) | `792` / `808` / `799` |

**核心结论**：
1. **纯 AP 下 `bridger` + Airoha PPE 决定性能天花板**：在二层网桥纯 AP 模式下，`YYH` 和 `xmiqz` LEDE 均因缺少 `bridger` 与 `tc` 卸载模块导致 `ppe_bnd = 0`，无线↔有线转发全部压在 AN7581 `cpu0` 软中断上（87%–94% 满载），速率直接腰斩到 `1.1–1.5 Gbps`；本基线版全程硬件直通，AP CPU 仅 **`~1%`**。
2. **Phase G 推荐运行时调优（稳定冲上 `3.05 Gbps` DL / `2.25 Gbps` UL）**：
   - **PCIe0 Gen3 x2 分流持久化（`npu_tx_band_map=2`）**：将 5G + 6G 上下行全切至 PCIe0 Gen3 x2（`15.752 Gb/s`），仅留 2.4G 在 PCIe2 Gen2 x1（`4.000 Gb/s`）：
     ```sh
     echo 'mt7996e npu_tx_band_map=2' > /etc/modules.d/mt7996e
     echo 2 > /sys/module/mt7996e/parameters/npu_tx_band_map
     /etc/init.d/bridger restart
     ```
   - **6 GHz 关闭设备级 eBF（避免一加 13 触发 `EHT: 1` 后锁死 `MCS 7`）**：
     ```sh
     uci set wireless.radio2.eht_su_beamformer='0'
     uci set wireless.radio2.eht_mu_beamformer='0'
     uci set wireless.radio2.he_su_beamformer='0'
     uci set wireless.radio2.he_mu_beamformer='0'
     uci commit wireless && wifi reload radio2
     ```
3. **配合服务端 `BBR + fq + 16 MB` 缓冲效果最佳**：内网有线服务端（如 NAS / 宿主机）开启 `BBR + fq` 与 `16 MB` `wmem/rmem/tcp_wmem` 后，6 GHz Wi-Fi 7 可稳定发挥全速。

---

### 源码改动明细（相比上游 `naoki66` `475b6b67ca`）

1. **修复上游内核及 `mt76` 补丁构建缺陷（`9999-16`、`126` & `9995`）**
   - 内核：`target/linux/airoha/patches-6.18/9999-16-net-airoha-ppe-debugfs-forward-decl-flow-stats-available.patch` — 为 `airoha_ppe_debugfs.c` 补充 `airoha_ppe_flow_stats_available()` 前置声明，解决 GCC 14.4 编译报错。
   - 无线：`package/kernel/mt76/patches/126-mt76-npu-fix-rx-ring-handling.patch` & `9995-wifi-mt76-mt7996-advertise-beacon-protection.patch` — 修正 hunk 计数与上下文。
2. **纯 AP 配置与包依赖解耦（`1710-ap-only.config`）**
   - `package/emortal/default-settings/Makefile`：`+luci` → `+luci-base`，脱离完整路由 UI 仍保留中文与基础设置。
   - `package/network/config/firewall4/Makefile`：移除硬编码 `+kmod-nft-fullcone` 依赖；停用坏损 `argon` footer 补丁，启用独立 `1710-ap-only.config` 并集成 `iperf3`。
3. **MT7996 Phase B 驱动调试与 EHT320 / AMSDU 修复（`9996` & `9997`）**
   - `9996-wifi-mt76-mt7996-debugfs-txpower-sku-and-bf-backoff-ctrl.patch`：在 `/sys/kernel/debug/ieee80211/phy0/mt76/` 增加运行时动态开关 `sku_power_ctrl` 与 `bf_backoff_ctrl`（默认 `1`，支持免重启调优）。
   - `9997-wifi-mt76-mt7996-fix-eht320-sounding-dim-and-amsdu-num.patch`：修复 `mt7996_mcu_sta_bfer_eht()` 中 320 MHz EHT Sounding Dimension 位移（`<< 2` 替代错误的 `<< 1`），并将 `amsdu->max_amsdu_num` 从 `8` 提升至 `31`。
4. **MT7996 Phase G NPU PCIe0 分流与 EHT320 BF/HE/RC 修复（`9998` & `9999`）**
   - `9998-wifi-mt76-mt7996-npu-6g-pcie0-wdma0.patch`：为 `mt7996e` 新增 `0644` 运行时模块参数 `npu_tx_band_map`（`0=原厂默认`, `1=6G->PCIe0`, `2=5G+6G->PCIe0` Gen3 x2 `15.752 Gb/s`），配合 `/etc/init.d/bridger restart` 免重启即时生效。
   - `9999-wifi-mt76-mt7996-fix-eht320-bf-he-bw320-and-rc-sync.patch`：修复 `mt7996_mcu_sta_bfer_eht()` 的 BW320→BW160 `fallthrough` 维度填充、`ncol <= nrow` 钳位与 `ng16`/`codebook` 字段解析；修复 `mt7996_mcu_sta_he_tlv()` 遗漏 `IEEE80211_STA_RX_BW_320` 分支；修复 `mt7996_mcu_add_rate_ctrl()` 变更时同步 `STA_REC_BF`；新增精简 `STA_REC_BF` 诊断日志。

---

### 刷写与使用指引

#### 分区兼容性与跨版本刷机

本固件、`naoki66` 原版、`YYH` (`gemtek,xr1710g-ubi`) 与 `xmiqz` LEDE 均使用**完全一致的 SPI-NAND UBI 2.0 分区布局**（`ubi` 起始 `0x00700000`、大小 `0x1b700000` / 439 MiB）。

- **从同系列（`gemtek,xr1710g`）升级**：直接在 LuCI 或 SSH 执行 `sysupgrade -v <itb>`。
- **从 YYH（`gemtek,xr1710g-ubi`）或 LEDE 刷入本固件**：因 `board_name` 字符串与原分支配置结构不同，请通过 SSH 上传 `.itb` 到 `/tmp/` 后使用 `-F -n`（不保留原路由配置，防止带入冲突项）：
  ```sh
  sysupgrade -F -v -n /tmp/immortalwrt-nijigencg-20261125-pure-ap-phasej-p0p1-airoha-an7581-gemtek_xr1710g-squashfs-sysupgrade.itb
  ```
- **Pure-AP 首次上线配置提示**：本固件**不含 DHCP 服务器**（默认 `br-lan` 为静态管理地址）。建议刷入时通过精简 `sysupgrade -f` 仅保留 `network` / `wireless` / `system` / `dropbear` / `shadow`，或刷后先网线直连修改 `lan` IP 与上级主路由网关（示例：`192.168.3.4/24`，网关指向主路由 `192.168.3.1`）。

---

### 自行编译与门禁验证

> **重要文件系统警告**：严禁在带私有 ACL 掩码的 NAS 目录（如强制文件为 `0700` 的 Btrfs `trimacl` 挂载点）直接编译固件，否则 `root.squashfs` 中 `/sbin/ubusd`、`/lib/libc.so` 与非 root 守护进程会因权限缺失导致无法启动。请务必在**标准 POSIX 权限文件系统**（如原生 `ext4` 或 `ext4` 回环镜像）中克隆并构建。

```bash
# 1. 更新并安装 feeds
./scripts/feeds update -a
./scripts/feeds install -a
FEED_PATCHES_STRICT=1 bash scripts/select-feed-providers.sh
FEED_PATCHES_STRICT=1 bash scripts/apply-feed-patches.sh

# 2. 使用纯 AP 配置
cp 1710-ap-only.config .config
make defconfig

# 若构建机已安装 clang/llvm >= 15，可改用宿主 BPF 工具链加速构建：
# scripts/config --enable BPF_TOOLCHAIN_HOST --set-str BPF_TOOLCHAIN_HOST_PATH /path/to/llvm15 --enable USE_LLVM_HOST --disable BPF_TOOLCHAIN_BUILD_LLVM --disable USE_LLVM_BUILD
# make defconfig

# 3. 下载源码并编译
make download -j$(nproc)
make -j$(nproc) world
```

**刷机前必检门禁**：使用 `unsquashfs4 -ll` 确认 `.itb` 内 `rootfs-1` 系统目录及二进制权限为标准 POSIX（`0755`/`0644`，无 `0700`），建议通过 QEMU `aarch64` 冷启冒烟测试及 `sysupgrade -T` 校验后再刷入真机。

---

### 鸣谢

- 上游固件与 Airoha AN7581 / XR1710G 基础工程：[naoki66/ImmortalWrt-for-Gemtek-brightspeed](https://github.com/naoki66/ImmortalWrt-for-Gemtek-brightspeed)
- [ImmortalWrt](https://github.com/immortalwrt/immortalwrt) 与 [OpenWrt](https://openwrt.org/) 社区

---

<a id="english"></a>
## English

### Overview

This repository maintains a **Pure-AP-Only Wi-Fi 7 baseline** for the **Gemtek XR1710G** (Airoha AN7581 quad-core A53 + MediaTek MT7996 tri-band Wi-Fi 7 + dual 10GbE RTL8261BE), derived from [naoki66/ImmortalWrt-for-Gemtek-brightspeed](https://github.com/naoki66/ImmortalWrt-for-Gemtek-brightspeed) (`475b6b67ca`, Linux `6.18.60`).

Prebuilt firmware & checksums are published under [Releases (`xr1710g-pure-ap-phaseJ-p0p1-20261125`)](https://github.com/nijigenCG/ImmortalWrt-for-Gemtek-brightspeed/releases/tag/xr1710g-pure-ap-phaseJ-p0p1-20261125) (`20.54 MiB` / `21,532,990` bytes, `SHA256: 4b8ac7d234f495e1f3dcfd6082fe46461ea6b82d4a4a5b9bfd8c9b206c008c4a`).

- **Kept for maximum L2 AP bridge performance**:
  - Airoha **NPU (`0.1111`)** + **PPE / PPE2 / L2B** hardware offload with **`bridger`** eBPF bridge acceleration (`cls_bpf`, `cls_flower`, `act_vlan`).
  - Tri-band MT7996 Wi-Fi 7 (`2.4G / 5G 160 MHz / 6G 320 MHz EHT`, `wpad-mesh-mbedtls`, `luci-app-mlo`) with full `30 dBm` 6 GHz TX power, runtime **`npu_tx_band_map`** (`0=stock`, `1=6G->PCIe0`, `2=5G+6G->PCIe0` Gen3 x2 `15.752 Gb/s`), and Phase G EHT320 BF/HE/RC fixes.
  - Lightweight UI & hardware utilities (`luci-base` + Bootstrap, `uhttpd` HTTP `:80`, `dropbear` SSH `:22`, minimal `firewall4`, Airoha fan/NPU/recovery LuCI apps) plus `iperf3`, `ethtool-full`, `iw`, `tcpdump`, and `i2c-tools`.
- **Permanently removed**: `ppp*`, `dnsmasq*`, `odhcpd*`, `kmod-nft-fullcone`, `luci-app-firewall`, `kmod-nf-nathelper`, `luci-theme-argon`, `ruby`, and all VPN/proxy/DDNS/UPnP router bloat.

### Comparative Benchmark Summary (OnePlus 13 6 GHz `320 MHz` ↔ 10GbE `fnOS`)

See the full analysis, Phase G PCIe0 & 6 GHz eBF A/B experiments, and server tuning notes in [docs/BENCHMARK_REPORT.md](docs/BENCHMARK_REPORT.md).

| Firmware | Kernel | `bridger` L2 Offload | 6 GHz Power / Key Tuning | `ppe_bnd` Entries | AP CPU (`cpu0` / Peak) | DL `P1` / `P4` / `P16` (Mbps) | UL `P1` / `P4` / `P16` (Mbps) |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Pure-AP (`Phase G`)** | `6.18.60` | **Enabled** | **`30.00 dBm`** / `npu_tx_band_map=2`, `radio2 eBF=0` | **`4 ~ 34`** | **`~1%`** | `1890` / **`2780~3050`** / **`2819~3050`** | — / **`2245`** (`MCS 10`) / — |
| **Pure-AP (`Phase B v2`)** | `6.18.60` | **Enabled** | `30.00 dBm` / `-42 dBm` | `4 ~ 34` | `0.46% ~ 1.62%` (`12.5%`) | `1793` / `2364~2453` / `2633~2713` | `1594` / `1808~1848` / `1890~1914` |
| **YYH (`xr1710g_260831`)** | `6.18.44` | Missing | `30.00 dBm` | `0` | `86.7% ~ 94.0%` | `42` / `1400` / `1457` | `793` / `775` / `793` |
| **`xmiqz` LEDE (`R26.11.6`)** | `6.18.60` | Missing | Locked to **`12.00 dBm`** / `-58 dBm` | `0` | `74.4% ~ 94.2%` | `44` / `1066` / `924` (`1145`) | `792` / `808` / `799` |

### Key Source Changes over Upstream `475b6b67ca`

1. **Kernel & `mt76` Build Fixes (`9999-16`, `126`, `9995`)**: Adds missing forward declaration for `airoha_ppe_flow_stats_available()` in `airoha_ppe_debugfs.c` and fixes patch `126`/`9995` hunk formatting.
2. **Pure-AP Dependency Decoupling (`1710-ap-only.config`)**: Switches `default-settings` to `+luci-base`, removes `kmod-nft-fullcone` from `firewall4`, disables broken `argon` footer patch, and enables `iperf3`.
3. **MT7996 Phase B Controls & EHT320 Fixes (`9996`, `9997`)**: Adds live debugfs switches `sku_power_ctrl` / `bf_backoff_ctrl`, fixes `mt7996_mcu_sta_bfer_eht()` 320 MHz sounding dim shift (`<< 2`), and raises `max_amsdu_num` from `8` to `31`.
4. **MT7996 Phase G PCIe0 NPU Map & EHT320 BF/HE/RC Fixes (`9998`, `9999`)**: Adds runtime `npu_tx_band_map` (`0=stock`, `1=6G->PCIe0`, `2=5G+6G->PCIe0` Gen3 x2 `15.752 Gb/s`), fixes `mt7996_mcu_sta_bfer_eht()` BW320→BW160 fallthrough + clamping + `ng16`/`codebook`, adds `BW320` case in `mt7996_mcu_sta_he_tlv()`, syncs `STA_REC_BF` in `mt7996_mcu_add_rate_ctrl()`, and logs concise `STA_REC_BF` diagnostics.

### Flashing & Partition Compatibility

`naoki66`, this Pure-AP baseline (`gemtek,xr1710g`), `YYH` (`gemtek,xr1710g-ubi`), and `xmiqz` LEDE share the identical SPI-NAND UBI 2.0 layout (`ubi` at `0x00700000`, size `0x1b700000`, 439 MiB). When switching across variants with a different `board_name`, flash cleanly with:

```sh
sysupgrade -F -v -n /tmp/immortalwrt-nijigencg-20261125-pure-ap-phasej-p0p1-airoha-an7581-gemtek_xr1710g-squashfs-sysupgrade.itb
```

Note that `dnsmasq` and `odhcpd` are omitted by design; configure a static management IP on `br-lan` pointing to your upstream main router.

### Build & Pre-Flash Verification

> **POSIX Filesystem Requirement**: Always build on a standard POSIX filesystem (native `ext4` or an `ext4` loopback image), never on ACL-clamped NAS mounts (`trimacl` forcing `0700`), or rootfs binaries (`ubusd`, `libc.so`, etc.) will fail on boot.

```bash
./scripts/feeds update -a
./scripts/feeds install -a
FEED_PATCHES_STRICT=1 bash scripts/select-feed-providers.sh
FEED_PATCHES_STRICT=1 bash scripts/apply-feed-patches.sh

cp 1710-ap-only.config .config
make defconfig
make download -j$(nproc)
make -j$(nproc) world
```

Verify `rootfs-1` permissions (`0755`/`0644`, zero `0700` dirs) via `unsquashfs4 -ll`, boot-test userspace in QEMU `aarch64`, and validate `/sbin/sysupgrade -T` before flashing.

### Credits

- Upstream Airoha AN7581 / XR1710G work: [naoki66/ImmortalWrt-for-Gemtek-brightspeed](https://github.com/naoki66/ImmortalWrt-for-Gemtek-brightspeed)
- [ImmortalWrt](https://github.com/immortalwrt/immortalwrt) & [OpenWrt](https://openwrt.org/)
