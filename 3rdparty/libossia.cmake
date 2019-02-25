
# OSSIA API-specific configuration
if(SCORE_SANITIZE)
  set(OSSIA_SANITIZE True CACHE INTERNAL "" FORCE)
endif()
if(SCORE_SPLIT_DEBUG)
  set(OSSIA_SPLIT_DEBUG True CACHE INTERNAL "" FORCE)
else()
  set(OSSIA_SPLIT_DEBUG False CACHE INTERNAL "" FORCE)
endif()
if(SCORE_COTIRE)
  set(OSSIA_DISABLE_COTIRE False CACHE INTERNAL "" FORCE)
else()
  set(OSSIA_DISABLE_COTIRE True CACHE INTERNAL "" FORCE)
endif()
if(SCORE_STATIC_PLUGINS)
  set(OSSIA_STATIC ON CACHE INTERNAL "" FORCE)
endif()
if(SCORE_ENABLE_LTO)
  set(OSSIA_LTO ON CACHE INTERNAL "" FORCE)
endif()
set(OSSIA_NO_INSTALL OFF CACHE INTERNAL "" FORCE)
set(OSSIA_PD OFF CACHE INTERNAL "" FORCE)
set(OSSIA_JAVA OFF CACHE INTERNAL "" FORCE)
set(OSSIA_OSX_FAT_LIBRARIES OFF CACHE INTERNAL "" FORCE)
set(OSSIA_PYTHON OFF CACHE INTERNAL "" FORCE)
set(OSSIA_QT ON CACHE INTERNAL "" FORCE)
set(OSSIA_QML_DEVICE OFF CACHE INTERNAL "" FORCE)
set(OSSIA_DISABLE_QT_PLUGIN ON CACHE INTERNAL "" FORCE)
set(OSSIA_PROTOCOL_HTTP ON CACHE INTERNAL "" FORCE)
set(OSSIA_PROTOCOL_WEBSOCKETS ON CACHE INTERNAL "" FORCE)
set(OSSIA_PROTOCOL_SERIAL ON CACHE INTERNAL "" FORCE)
set(OSSIA_PROTOCOL_ARTNET ON CACHE INTERNAL "" FORCE)
set(OSSIA_PROTOCOL_JOYSTICK OFF CACHE INTERNAL "" FORCE)
set(OSSIA_PROTOCOL_WIIMOTE OFF CACHE INTERNAL "" FORCE)
set(RTMIDI17_NO_WINUWP ON CACHE INTERNAL "" FORCE)

if(NOT EXISTS ".git")
  set(OSSIA_SUBMODULE_AUTOUPDATE OFF CACHE INTERNAL "" FORCE)
endif()

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libossia")