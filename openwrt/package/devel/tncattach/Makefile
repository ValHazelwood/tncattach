#
# This software is licensed under the Public Domain.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=tncattach
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_MAINTAINER:=John Doe <john.doe@example.com>
PKG_LICENSE:=CC0-1.0

include $(INCLUDE_DIR)/package.mk

define Package/tncattach
	SECTION:=devel
	# Select package by default
	#DEFAULT:=y
	CATEGORY:=Development
	TITLE:=tncattach application.
	URL:=https://www.example.com
	#DEPENDS:=argp-standalone
endef

define Package/tncattach/description
	Some example Programm called tncattach
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	CFLAGS="$(TARGET_CFLAGS)" CPPFLAGS="$(TARGET_CPPFLAGS)" $(MAKE) -C $(PKG_BUILD_DIR) $(TARGET_CONFIGURE_OPTS)
endef

define Package/tncattach/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/tncattach $(1)/usr/sbin/
endef

$(eval $(call BuildPackage,tncattach))
