add_library(lua STATIC IMPORTED GLOBAL)

set_property(TARGET lua PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/lua54.lib)

target_include_directories(lua
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/lua>
)