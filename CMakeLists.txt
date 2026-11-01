cmake_minimum_required(VERSION 3.24)
project(X21_CHAT VERSION 1.0.0 LANGUAGES CXX)
set(X21_APP_VERSION "${PROJECT_VERSION}" CACHE STRING "Version embedded in the app")

if(NOT WIN32 OR NOT MSVC)
  message(FATAL_ERROR "X21 CHAT requires Windows and the Visual Studio C++ toolchain.")
endif()

set(KA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/keyauth-1.3-vendor")
add_executable(X21_CHAT WIN32 main.cpp tweaks.cpp "${KA_ROOT}/auth.cpp" "${KA_ROOT}/utils.cpp" "${KA_ROOT}/QRCode/qr.png.cpp" "${KA_ROOT}/QRCode/qrcode.cpp" "${KA_ROOT}/QRCode/pingout.cpp")
set_target_properties(X21_CHAT PROPERTIES OUTPUT_NAME "X21_CHEATS_GITHUB")
target_compile_features(X21_CHAT PRIVATE cxx_std_17)
target_compile_definitions(X21_CHAT PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX SODIUM_STATIC)
target_compile_definitions(X21_CHAT PRIVATE X21_APP_VERSION="${X21_APP_VERSION}" X21_UPDATE_OWNER="orhan-31" X21_UPDATE_REPO="x21-cheats-updates")
target_compile_options(X21_CHAT PRIVATE /utf-8)
target_include_directories(X21_CHAT PRIVATE "${KA_ROOT}" "${KA_ROOT}/curl" "${KA_ROOT}/libsodium" "${KA_ROOT}/nlohmann" "${KA_ROOT}/QRCode")
target_link_libraries(X21_CHAT PRIVATE
  "${KA_ROOT}/curl/libcurl.lib" "${KA_ROOT}/curl/zlib.lib" "${KA_ROOT}/libsodium/libsodium.lib"
  user32 gdi32 gdiplus advapi32 powrprof dnsapi ws2_32 wldap32 crypt32 normaliz winhttp shell32 shlwapi rpcrt4 httpapi psapi wintrust)
target_link_options(X21_CHAT PRIVATE "/NODEFAULTLIB:LIBCMT" "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'")
set_property(TARGET X21_CHAT PROPERTY VS_DPI_AWARE "PerMonitor")
add_custom_command(TARGET X21_CHAT POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:X21_CHAT>/assets"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/assets/x21-theme.jpg" "$<TARGET_FILE_DIR:X21_CHAT>/assets/x21-theme.jpg")



