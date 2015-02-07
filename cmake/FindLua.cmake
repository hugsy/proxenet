find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
  pkg_search_module(LUA lua5.2 lua-5.2 lua52 lua5.1 lua-5.1 lua51 lua-5.0 lua5.0 lua50 lua)
endif()