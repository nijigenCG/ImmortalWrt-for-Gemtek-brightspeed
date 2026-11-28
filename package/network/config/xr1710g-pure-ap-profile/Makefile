include $(TOPDIR)/rules.mk

PKG_NAME:=xr1710g-pure-ap-profile
PKG_VERSION:=1
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/xr1710g-pure-ap-profile
  SECTION:=net
  CATEGORY:=Network
  TITLE:=XR1710G Pure-AP profile marker
  DEPENDS:=@TARGET_airoha_an7581 +luci-app-airoha-npu +bridger
endef

define Build/Compile
endef

define Package/xr1710g-pure-ap-profile/install
	$(INSTALL_DIR) $(1)/usr/share/airoha
	touch $(1)/usr/share/airoha/xr1710g-pure-ap
endef

$(eval $(call BuildPackage,xr1710g-pure-ap-profile))
