# ImmortalWrt for Gemtek XR1710G — Pure-AP Wi-Fi 7 Baseline

[中文说明](#中文说明) | [English](#english)

---

<a id="中文说明"></a>
## 中文说明

### 项目定位

本仓库基于 [naoki66/ImmortalWrt-for-Gemtek-brightspeed](https://github.com/naoki66/ImmortalWrt-for-Gemtek-brightspeed)（上游提交 `475b6b67ca` / Linux `6.18.60`），面向 **Gemtek XR1710G**（Airoha AN7581 SoC + MediaTek MT7996 三频 Wi-Fi 7 + 双 10GbE RTL8261BE）重构为**永久纯 AP（Pure-AP-Only）高吞吐无线桥接基线**：

- **保留完整无线与硬件转发能力**：
  - Airoha **NPU (`0.1111`)** + **PPE / PPE2 / L2B** 硬件加速与 `bridger` eBPF 桥接加速
  - `mt76` / `mt7996e` 三频 Wi-Fi 7（2.4G / 5G / 6G 320 MHz EHT）与 `luci-app-mlo`
  - `luci-base`（`luci-theme-bootstrap`）+ `uhttpd` + SSH（`dropbear`）+ `firewall4`（纯网桥最小防火墙）
  - 硬件管理：`luci-app-airoha-fancontrol`、`luci-app-airoha-npu`、`luci-app-airoha-recovery`、`luci-app-airoha-factory`
  - 诊断工具：`iperf3`、`ethtool-full`、`iw`、`tcpdump`、`i2c-tools`
- **永久精简路由器/代理冗余组件**：
  - 移除 `ppp*`、`dnsmasq*`、`odhcpd*`、`luci-theme-argon`、`ruby`、`kmod-nft-fullcone`、各类代理/穿透/DDNS/UPnP 插件等，避免与纯二层桥接 AP 角色冲突。

---

### 本基线包含的源码改动（相比上游 `475b6b67ca`）

1. **修复上游内核编译缺陷（`9999-16`）**
   - 文件：`target/linux/airoha/patches-6.18/9999-16-net-airoha-ppe-debugfs-forward-decl-flow-stats-available.patch`
   - 问题：上游 `9999-15` 补丁在 `drivers/net/ethernet/airoha/airoha_ppe_debugfs.c:172` 提前调用 `airoha_ppe_flow_stats_available()`，但其 `static` 定义位于第 234 行，GCC 14.4 报隐式声明/类型冲突。
   - 修复：在文件头部补充静态函数前置声明。

2. **纯 AP 配置与包依赖解耦（`1710-ap-only.config`）**
   - `package/emortal/default-settings/Makefile`：将 `+luci` 替换为 `+luci-base`，允许纯 AP 配置在不引入 `luci-light`/`luci-proto-ppp`/`ppp` 的前提下保留中文基础设置。
   - `package/network/config/firewall4/Makefile`：移除硬编码 `+kmod-nft-fullcone` 依赖。
   - 移除损坏的 `patches/feeds/argon/.../100-footer-show-model-version.patch`；新增可移植纯 AP 配置文件 `1710-ap-only.config`。

3. **MT7996 Phase B 驱动调试与 EHT320 修正（`9996` & `9997`）**
   - `package/kernel/mt76/patches/9996-wifi-mt76-mt7996-debugfs-txpower-sku-and-bf-backoff-ctrl.patch`：在 `/sys/kernel/debug/ieee80211/phy0/mt76/` 增加运行时动态开关 `sku_power_ctrl` 与 `bf_backoff_ctrl`（默认保持开启 `1`，支持免重启在线切换 SKU 限功与波束成形回退）。
   - `package/kernel/mt76/patches/9997-wifi-mt76-mt7996-fix-eht320-sounding-dim-and-amsdu-num.patch`：修正 `mt7996_mcu_sta_bfer_eht()` 中 EHT 320 MHz Sounding Dimension 计算（`CAP3_SOUNDING_DIM_320MHZ_MASK << 2` 替代错误的 `<< 1`），并将 `amsdu->max_amsdu_num` 从 `8` 提升至 `31`。

---

### 编译与验证要求

> **重要文件系统警告**：严禁在带私有 ACL 掩码的 NAS 目录（如会强制文件为 `0700` 的 Btrfs `trimacl` 挂载点）直接编译固件，否则生成的 `root.squashfs` 中 `/sbin/ubusd`、`/lib/libc.so` 与普通用户权限将无法执行，导致路由器无法启动。请务必在**标准 POSIX 权限文件系统**（如原生 `ext4` 或 Linux `ext4` 回环镜像）中克隆并构建。

```bash
# 1. 更新并安装 feeds
./scripts/feeds update -a
./scripts/feeds install -a
FEED_PATCHES_STRICT=1 bash scripts/select-feed-providers.sh
FEED_PATCHES_STRICT=1 bash scripts/apply-feed-patches.sh

# 2. 使用纯 AP 配置
cp 1710-ap-only.config .config
make defconfig

# 若构建机已安装 clang/llvm >= 15，可改用宿主 BPF 工具链以跳过漫长的 LLVM 源码编译：
# scripts/config --enable BPF_TOOLCHAIN_HOST --set-str BPF_TOOLCHAIN_HOST_PATH /path/to/llvm15 --enable USE_LLVM_HOST --disable BPF_TOOLCHAIN_BUILD_LLVM --disable USE_LLVM_BUILD
# make defconfig

# 3. 下载源码并编译
make download -j$(nproc)
make -j$(nproc) world
```

**刷机前必检门禁**：
- 使用 `unsquashfs4 -ll` 检查最终 `.itb` 内 `rootfs-1` 的系统目录（`/`、`/bin`、`/sbin`、`/lib`、`/etc`、`/usr`）与关键守护进程权限均为标准 POSIX（`0755`/`0644`，无 `0700`）。
- 建议先使用 QEMU `aarch64` 用户态冷启冒烟测试验证 `ubusd`、`netifd`、`br-lan`、`dropbear`、`uhttpd`、`fw4` 与 `bridger` 正常拉起，再在设备旁执行真机验证并预留 U-Boot 恢复手段。

---

### 鸣谢

- 上游固件与 Airoha AN7581 / XR1710G 基础工程：[naoki66/ImmortalWrt-for-Gemtek-brightspeed](https://github.com/naoki66/ImmortalWrt-for-Gemtek-brightspeed)
- [ImmortalWrt](https://github.com/immortalwrt/immortalwrt) 与 [OpenWrt](https://openwrt.org/) 社区

---

<a id="english"></a>
## English

### Overview

This repository establishes a clean **Pure-AP-Only Wi-Fi 7 baseline** for the **Gemtek XR1710G** (Airoha AN7581 SoC + MediaTek MT7996 tri-band Wi-Fi 7 + dual 10GbE RTL8261BE), derived from [naoki66/ImmortalWrt-for-Gemtek-brightspeed](https://github.com/naoki66/ImmortalWrt-for-Gemtek-brightspeed) (`475b6b67ca`, Linux `6.18.60`).

It is tailored strictly for high-throughput wired-to-6GHz L2 bridge AP operation:
- **Kept**: Airoha NPU (`0.1111`), PPE / PPE2 / L2B hardware offload, `bridger` eBPF bridge accelerator, MT7996 tri-band EHT320 wireless (`wpad-mesh-mbedtls`, `luci-app-mlo`), minimal `firewall4`, `luci-base` (`luci-theme-bootstrap`) + `uhttpd`, `dropbear` SSH, Airoha hardware/recovery/fancontrol LuCI apps, and diagnostics (`iperf3`, `ethtool-full`, `iw`, `tcpdump`, `i2c-tools`).
- **Removed permanently**: Router/WAN dialers and bloat (`ppp*`, `dnsmasq*`, `odhcpd*`, `kmod-nft-fullcone`, `luci-theme-argon`, `ruby`, proxy/VPN/DDNS/UPnP packages).

### Key Changes over Upstream `475b6b67ca`

1. **Fix `airoha_ppe_debugfs.c` build error (`9999-16`)**:
   - Adds missing forward declaration for `airoha_ppe_flow_stats_available()` in `drivers/net/ethernet/airoha/airoha_ppe_debugfs.c` introduced by upstream patch `9999-15`.
2. **Pure-AP Dependency Decoupling & Portable Seed (`1710-ap-only.config`)**:
   - `default-settings`: `+luci` -> `+luci-base` (keeps localization without pulling `luci-light` / `ppp`).
   - `firewall4`: drops mandatory `+kmod-nft-fullcone`.
   - Removes broken `argon` footer patch and provides portable `1710-ap-only.config`.
3. **MT7996 Phase B Runtime Controls & EHT320/AMSDU Fixes (`9996`, `9997`)**:
   - `9996`: Exposes runtime debugfs toggles `sku_power_ctrl` and `bf_backoff_ctrl` under `/sys/kernel/debug/ieee80211/phy0/mt76/` (defaulting to `1` on boot, switchable live without reboot).
   - `9997`: Fixes `mt7996_mcu_sta_bfer_eht()` 320 MHz sounding dimension bit shift (`<< 2` instead of `<< 1`) and raises `amsdu->max_amsdu_num` from `8` to `31`.

### Build & Pre-Flash Verification

> **POSIX Filesystem Requirement**: Always build on a standard POSIX filesystem (native `ext4` or an `ext4` loopback image). Never build directly on ACL-clamped filesystems (such as NAS Btrfs mounts with `trimacl` forcing `0700` modes), or system binaries (`ubusd`, `libc.so`, etc.) inside `root.squashfs` will lose world execute/read bits and fail to boot.

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

Before flashing `.itb` images, verify `rootfs-1` permissions (`0755`/`0644`, no `0700` system directories) via `unsquashfs4 -ll`, boot-test userspace in QEMU `aarch64`, and validate `/sbin/sysupgrade -T`.

### Credits

- Upstream Airoha AN7581 / XR1710G work: [naoki66/ImmortalWrt-for-Gemtek-brightspeed](https://github.com/naoki66/ImmortalWrt-for-Gemtek-brightspeed)
- [ImmortalWrt](https://github.com/immortalwrt/immortalwrt) & [OpenWrt](https://openwrt.org/)
