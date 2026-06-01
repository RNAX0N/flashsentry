# Windows MSI + Burn EXE installers (WiX Toolset v3.14+)
cmake_minimum_required(VERSION 3.20)

if(NOT WIN32)
    return()
endif()

option(FLASHSENTRY_BUILD_WINDOWS_INSTALLERS
       "Build FlashSentry.msi and FlashSentry-setup.exe (requires WiX Toolset)"
       OFF)

if(NOT FLASHSENTRY_BUILD_WINDOWS_INSTALLERS)
    return()
endif()

set(FLASHSENTRY_USBPCAP_INSTALLER "" CACHE FILEPATH
    "Path to official USBPcapSetup.exe (required for installer targets)")

if(NOT FLASHSENTRY_USBPCAP_INSTALLER OR NOT EXISTS "${FLASHSENTRY_USBPCAP_INSTALLER}")
    message(FATAL_ERROR
        "FLASHSENTRY_USBPCAP_INSTALLER must point to official USBPcapSetup.exe. "
        "See packaging/windows/third-party/README.md")
endif()

find_program(WIX_CANDLE candle)
find_program(WIX_LIGHT light)
find_program(WIX_HEAT heat)
find_program(WIX_LIT lit)

if(NOT WIX_CANDLE OR NOT WIX_LIGHT OR NOT WIX_HEAT)
    message(FATAL_ERROR
        "WiX Toolset v3 not found (candle, light, heat). Install from https://wixtoolset.org/ "
        "or: choco install wixtoolset")
endif()

if(NOT WIX_LIT)
    message(WARNING "lit.exe not found; Burn bundle (.exe) will not be built")
endif()

get_filename_component(_wix_bin "${WIX_CANDLE}" DIRECTORY)
set(WIX_EXT_DIR "${_wix_bin}")
set(WIX_UI_EXT "-ext" "WixUIExtension")
set(WIX_UTIL_EXT "-ext" "WixUtilExtension")
set(WIX_BAL_EXT "-ext" "WixBalExtension")

set(FLASHSENTRY_STAGE_DIR "${CMAKE_BINARY_DIR}/stage" CACHE PATH "Windows install staging root")
set(FLASHSENTRY_WIX_DIR "${CMAKE_BINARY_DIR}/wix")
set(FLASHSENTRY_WIX_OUT "${CMAKE_BINARY_DIR}/wix-out")

file(MAKE_DIRECTORY "${FLASHSENTRY_WIX_DIR}")
file(MAKE_DIRECTORY "${FLASHSENTRY_WIX_OUT}")

if(NOT DEFINED PROJECT_VERSION OR PROJECT_VERSION STREQUAL "")
    set(_flashsentry_version "${FLASHSENTRY_PROJECT_VERSION}")
else()
    set(_flashsentry_version "${PROJECT_VERSION}")
endif()
set(FLASHSENTRY_PROJECT_VERSION "${_flashsentry_version}")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/wix/Variables.wxi.in"
    "${FLASHSENTRY_WIX_DIR}/Variables.wxi"
    @ONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/wix/Product.wxs.in"
    "${FLASHSENTRY_WIX_DIR}/Product.wxs"
    @ONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/wix/Bundle.wxs.in"
    "${FLASHSENTRY_WIX_DIR}/Bundle.wxs"
    @ONLY
)

file(TO_NATIVE_PATH "${FLASHSENTRY_USBPCAP_INSTALLER}" _usbpcap_native)
file(TO_NATIVE_PATH "${FLASHSENTRY_STAGE_DIR}/bin" _stage_bin_native)
file(TO_NATIVE_PATH "${FLASHSENTRY_STAGE_DIR}/share" _stage_share_native)
file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/wix/License.rtf" _license_rtf_native)

set(_heat_bin_wxs "${FLASHSENTRY_WIX_DIR}/HeatBin.wxs")
set(_heat_share_wxs "${FLASHSENTRY_WIX_DIR}/HeatShare.wxs")
set(_product_wixobj "${FLASHSENTRY_WIX_OUT}/Product.wixobj")
set(_heat_bin_wixobj "${FLASHSENTRY_WIX_OUT}/HeatBin.wixobj")
set(_heat_share_wixobj "${FLASHSENTRY_WIX_OUT}/HeatShare.wixobj")
set(_msi_output "${FLASHSENTRY_WIX_OUT}/FlashSentry-${_flashsentry_version}-x64.msi")
set(_bundle_wixobj "${FLASHSENTRY_WIX_OUT}/Bundle.wixobj")
set(_exe_output "${FLASHSENTRY_WIX_OUT}/FlashSentry-${_flashsentry_version}-x64-setup.exe")

# Stage install tree + windeployqt (Release)
set(_stage_script "${CMAKE_CURRENT_BINARY_DIR}/stage-windows.cmd")
file(WRITE "${_stage_script}" "@echo off\r\n")
file(APPEND "${_stage_script}" "\"${CMAKE_COMMAND}\" -E rm -rf \"${FLASHSENTRY_STAGE_DIR}\"\r\n")
file(APPEND "${_stage_script}"
    "\"${CMAKE_COMMAND}\" --install \"${CMAKE_BINARY_DIR}\" --config Release --prefix \"${FLASHSENTRY_STAGE_DIR}\"\r\n")
file(APPEND "${_stage_script}"
    "\"${CMAKE_COMMAND}\" -E make_directory \"${FLASHSENTRY_STAGE_DIR}/share\"\r\n")
if(WINDEPLOYQT_EXECUTABLE)
    file(APPEND "${_stage_script}"
        "\"${WINDEPLOYQT_EXECUTABLE}\" --release --compiler-runtime --no-translations \"${FLASHSENTRY_STAGE_DIR}/bin/FlashSentry.exe\"\r\n")
endif()
if(DEFINED OPENSSL_ROOT_DIR)
    file(GLOB _openssl_crypto "${OPENSSL_ROOT_DIR}/bin/libcrypto*.dll")
    file(GLOB _openssl_ssl "${OPENSSL_ROOT_DIR}/bin/libssl*.dll")
    foreach(_dll ${_openssl_crypto} ${_openssl_ssl})
        file(APPEND "${_stage_script}"
            "\"${CMAKE_COMMAND}\" -E copy_if_different \"${_dll}\" \"${FLASHSENTRY_STAGE_DIR}/bin/\"\r\n")
    endforeach()
endif()

add_custom_target(flashsentry-stage-windows
    COMMAND "${_stage_script}"
    DEPENDS flashsentry flashsentry-policyd flashsentry-read-helper
    COMMENT "Staging FlashSentry install tree for WiX"
    VERBATIM
)

add_custom_command(
    OUTPUT "${_heat_bin_wxs}"
    COMMAND "${WIX_HEAT}" dir "${FLASHSENTRY_STAGE_DIR}/bin"
            -cg FlashSentryBinFiles
            -dr INSTALLFOLDER
            -gg -scom -sreg -sfrag -srd
            -xr "redist"
            -var var.StageBinDir
            -out "${_heat_bin_wxs}"
    DEPENDS flashsentry-stage-windows
    COMMENT "WiX heat: harvest bin/"
    VERBATIM
)

add_custom_command(
    OUTPUT "${_heat_share_wxs}"
    COMMAND "${WIX_HEAT}" dir "${FLASHSENTRY_STAGE_DIR}/share"
            -cg FlashSentryShareFiles
            -dr SHAREFOLDER
            -gg -scom -sreg -sfrag -srd
            -var var.StageShareDir
            -out "${_heat_share_wxs}"
    DEPENDS flashsentry-stage-windows
    COMMENT "WiX heat: harvest share/"
    VERBATIM
)

set(_candle_common
    -nologo
    -arch x64
    -dStageBinDir=${_stage_bin_native}
    -dStageShareDir=${_stage_share_native}
    -dLicenseRtf=${_license_rtf_native}
    -dUsbPcapInstaller=${_usbpcap_native}
)

add_custom_command(
    OUTPUT "${_heat_bin_wixobj}"
    COMMAND "${WIX_CANDLE}" ${_candle_common} -out "${_heat_bin_wixobj}" "${_heat_bin_wxs}"
    DEPENDS "${_heat_bin_wxs}" flashsentry-stage-windows
    COMMENT "WiX candle: HeatBin"
    VERBATIM
)

add_custom_command(
    OUTPUT "${_heat_share_wixobj}"
    COMMAND "${WIX_CANDLE}" ${_candle_common} -out "${_heat_share_wixobj}" "${_heat_share_wxs}"
    DEPENDS "${_heat_share_wxs}" flashsentry-stage-windows
    COMMENT "WiX candle: HeatShare"
    VERBATIM
)

add_custom_command(
    OUTPUT "${_product_wixobj}"
    COMMAND "${WIX_CANDLE}" ${_candle_common}
            ${WIX_UI_EXT}
            -out "${_product_wixobj}"
            "${FLASHSENTRY_WIX_DIR}/Product.wxs"
    DEPENDS "${FLASHSENTRY_WIX_DIR}/Product.wxs" "${FLASHSENTRY_WIX_DIR}/Variables.wxi"
    COMMENT "WiX candle: Product"
    VERBATIM
)

add_custom_command(
    OUTPUT "${_msi_output}"
    COMMAND "${WIX_LIGHT}" -nologo -ext WixUIExtension
            -out "${_msi_output}"
            "${_product_wixobj}" "${_heat_bin_wixobj}" "${_heat_share_wixobj}"
    DEPENDS "${_product_wixobj}" "${_heat_bin_wixobj}" "${_heat_share_wixobj}"
    COMMENT "WiX light: FlashSentry.msi"
    VERBATIM
)

add_custom_target(flashsentry-msi DEPENDS "${_msi_output}")

if(WIX_LIT)
    file(TO_NATIVE_PATH "${_msi_output}" _msi_native)
    add_custom_command(
        OUTPUT "${_bundle_wixobj}"
        COMMAND "${WIX_CANDLE}" -nologo ${WIX_BAL_EXT} ${WIX_UTIL_EXT}
                -dStageBinDir=${_stage_bin_native}
                -dLicenseRtf=${_license_rtf_native}
                -dUsbPcapInstaller=${_usbpcap_native}
                -dFlashSentryMsi=${_msi_native}
                -out "${_bundle_wixobj}"
                "${FLASHSENTRY_WIX_DIR}/Bundle.wxs"
        DEPENDS "${_msi_output}" "${FLASHSENTRY_WIX_DIR}/Bundle.wxs"
        COMMENT "WiX candle: Bundle"
        VERBATIM
    )

    add_custom_command(
        OUTPUT "${_exe_output}"
        COMMAND "${WIX_LIGHT}" -nologo ${WIX_BAL_EXT} ${WIX_UTIL_EXT}
                -out "${_exe_output}"
                "${_bundle_wixobj}"
        DEPENDS "${_bundle_wixobj}"
        COMMENT "WiX light: FlashSentry-setup.exe"
        VERBATIM
    )

    add_custom_target(flashsentry-setup-exe DEPENDS "${_exe_output}")
    add_custom_target(flashsentry-windows-installers
        DEPENDS flashsentry-msi flashsentry-setup-exe)
else()
    add_custom_target(flashsentry-windows-installers DEPENDS flashsentry-msi)
endif()

message(STATUS "Windows installers: ${_msi_output}")
if(WIX_LIT)
    message(STATUS "  Setup EXE:      ${_exe_output}")
endif()
