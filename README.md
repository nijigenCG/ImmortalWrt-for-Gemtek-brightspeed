# ImmortalWrt for Gemtek XR1710G — Pure-AP Wi-Fi 7 Baseline

[中文说明](#中文说明) | [English](#english)

---

<a id="中文说明"></a>
## 中文说明

### 固件简介

本仓库基于 [naoki66/ImmortalWrt-for-Gemtek-brightspeed](https://github.com/naoki66/ImmortalWrt-for-Gemtek-brightspeed)（上游提交 `475b6b67ca` / Linux `6.18.60`），面向 **Gemtek XR1710G**（Airoha AN7581 四核 A53 + MediaTek MT7996 三频 Wi-Fi 7 + 双 10GbE RTL8261BE）打造的**永久纯 AP（Pure-AP-Only）高吞吐二层无线桥接基线固件**：

- **预编译固件下载**：见本仓库 [Releases (`xr1710g-pure-ap-phaseJ-v2-20261126`)](https://github.com/nijigenCG/ImmortalWrt-for-Gemtek-brightspeed/releases/tag/xr1710g-pure-ap-phaseJ-v2-20261126)（`.itb` `20.54 MiB` / `21,532,993` bytes，`SHA256: 8ffbbfdfd7c16069fe33c9d1fdfcf49f49db5bda5ce12c0098ff7994c459dd6f`）。
- **Phase J (`P0+P1`) 在 Phase G 峰值吞吐引擎基础上新增的稳定性与诊断加固**：
  - **P0（`mt76` / NPU / RRO 稳定性大修）**：解包修复上游原双层包装未生效的 `128/129/132/134`（Full-Reset/L1 SER NPU 自愈）；加固 `126-v2` NPU RX 环防越界/防死锁；合入 `openwrt/mt76` 11 月 24 个核心修复（HW-RRO `qos_ctl` 修 CCMP 重放误丢包、GCMP-256/BIGTK PN 保护、TWT/MLD PS-sync 保护非 MLD IoT 设备、WTBL 竞态与 L1 SER 信道恢复）；合入 `yahuisme` `913`（NPU 多描述符 TXFREE 线性化防越界读）与 `914`（RRO 页面防泄漏）；移除风险死补丁 `133`。
  - **Phase J v2 (`20261126`, 开箱即用冷启动固化)**：内置 `/etc/rc.local`（冷启动自动生效 `npu_tx_band_map=2` + 四核 GICv3 IRQ 分流 `airoha_eth.4→CPU1` / `mt76-npu.0→CPU2` / `mt76-npu.1→CPU3`）；`03_wireless` 默认关闭 6 GHz `radio2` TX Beamformer（冷启动全程 `eBF: 0`，根除过夜 EHT Sounding 锁死并保留配置升级时安全自愈空白 `rc.local`）。
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
| **本 Pure-AP (`Phase J v2`, 20261126 冷启动)** | `6.18.60` | **开启** (`cls_bpf`/`cls_flower`/`act_vlan`) | **`30.00 dBm`** / `map=2` + **四核 IRQ** + **6G `eBF=0` 开箱内置** | **`4 ~ 34`** | **`~1%`** (硬件全卸载，**`dmesg` 零报错**) | **`3092`** (`CUBIC`, `0` 重传) / **`3193`** (`P8:` **`3448`**) / **`3497`** (`1s:` **`3649`**) | — / **`3124`** / **`3312`** (`P8`, `1s:` **`3622`**) |
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

#### 1. 全新刷入后的出厂默认配置（不保留配置 / `sysupgrade -n`）

| 项目 | 默认值 | 说明 |
| :--- | :--- | :--- |
| **管理后台地址** | `http://192.168.50.1` | 静态 IP `192.168.50.1/24`（子网掩码 `255.255.255.0`），主机名 `XR1710G`，SSH 端口 `22` |
| **登录用户名 / 密码** | `root` / **空（无密码）** | LuCI 网页与 SSH 均支持空密码首次登录，**登录后请第一时间修改密码** |
| **默认网口分工** | LAN：`lan2`、`lan3`、`lan4`（`br-lan`）<br>WAN：`wan`（独立 DHCP 客户端） | 默认防火墙拒绝 WAN 口入站，**首次配置请连 `lan2`/`lan3`/`lan4` 或默认 Wi-Fi，不要先插 `wan` 口** |
| **DHCP 服务** | **已彻底移除（无 DHCP 服务端）** | 电脑/手机刚刷完连网线或 Wi-Fi **不会自动获取 IP**，需先手动配置 `192.168.50.x/24` 静态 IP |
| **默认 Wi-Fi（国家码 `US`）** | 2.4G：**`ImmortalWrt-2G`**（`WPA2-PSK` / `HE20` / 自动信道 / `28 dBm`）<br>5G：**`ImmortalWrt-5G`**（`WPA2/WPA3` / `EHT160` / 信道 `36` / `30 dBm`）<br>6G：**`ImmortalWrt-6G`**（`WPA3-SAE` / `EHT320` / 信道 `37` / `30 dBm`，默认关闭 TX `eBF`） | **三频统一初始无线密码：`12345678`**；首次登录后请及时修改 SSID 与无线密码 |

> **关键提示**：由于本固件永久精简了 `dnsmasq` 与 `odhcpd`，全新刷机重启后**必须给电脑或手机手动指定静态 IP**（例如 IP `192.168.50.10`、子网掩码 `255.255.255.0`，网关留空或填 `192.168.50.1`），然后通过网线接入 `lan2`/`lan3`/`lan4`（或连接上述默认 Wi-Fi），即可打开 `http://192.168.50.1` 或 `ssh root@192.168.50.1` 进入管理界面。

#### 2. 首次上线配置为 Pure-AP（四口全桥接二层 AP）

完成以下几步后，即可把全部 4 个网口（双 10GbE `wan`/`lan2` + 双 1GbE `lan3`/`lan4`）合并为同一个二层网桥，接入你的主路由：

**方式 A：通过 LuCI 网页配置（推荐新手）**
1. 电脑手动配静态 IP `192.168.50.10/24`，网线接 `lan2`/`lan3`/`lan4`（或连默认 Wi-Fi `ImmortalWrt-5G`，密码 `12345678`），访问 `http://192.168.50.1`，用户名 `root`、密码留空登录。
2. **设置登录密码**：进入「系统 → 管理权」，设置 `root` 密码并保存。
3. **关闭并删除独立 WAN 接口**：进入「网络 → 接口」，删除 `wan` 和 `wan6` 接口。
4. **将 `wan` 物理口并入 `br-lan`**：进入「网络 → 接口 → 设备」，编辑 `br-lan`，在「网桥端口」中勾选/添加 `wan`（最终包含 `lan2`、`lan3`、`lan4`、`wan`），点击保存。
5. **修改管理 IP 为主路由同网段**：进入「网络 → 接口」，编辑 `lan`：
   - **静态管理 IP（推荐）**：将 IPv4 地址改为主路由同网段未占用地址（例如主路由是 `192.168.3.1`，则填 `192.168.3.4/24`），IPv4 网关与自定义 DNS 均填主路由 IP（如 `192.168.3.1`）；
   - **或改为 DHCP 客户端**：若希望由主路由自动分配管理 IP，也可直接将 `lan` 协议切换为 `DHCP 客户端`。
   - 点击「保存并应用」（若弹出回滚倒计时且已切换网段，请选择「强制应用」）。
6. **修改 Wi-Fi 名称与密码**：进入「网络 → 无线」，按需修改 2.4G / 5G / 6G 的 SSID 与无线密码（**建议保持 6 GHz `radio2` 的 TX Beamformer 关闭状态**，避免过夜 EHT Sounding 锁速）。
7. **接入主路由**：将电脑网卡改回「自动获取 IP (DHCP)」，用网线将 XR1710G 任意 10GbE 网口（原 `wan` 或 `lan2`）连到上级主路由/交换机，之后直接通过新管理 IP（如 `http://192.168.3.4`）访问即可。

**方式 B：通过 SSH 一键配置（以主路由 `192.168.3.1`、AP 管理地址 `192.168.3.4/24` 为例）**
```sh
# 1. 电脑手动配 192.168.50.10/24 后登录：ssh root@192.168.50.1（空密码）
passwd  # 修改 root 密码

# 2. 四口全桥接 + 删除独立 wan/wan6 + 设为主路由同网段静态管理 IP
uci -q del_list network.@device[0].ports='wan'
uci add_list network.@device[0].ports='wan'
uci -q delete network.wan
uci -q delete network.wan6
uci set network.lan.proto='static'
uci -q delete network.lan.ipaddr
uci add_list network.lan.ipaddr='192.168.3.4/24'
uci -q delete network.lan.netmask
uci set network.lan.gateway='192.168.3.1'
uci -q delete network.lan.dns
uci add_list network.lan.dns='192.168.3.1'
uci commit network
/etc/init.d/network restart
```
> 若希望 `br-lan` 由上级主路由 DHCP 自动分配管理 IP，把上面 `network.lan` 相关几行替换为：`uci set network.lan.proto='dhcp' && uci -q delete network.lan.ipaddr && uci -q delete network.lan.netmask && uci -q delete network.lan.gateway && uci -q delete network.lan.dns`。
>
> **Phase J v2 开箱自动优化说明**：本镜像已将 `npu_tx_band_map=2`、四核 GICv3 IRQ 分核（`/etc/rc.local`）以及 6 GHz `radio2` 默认关闭 `eBF` 全部内置，刷完即可冷启动生效，无需再手动执行 Phase G 调优命令。

#### 3. 分区兼容性与跨版本刷机

本固件、`naoki66` 原版、`YYH` (`gemtek,xr1710g-ubi`) 与 `xmiqz` LEDE 均使用**完全一致的 SPI-NAND UBI 2.0 分区布局**（`ubi` 起始 `0x00700000`、大小 `0x1b700000` / 439 MiB）。

- **从同系列（`gemtek,xr1710g`）升级**：直接在 LuCI 或 SSH 执行 `sysupgrade -v <itb>`（建议仅保留 `network` / `wireless` / `system` / `dropbear` / `shadow` 等纯 AP 配置；若旧 `/etc/rc.local` 未改动，升级后会自动刷新为新版内置脚本）。
- **从 YYH（`gemtek,xr1710g-ubi`）或 LEDE 刷入本固件**：因 `board_name` 字符串与原分支配置结构不同，请通过 SSH 上传 `.itb` 到 `/tmp/` 后使用 `-F -n`（不保留原路由配置，防止带入冲突项）：
  ```sh
  sysupgrade -F -v -n /tmp/immortalwrt-nijigencg-20261126-pure-ap-phasej-v2-airoha-an7581-gemtek_xr1710g-squashfs-sysupgrade.itb
  ```
  刷机重启完成后，按上方「第 1、2 节」手动设 `192.168.50.x/24` 静态 IP 接入 `http://192.168.50.1` 完成首次配置即可。

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

Prebuilt firmware & checksums are published under [Releases (`xr1710g-pure-ap-phaseJ-v2-20261126`)](https://github.com/nijigenCG/ImmortalWrt-for-Gemtek-brightspeed/releases/tag/xr1710g-pure-ap-phaseJ-v2-20261126) (`20.54 MiB` / `21,532,993` bytes, `SHA256: 8ffbbfdfd7c16069fe33c9d1fdfcf49f49db5bda5ce12c0098ff7994c459dd6f`).

- **Kept for maximum L2 AP bridge performance**:
  - Airoha **NPU (`0.1111`)** + **PPE / PPE2 / L2B** hardware offload with **`bridger`** eBPF bridge acceleration (`cls_bpf`, `cls_flower`, `act_vlan`).
  - Tri-band MT7996 Wi-Fi 7 (`2.4G / 5G 160 MHz / 6G 320 MHz EHT`, `wpad-mesh-mbedtls`, `luci-app-mlo`) with full `30 dBm` 6 GHz TX power, runtime **`npu_tx_band_map`** (`0=stock`, `1=6G->PCIe0`, `2=5G+6G->PCIe0` Gen3 x2 `15.752 Gb/s`), and Phase G EHT320 BF/HE/RC fixes.
  - Lightweight UI & hardware utilities (`luci-base` + Bootstrap, `uhttpd` HTTP `:80`, `dropbear` SSH `:22`, minimal `firewall4`, Airoha fan/NPU/recovery LuCI apps) plus `iperf3`, `ethtool-full`, `iw`, `tcpdump`, and `i2c-tools`.
- **Permanently removed**: `ppp*`, `dnsmasq*`, `odhcpd*`, `kmod-nft-fullcone`, `luci-app-firewall`, `kmod-nf-nathelper`, `luci-theme-argon`, `ruby`, and all VPN/proxy/DDNS/UPnP router bloat.

### Comparative Benchmark Summary (OnePlus 13 6 GHz `320 MHz` ↔ 10GbE `fnOS`)

See the full analysis, Phase G PCIe0 & 6 GHz eBF A/B experiments, and server tuning notes in [docs/BENCHMARK_REPORT.md](docs/BENCHMARK_REPORT.md).

| Firmware | Kernel | `bridger` L2 Offload | 6 GHz Power / Key Tuning | `ppe_bnd` Entries | AP CPU (`cpu0` / Peak) | DL `P1` / `P4` / `P16` (Mbps) | UL `P1` / `P4` / `P16` (Mbps) |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Pure-AP (`Phase J v2`)** | `6.18.60` | **Enabled** | **`30.00 dBm`** / `map=2`, **4-core IRQ**, **6G `eBF=0` built-in** | **`4 ~ 34`** | **`~1%`** (zero `dmesg` errors) | **`3092`** (`CUBIC`) / **`3193`** (`P8:` **`3448`**) / **`3497`** (`1s:` **`3649`**) | — / **`3124`** / **`3312`** (`P8`, `1s:` **`3622`**) |
| **Pure-AP (`Phase G`)** | `6.18.60` | **Enabled** | **`30.00 dBm`** / `npu_tx_band_map=2`, `radio2 eBF=0` | **`4 ~ 34`** | **`~1%`** | `1890` / **`2780~3050`** / **`2819~3050`** | — / **`2245`** (`MCS 10`) / — |
| **Pure-AP (`Phase B v2`)** | `6.18.60` | **Enabled** | `30.00 dBm` / `-42 dBm` | `4 ~ 34` | `0.46% ~ 1.62%` (`12.5%`) | `1793` / `2364~2453` / `2633~2713` | `1594` / `1808~1848` / `1890~1914` |
| **YYH (`xr1710g_260831`)** | `6.18.44` | Missing | `30.00 dBm` | `0` | `86.7% ~ 94.0%` | `42` / `1400` / `1457` | `793` / `775` / `793` |
| **`xmiqz` LEDE (`R26.11.6`)** | `6.18.60` | Missing | Locked to **`12.00 dBm`** / `-58 dBm` | `0` | `74.4% ~ 94.2%` | `44` / `1066` / `924` (`1145`) | `792` / `808` / `799` |

### Key Source Changes over Upstream `475b6b67ca`

1. **Kernel & `mt76` Build Fixes (`9999-16`, `126`, `9995`)**: Adds missing forward declaration for `airoha_ppe_flow_stats_available()` in `airoha_ppe_debugfs.c` and fixes patch `126`/`9995` hunk formatting.
2. **Pure-AP Dependency Decoupling (`1710-ap-only.config`)**: Switches `default-settings` to `+luci-base`, removes `kmod-nft-fullcone` from `firewall4`, disables broken `argon` footer patch, and enables `iperf3`.
3. **MT7996 Phase B Controls & EHT320 Fixes (`9996`, `9997`)**: Adds live debugfs switches `sku_power_ctrl` / `bf_backoff_ctrl`, fixes `mt7996_mcu_sta_bfer_eht()` 320 MHz sounding dim shift (`<< 2`), and raises `max_amsdu_num` from `8` to `31`.
4. **MT7996 Phase G PCIe0 NPU Map & EHT320 BF/HE/RC Fixes (`9998`, `9999`)**: Adds runtime `npu_tx_band_map` (`0=stock`, `1=6G->PCIe0`, `2=5G+6G->PCIe0` Gen3 x2 `15.752 Gb/s`), fixes `mt7996_mcu_sta_bfer_eht()` BW320→BW160 fallthrough + clamping + `ng16`/`codebook`, adds `BW320` case in `mt7996_mcu_sta_he_tlv()`, syncs `STA_REC_BF` in `mt7996_mcu_add_rate_ctrl()`, and logs concise `STA_REC_BF` diagnostics.

### Flashing & Pure-AP Setup Guide

#### 1. Fresh Install Defaults (`sysupgrade -n` / Clean Flash)

| Item | Default Value | Notes |
| :--- | :--- | :--- |
| **Management Address** | `http://192.168.50.1` | Static `192.168.50.1/24` (netmask `255.255.255.0`), hostname `XR1710G`, SSH `:22` |
| **Username / Password** | `root` / *(empty)* | Blank password works for both LuCI and SSH on first boot; set a password immediately |
| **Default Port Layout** | LAN: `lan2`, `lan3`, `lan4` (`br-lan`)<br>WAN: `wan` (DHCP client) | Firewall rejects WAN input by default; connect via `lan2`/`lan3`/`lan4` or default Wi-Fi first |
| **DHCP Server** | **Removed (`dnsmasq`/`odhcpd` omitted)** | Clients will **not** receive an IP automatically on first boot; set a static IP (`192.168.50.x/24`) on your PC/phone |
| **Default Wi-Fi (Country `US`)** | 2.4 GHz: **`ImmortalWrt-2G`** (`WPA2-PSK`, `HE20`, auto ch, `28 dBm`)<br>5 GHz: **`ImmortalWrt-5G`** (`WPA2/WPA3`, `EHT160`, ch `36`, `30 dBm`)<br>6 GHz: **`ImmortalWrt-6G`** (`WPA3-SAE`, `EHT320`, ch `37`, `30 dBm`, TX `eBF=0`) | **Default Wi-Fi password for all 3 bands: `12345678`**; change SSIDs and passphrases after login |

> **First-Boot Connection Tip**: Because `dnsmasq` and `odhcpd` are removed by design, assign a static IP such as `192.168.50.10` / `255.255.255.0` on your PC or phone, connect to `lan2`/`lan3`/`lan4` (or one of the default Wi-Fi networks above), and open `http://192.168.50.1` or `ssh root@192.168.50.1`.

#### 2. Configuring as a 4-Port L2 Pure AP

1. **Set a root password** (`System → Administration` in LuCI or `passwd` over SSH).
2. **Bridge all 4 Ethernet ports (`lan2`, `lan3`, `lan4`, `wan`) into `br-lan`** and remove the standalone `wan`/`wan6` interfaces so both 10GbE ports and both 1GbE ports act as a single L2 switch/AP bridge.
3. **Move `lan` onto your main router's subnet** (static IP + gateway + DNS pointing to your main router, or `proto='dhcp'`), then update your Wi-Fi SSIDs and passwords (keep 6 GHz `radio2` TX Beamformer disabled to avoid overnight EHT Sounding stalls).

Example SSH one-shot setup (main router `192.168.3.1`, AP management IP `192.168.3.4/24`):
```sh
passwd
uci -q del_list network.@device[0].ports='wan'
uci add_list network.@device[0].ports='wan'
uci -q delete network.wan
uci -q delete network.wan6
uci set network.lan.proto='static'
uci -q delete network.lan.ipaddr
uci add_list network.lan.ipaddr='192.168.3.4/24'
uci -q delete network.lan.netmask
uci set network.lan.gateway='192.168.3.1'
uci -q delete network.lan.dns
uci add_list network.lan.dns='192.168.3.1'
uci commit network
/etc/init.d/network restart
```
Then revert your PC back to DHCP and plug either 10GbE port (`wan` or `lan2`) into your upstream router/switch. In **Phase J v2**, `npu_tx_band_map=2`, 4-core GICv3 IRQ affinity (`/etc/rc.local`), and 6 GHz `eBF=0` are baked in and activate automatically on cold boot.

#### 3. Partition Compatibility & Cross-Flashing

`naoki66`, this Pure-AP baseline (`gemtek,xr1710g`), `YYH` (`gemtek,xr1710g-ubi`), and `xmiqz` LEDE share the identical SPI-NAND UBI 2.0 layout (`ubi` at `0x00700000`, size `0x1b700000`, 439 MiB). When switching across variants with a different `board_name`, flash cleanly with:

```sh
sysupgrade -F -v -n /tmp/immortalwrt-nijigencg-20261126-pure-ap-phasej-v2-airoha-an7581-gemtek_xr1710g-squashfs-sysupgrade.itb
```

After reboot, follow steps 1 and 2 above to connect via static IP `192.168.50.x/24` and complete Pure-AP onboarding.

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
