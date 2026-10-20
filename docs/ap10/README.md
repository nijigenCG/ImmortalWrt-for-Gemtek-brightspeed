# XR1710G AP10 + RFS 源码汇总

此分支将实际保留的 AP1–AP5、AP7–AP10 改动及已采用的 RFS 合并为一个提交，父提交为 naoki66 原库 `04cc62e8928308598cf2e88bddee37ede658b814`。没有合入已回退的 AP6；也没有更换 NPU 或 MT7996 内部镜像。

## 当前行为

- 普通 RX 使用相应缓冲区；保留硬件 LRO 生命周期和元数据修正，使用真实包流哈希。
- 保留实际工作量的 NAPI 完成、RX 描述符元数据缓存及耗尽后的 refill/中断恢复。
- 保留桥接 conntrack crossing、桥接 flowtable/PPE 与 DSCP/hop-count 语义；这些改动改变 conntrack 扩展，必须重编内核及配套模块，不能只换一个 `.ko`。
- QDMA RX 线程稳定命名；原生 steering 在 LAN2 RPS 为1时将 RX0 放到其余 CPU；RX0 使用普通 CFS nice -10。
- AN7581 内核 HZ1000、相邻 RX 元数据的对齐64位读取。
- 独立 `xr1710g-ap10-rfs` 包内保存已采用的原服务：global4096、LAN2全部32个RX队列各1024；不改原 RPS。服务随网络/防火墙/接口重载再次应用，并保留升级路径。

历史构建使用完整、已编译验证的 Airoha 驱动文件，因此本次保留该方式：删除被完整文件取代的原驱动补丁，只保留其中非驱动部分，避免重复应用；完整文件及 netfilter 输出 SHA 见清单。原有 coherent mailbox 等已有上游修复不重复算新增成果。

## 构建与适用范围

仅面向实测的 **XR1710G legacy 分区 profile `gemtek_xr1710g`**，不是 `gemtek_xr1710g-ubi` 或 XG2010G。保留 AP10 使用的硬件 DTS；不能据仓库型号名忽略实际分区/校准差异。

在 GNU/Linux 上按上游依赖准备 feeds 后：

```sh
./scripts/feeds update -a
./scripts/feeds install -a
cp 1710.config .config
make defconfig
make target/linux/prepare V=s
python3 scripts/check-ap10-source.py build_dir/target-*/linux-airoha_an7581/linux-6.18.55
make -j2 world V=s
```

本提交使用可移植的仓库工具链配置，不复制历史 `/work` 外部工具链路径、生产网络配置、校准数据或管理凭据。feeds 随时间可变；本提交不声称逐字节复现已部署 FIT。历史 AP10 内核/126模块及88配套 APK 已构建，但这个整合后的全树尚未重建或刷入。`check-ap10-source.py` 在 kernel prepare 后核验关键源码和 HZ，不能代替编译、ABI 和实际启动验证。

## 已测性能及界限

NAS RX1024恒定、相同连接条件的受控对照：AP→NAS4.8568→7.7115Gbps（+58.78%）；NAS→AP32流1.4540→1.4216Gbps（-2.23%）。用户接受综合收益，已采用 RFS。采用后 NAS 原RX256、8流各方向一次记录：AP→NAS7.7231、NAS→AP1.5878、OP→NAS2.8428、NAS→OP6.5717、AP→OP5.7859、OP→AP1.5425Gbps。AP→NAS该次重传89657且一次NAS ping未响应，其余方向探针无失败，最后服务恢复正常；这些单行不能计算新对照增幅。

AP4当轮局部约+10%、AP10特定32流 nice调整约+7.93%。早期端点与拓扑有变化，AP1至AP10不能累计百分比。没有三通路整体10G证据，也没有6GHz正常单播吞吐：缺少接收端。

## 后续研究候选

官方 MediaTek `mtk-openwrt-feeds` 提交 `e1d5a2d3f750976080b7b1559cabafe31322819f` 的 July21 WM/WA/DSP/ROMpatch 已离线核验，未部署；普通 wifi reload 不重装 MCU 镜像。NPU `0.1111` 是当前版本字符串解析的回退值；GET_COUNTER 固件回复592字节与主机逻辑256字节缓冲存在离线复现的容量缺口，未在生产调用，未证明业务吞吐根因。这些研究不进入本提交的活动固件/驱动。后续对照先比较 AP10 AP模式与 naoki66 原版路由模式，再恢复 AP10；需要新鲜备份、确切镜像与本地恢复方案。
