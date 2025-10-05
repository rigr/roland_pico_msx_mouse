# File: pico_sdk_import.cmake
# Diese Datei sorgt dafür, dass CMake das Pico SDK findet
# File: pico_sdk_import.cmake
if (NOT PICO_SDK_PATH)
  set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/pico-sdk")
endif()

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

