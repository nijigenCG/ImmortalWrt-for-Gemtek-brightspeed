# SPDX-License-Identifier: GPL-2.0-only
include $(TOPDIR)/rules.mk

PKG_NAME:=xr1710g-ap10-rfs
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0-only

include $(INCLUDE_DIR)/package.mk

define Package/xr1710g-ap10-rfs
  SECTION:=net
  CATEGORY:=Network
  TITLE:=XR1710G AP10 tested LAN2 software RFS policy
  DEPENDS:=@TARGET_airoha_an7581
endef

define Package/xr1710g-ap10-rfs/description
  Apply the tested 4096 global / 1024 per LAN2 RX queue RFS policy.
  Preserve the native RPS masks and NAPI scheduling policy.
endef

define Build/Compile
endef

define Package/xr1710g-ap10-rfs/install
	$(INSTALL_DIR) $(1)/etc/init.d $(1)/etc/uci-defaults $(1)/lib/upgrade/keep.d
	$(INSTALL_BIN) ./files/codex_rfs $(1)/etc/init.d/codex_rfs
	$(INSTALL_BIN) ./files/99-ap10-rfs $(1)/etc/uci-defaults/99-ap10-rfs
	$(INSTALL_DATA) ./files/keep $(1)/lib/upgrade/keep.d/xr1710g-ap10-rfs
	$(INSTALL_DATA) ./files/ap-driver-build-id $(1)/etc/ap-driver-build-id
endef

$(eval $(call BuildPackage,xr1710g-ap10-rfs))
