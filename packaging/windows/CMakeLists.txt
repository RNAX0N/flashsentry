# Windows MSI + Burn EXE installers (WiX Toolset v3.14+)
cmake_minimum_required(VERSION 3.20)

if(NOT WIN32)
    return()
endif()

option(FLASHSPARTAN_BUILD_WINDOWS_INSTALLERS
       "Build FlashSpartan.msi and FlashSpartan-setup.exe (requires WiX Toolset)"
       OFF)

if(NOT FLASHSPARTAN_BUILD_WINDOWS_INSTALLERS)
    return()
endif()

# USBPcap is not bundled in installers (install manually for packet capture).
# This cache remains for optional redist packaging experiments only.
set(FLASHSPARTAN_USBPCAP_INSTALLER "" CACHE FILEPATH
    "Optional path to USBPcapSetup.exe (not used by default WiX targets)")

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

set(FLASHSPARTAN_STAGE_DIR "${CMAKE_BINARY_DIR}/stage" CACHE PATH "Windows install staging root")
set(FLASHSPARTAN_WIX_DIR "${CMAKE_BINARY_DIR}/wix")
set(FLASHSPARTAN_WIX_OUT "${CMAKE_BINARY_DIR}/wix-out")

file(MAKE_DIRECTORY "${FLASHSPARTAN_WIX_DIR}")
file(MAKE_DIRECTORY "${FLASHSPARTAN_WIX_OUT}")

if(NOT DEFINED PROJECT_VERSION OR PROJECT_VERSION STREQUAL "")
    set(_flashspartan_version "${FLASHSPARTAN_PROJECT_VERSION}")
else()
    set(_flashspartan_version "${PROJECT_VERSION}")
endif()
set(FLASHSPARTAN_PROJECT_VERSION "${_flashspartan_version}")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/wix/Variables.wxi.in"
    "${FLASHSPARTAN_WIX_DIR}/Variables.wxi"
    @ONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/wix/Product.wxs.in"
    "${FLASHSPARTAN_WIX_DIR}/Product.wxs"
    @ONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/wix/Bundle.wxs.in"
    "${FLASHSPARTAN_WIX_DIR}/Bundle.wxs"
    @ONLY
)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/wix/Variables.wxi.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/wix/Product.wxs.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/wix/Bundle.wxs.in"
)

file(TO_NATIVE_PATH "${FLASHSPARTAN_STAGE_DIR}/bin" _stage_bin_native)
file(TO_NATIVE_PATH "${FLASHSPARTAN_STAGE_DIR}/share" _stage_share_native)
file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/wix/License.rtf" _license_rtf_native)

set(_heat_bin_wxs "${FLASHSPARTAN_WIX_DIR}/HeatBin.wxs")
set(_heat_share_wxs "${FLASHSPARTAN_WIX_DIR}/HeatShare.wxs")
set(_product_wixobj "${FLASHSPARTAN_WIX_OUT}/Product.wixobj")
set(_heat_bin_wixobj "${FLASHSPARTAN_WIX_OUT}/HeatBin.wixobj")
set(_heat_share_wixobj "${FLASHSPARTAN_WIX_OUT}/HeatShare.wixobj")
set(_msi_output "${FLASHSPARTAN_WIX_OUT}/FlashSpartan-${_flashspartan_version}-x64.msi")
set(_bundle_wixobj "${FLASHSPARTAN_WIX_OUT}/Bundle.wixobj")
set(_exe_output "${FLASHSPARTAN_WIX_OUT}/FlashSpartan-${_flashspartan_version}-x64-setup.exe")

# Stage install tree + windeployqt (Release)
set(_stage_script "${CMAKE_CURRENT_BINARY_DIR}/stage-windows.cmd")
file(WRITE "${_stage_script}" "@echo off\r\n")
file(APPEND "${_stage_script}" "\"${CMAKE_COMMAND}\" -E rm -rf \"${FLASHSPARTAN_STAGE_DIR}\"\r\n")
file(APPEND "${_stage_script}"
    "\"${CMAKE_COMMAND}\" --install \"${CMAKE_BINARY_DIR}\" --config Release --prefix \"${FLASHSPARTAN_STAGE_DIR}\"\r\n")
file(APPEND "${_stage_script}"
    "\"${CMAKE_COMMAND}\" -E make_directory \"${FLASHSPARTAN_STAGE_DIR}/share\"\r\n")
if(WINDEPLOYQT_EXECUTABLE)
    file(APPEND "${_stage_script}"
        "\"${WINDEPLOYQT_EXECUTABLE}\" --release --compiler-runtime --no-translations \"${FLASHSPARTAN_STAGE_DIR}/bin/FlashSpartan.exe\"\r\n")
endif()
if(DEFINED OPENSSL_ROOT_DIR)
    file(GLOB _openssl_crypto "${OPENSSL_ROOT_DIR}/bin/libcrypto*.dll")
    file(GLOB _openssl_ssl "${OPENSSL_ROOT_DIR}/bin/libssl*.dll")
    foreach(_dll ${_openssl_crypto} ${_openssl_ssl})
        file(APPEND "${_stage_script}"
            "\"${CMAKE_COMMAND}\" -E copy_if_different \"${_dll}\" \"${FLASHSPARTAN_STAGE_DIR}/bin/\"\r\n")
    endforeach()
endif()

add_custom_target(flashspartan-stage-windows
    COMMAND "${_stage_script}"
    DEPENDS flashspartan flashspartan-policyd flashspartan-read-helper
    COMMENT "Staging FlashSpartan install tree for WiX"
    VERBATIM
)

add_custom_command(
    OUTPUT "${_heat_bin_wxs}"
    COMMAND "${WIX_HEAT}" dir "${FLASHSPARTAN_STAGE_DIR}/bin"
            -cg FlashSpartanBinFiles
            -dr INSTALLFOLDER
            -gg -scom -sreg -sfrag -srd
            -var var.StageBinDir
            -out "${_heat_bin_wxs}"
    DEPENDS flashspartan-stage-windows
    COMMENT "WiX heat: harvest bin/"
    VERBATIM
)

add_custom_command(
    OUTPUT "${_heat_share_wxs}"
    COMMAND "${WIX_HEAT}" dir "${FLASHSPARTAN_STAGE_DIR}/share"
            -cg FlashSpartanShareFiles
            -dr SHAREFOLDER
            -gg -scom -sreg -sfrag -srd
            -var var.StageShareDir
            -out "${_heat_share_wxs}"
    DEPENDS flashspartan-stage-windows
    COMMENT "WiX heat: harvest share/"
    VERBATIM
)

set(_candle_common
    -nologo
    -arch x64
    -dStageBinDir=${_stage_bin_native}
    -dStageShareDir=${_stage_share_native}
    -dLicenseRtf=${_license_rtf_native}
)

add_custom_command(
    OUTPUT "${_heat_bin_wixobj}"
    COMMAND "${WIX_CANDLE}" ${_candle_common} -out "${_heat_bin_wixobj}" "${_heat_bin_wxs}"
    DEPENDS "${_heat_bin_wxs}" flashspartan-stage-windows
    COMMENT "WiX candle: HeatBin"
    VERBATIM
)

add_custom_command(
    OUTPUT "${_heat_share_wixobj}"
    COMMAND "${WIX_CANDLE}" ${_candle_common} -out "${_heat_share_wixobj}" "${_heat_share_wxs}"
    DEPENDS "${_heat_share_wxs}" flashspartan-stage-windows
    COMMENT "WiX candle: HeatShare"
    VERBATIM
)

add_custom_command(
    OUTPUT "${_product_wixobj}"
    COMMAND "${WIX_CANDLE}" ${_candle_common}
            ${WIX_UI_EXT}
            -out "${_product_wixobj}"
            "${FLASHSPARTAN_WIX_DIR}/Product.wxs"
    DEPENDS "${FLASHSPARTAN_WIX_DIR}/Product.wxs" "${FLASHSPARTAN_WIX_DIR}/Variables.wxi"
    COMMENT "WiX candle: Product"
    VERBATIM
)

add_custom_command(
    OUTPUT "${_msi_output}"
    COMMAND "${WIX_LIGHT}" -nologo -ext WixUIExtension
            -out "${_msi_output}"
            "${_product_wixobj}" "${_heat_bin_wixobj}" "${_heat_share_wixobj}"
    DEPENDS "${_product_wixobj}" "${_heat_bin_wixobj}" "${_heat_share_wixobj}"
    COMMENT "WiX light: FlashSpartan.msi"
    VERBATIM
)

add_custom_target(flashspartan-msi DEPENDS "${_msi_output}")

if(WIX_LIT)
    file(TO_NATIVE_PATH "${_msi_output}" _msi_native)
    add_custom_command(
        OUTPUT "${_bundle_wixobj}"
        COMMAND "${WIX_CANDLE}" -nologo ${WIX_BAL_EXT}
                -dLicenseRtf=${_license_rtf_native}
                -dFlashSpartanMsi=${_msi_native}
                -out "${_bundle_wixobj}"
                "${FLASHSPARTAN_WIX_DIR}/Bundle.wxs"
        DEPENDS "${_msi_output}" "${FLASHSPARTAN_WIX_DIR}/Bundle.wxs"
        COMMENT "WiX candle: Bundle"
        VERBATIM
    )

    add_custom_command(
        OUTPUT "${_exe_output}"
        COMMAND "${WIX_LIGHT}" -nologo ${WIX_BAL_EXT}
                -out "${_exe_output}"
                "${_bundle_wixobj}"
        DEPENDS "${_bundle_wixobj}"
        COMMENT "WiX light: FlashSpartan-setup.exe"
        VERBATIM
    )

    add_custom_target(flashspartan-setup-exe DEPENDS "${_exe_output}")
    add_custom_target(flashspartan-windows-installers
        DEPENDS flashspartan-msi flashspartan-setup-exe)
else()
    add_custom_target(flashspartan-windows-installers DEPENDS flashspartan-msi)
endif()

message(STATUS "Windows installers: ${_msi_output}")
if(WIX_LIT)
    message(STATUS "  Setup EXE:      ${_exe_output}")
endif()
