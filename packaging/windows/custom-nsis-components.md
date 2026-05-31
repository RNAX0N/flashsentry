# Optional: NSIS components page (checkbox)

CPack’s stock NSIS generator does not expose a per-component checkbox UI without a custom script. Two patterns:

## A. Yes/No during install (implemented in CMake)

When `FLASHSENTRY_USBPCAP_INSTALLER` is set, `CPACK_NSIS_EXTRA_INSTALL_COMMANDS` prompts and runs `USBPcapSetup.exe /S`. No PATH changes required.

## B. Full components page (custom NSIS)

1. Build the app tree with `cmake --install` into `stage/`.
2. Maintain `packaging/windows/FlashSentry-custom.nsi` with:

```nsis
!insertmacro MUI_PAGE_COMPONENTS
Section "!required" SecApp
  ; File /r stage\*.*
SectionEnd
Section "USBPcap" SecUsbPcap
  SectionIn 1   ; checked by default
  File /r USBPcapSetup.exe
  ExecWait '$INSTDIR\redist\USBPcapSetup.exe /S'
SectionEnd
```

3. Build with `makensis packaging/windows/FlashSentry-custom.nsi` instead of `cpack -G NSIS`.

## C. WiX Burn (.exe bootstrapper)

```xml
<Bundle>
  <BootstrapperApplicationRef Id="WixStandardBootstrapperApplication.HyperlinkLicense" />
  <Chain>
    <MsiPackage SourceFile="FlashSentry.msi" />
    <ExePackage SourceFile="USBPcapSetup.exe" InstallCondition="InstallUsbPcap"
               CommandLine="/S" PerMachine="yes" Vital="no" />
  </Chain>
</Bundle>
```

Add `<Checkbox Name="InstallUsbPcap" Checked="yes" />` in the BA manifest.

This is the usual approach for commercial `.exe` + optional driver redists.
