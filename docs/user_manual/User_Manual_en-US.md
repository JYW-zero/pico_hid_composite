# Zhidong HID Composite Device - User Manual

**Version**: v1.0  
**Applicable Firmware**: v4.0.0+  
**Last Updated**: 2026-08-20

---

## Table of Contents

1. [Quick Start](#1-quick-start)
2. [Device Overview](#2-device-overview)
3. [Software Installation](#3-software-installation)
4. [Key Customization](#4-key-customization)
5. [Macro Functions](#5-macro-functions)
6. [Mouse Settings](#6-mouse-settings)
7. [Joystick Settings](#7-joystick-settings)
8. [Encoder Settings](#8-encoder-settings)
9. [Configuration Management](#9-configuration-management)
10. [Firmware Update](#10-firmware-update)
11. [FAQ](#11-faq)
12. [Technical Specifications](#12-technical-specifications)

---

## 1. Quick Start

### 1.1 Package Contents

- [ ] Zhidong HID Composite Device × 1
- [ ] USB Type-C Cable × 1
- [ ] This User Manual × 1

> 📷 *[Image: Product package contents photo]*

### 1.2 Connecting the Device

1. Connect the device to your computer using the USB Type-C cable
2. Wait for the computer to automatically recognize the device (driver-free on Windows/macOS/Linux)
3. The device indicator light illuminates to indicate successful connection

> 📷 *[Image: Device connection diagram]*

### 1.3 Verify Connection

**Windows**:
- Open「Device Manager」→「Human Interface Devices」
- You should see「HID-compliant device」and「HID-compliant vendor-defined device」

**macOS**:
- Open「System Information」→「USB」
- You should see「Zhidong HID Composite Device」

**Linux**:
- Run `lsusb` in terminal
- You should see a device with VID 0x1209 / PID 0x4004

### 1.4 Install the Configuration Software

1. Visit the project GitHub repository to download the latest software
2. Windows users download `pc-tool-windows-self-contained.zip`
3. Extract and run `HidConfigTool.App.exe`
4. The software will automatically detect connected devices

> 📷 *[Image: Software main interface screenshot]*

---

## 2. Device Overview

### 2.1 Features

The Zhidong HID Composite Device is a highly configurable input device supporting:

- ✅ **Full Key Customization**: Every key can be mapped to any keyboard key, mouse button, media key, or macro
- ✅ **Multi-Layer Key Mapping**: Support for 4 key layers, switched via Fn key
- ✅ **Macro Recording**: Support key sequences, delays, loops, storable in Flash
- ✅ **Multi-DPI**: Support 4 preset DPI levels + custom DPI, one-click switching
- ✅ **Joystick Calibration**: Support deadzone adjustment, sensitivity curves, axis inversion
- ✅ **Encoder**: Support volume, scroll, zoom, brightness and other modes
- ✅ **Config Import/Export**: Configurations can be saved to files for backup and sharing
- ✅ **Firmware Update**: Online firmware update via USB DFU mode

> 📷 *[Image: Device feature diagram]*

### 2.2 Hardware Specifications

| Item | Specification |
|-----|--------------|
| MCU | RP2350 (Dual-core ARM Cortex-M33) |
| Keys | 64 keys (configurable) |
| Joystick | Dual-channel analog joystick |
| Encoder | Rotary encoder (with push button) |
| Mouse Sensor | Optical sensor (up to 26000 DPI) |
| Interface | USB Type-C |
| Compatible OS | Windows 10/11, macOS 12+, Linux |
| Driver | Driver-free (standard HID device) |

---

## 3. Software Installation

### 3.1 System Requirements

| OS | Minimum Version | Recommended Version |
|----|----------------|---------------------|
| Windows | Windows 10 1809 | Windows 11 |
| macOS | macOS 12 Monterey | macOS 14 Sonoma |
| Linux | Ubuntu 20.04 | Ubuntu 22.04+ |

### 3.2 Windows Installation

1. Download `pc-tool-windows-self-contained.zip` from GitHub Releases
2. Extract to any directory (e.g., `C:\Program Files\HidConfigTool`)
3. Right-click `HidConfigTool.App.exe` →「Send to」→「Desktop shortcut」
4. Double-click the shortcut to launch the software

> 💡 **Tip**: The self-contained version includes the .NET runtime, no additional installation required.

### 3.3 macOS Installation

1. Download `pc-tool-macos.zip` from GitHub Releases
2. Extract and drag `HidConfigTool.app` into the「Applications」folder
3. First run: right-click the app →「Open」→ Confirm opening
4. Subsequent launches can be done directly from Launchpad

### 3.4 Linux Installation

```bash
# Download and extract
wget https://github.com/JYW-zero/pico_hid_composite/releases/latest/download/pc-tool-linux.zip
unzip pc-tool-linux.zip -d HidConfigTool
cd HidConfigTool

# Add execute permission
chmod +x HidConfigTool

# Run
./HidConfigTool
```

> ⚠️ **Note**: Linux users may need to configure udev rules for USB device access.

### 3.5 Software Interface Overview

> 📷 *[Image: Software main interface annotated diagram]*

1. **Menu Bar**: File, Tools, Help
2. **Device Status Bar**: Shows currently connected device, firmware version, connection status
3. **Function Navigation Bar**: Key Settings, Mouse Settings, Joystick Settings, Encoder Settings, Macro Management, Config Management, Firmware Update
4. **Settings Area**: Detailed settings for the currently selected function
5. **Action Buttons**: Save Config, Restore Defaults, Import/Export

---

## 4. Key Customization

### 4.1 Access Key Settings

1. Connect the device and open the software
2. Click「Key Settings」in the left navigation bar
3. The interface displays the device's key layout

> 📷 *[Image: Key settings interface screenshot]*

### 4.2 Modify a Single Key

1. Click the key to modify in the key layout diagram
2. The key configuration panel appears on the right
3. Select the key function type:
   - **Keyboard Key**: Map to standard keyboard keys (e.g., A, B, Ctrl, Shift)
   - **Mouse Button**: Map to mouse left/right/middle button
   - **Media Key**: Map to play/pause, volume up/down, next track, etc.
   - **Macro**: Map to a recorded macro
   - **Layer Switch**: Map to switch to another key layer
   - **No Function**: Disable the key
4. Select the specific key value
5. Click「Apply」to save changes

### 4.3 Multi-Layer Key Mapping

The device supports 4 key mapping layers (Layer 0 - Layer 3):

- **Layer 0**: Default layer, normal use
- **Layer 1**: Fn layer, activated while holding Fn key
- **Layer 2**: Extended layer, switchable via key combination
- **Layer 3**: Gaming layer, optimized for gaming

**How to switch layers**:
1. Set a key to「Layer Switch」function
2. Select the target layer (Layer 1/2/3)
3. Select switch mode:
   - **Hold to switch**: Temporarily switch while holding, return on release
   - **Toggle switch**: Press once to lock to target layer, press again to return

### 4.4 Batch Modification

1. Click「Batch Edit」button
2. Box-select multiple keys (or Ctrl-click to select)
3. Set unified function in the right panel
4. Click「Apply to selected keys」

### 4.5 Restore Single Key Default

1. Right-click the key to restore
2. Select「Restore Default」
3. Confirm the operation

---

## 5. Macro Functions

### 5.1 Macro Introduction

A macro is a predefined sequence of key operations that can be triggered with one key press. Supports:

- Key press/release
- Delay waiting
- Loop execution
- Mouse clicks
- Media key operations

### 5.2 Record a New Macro

1. Click「Macro Management」in the left navigation bar
2. Click「New Macro」button
3. Enter macro name (e.g.,「Combo1」)
4. Click「Start Recording」
5. Press the key sequence to record on the device
6. Click「Stop Recording」
7. (Optional) Adjust delay for each action on the timeline
8. Click「Save」

> 📷 *[Image: Macro recording interface screenshot]*

### 5.3 Manually Edit a Macro

1. Select the macro to edit in the macro list
2. Click「Edit」button
3. In the action list:
   - **Add action**: Click「+」button, select action type
   - **Modify action**: Double-click an action row, modify parameters
   - **Delete action**: Select action, click「-」button
   - **Reorder**: Drag action rows up/down
4. Set loop count (1-999 times, or infinite loop)
5. Click「Save」

### 5.4 Macro Action Types

| Action Type | Description | Parameters |
|------------|-------------|------------|
| Key Press | Press specified key | Key code |
| Key Release | Release specified key | Key code |
| Key Click | Press and release | Key code, press duration |
| Mouse Press | Press mouse button | Mouse button (left/right/middle) |
| Mouse Release | Release mouse button | Mouse button |
| Mouse Click | Click mouse button | Mouse button, click duration |
| Delay | Wait specified time | Delay in milliseconds (1-10000ms) |
| Scroll | Scroll mouse wheel | Scroll direction, scroll amount |

### 5.5 Bind Macro to a Key

1. Go to「Key Settings」
2. Click the key to bind the macro
3. Select「Macro」as the function type
4. Select the macro from the dropdown list
5. Select trigger method:
   - **Press trigger**: Execute macro when key is pressed
   - **Release trigger**: Execute macro when key is released
   - **Toggle trigger**: Press once to start, press again to stop (for loop macros)
6. Click「Apply」

### 5.6 Manage Macros

- **Rename**: Right-click macro →「Rename」
- **Delete**: Right-click macro →「Delete」
- **Copy**: Right-click macro →「Copy」
- **Export**: Select macro →「Export」, save as `.macro` file
- **Import**: Click「Import」, select `.macro` file

### 5.7 Macro Usage Tips

1. **Gaming combos**: Record complex combos as macros for one-click execution
2. **Office shortcuts**: Record common shortcut combinations (e.g., Ctrl+C, Ctrl+V)
3. **Text input**: Use macros to quickly input common text (e.g., email addresses)
4. **Loop operations**: Set infinite loop macros for repetitive tasks (e.g., auto-click)

> ⚠️ **Note**: Using macros in online games may violate game rules. Use with caution.

---

## 6. Mouse Settings

### 6.1 DPI Settings

DPI (Dots Per Inch) determines mouse movement sensitivity. Higher DPI = faster cursor movement.

**Preset DPI Levels**:

| Level | DPI Value | Use Case |
|-------|-----------|----------|
| 1 | 400 | Precision work, FPS gaming |
| 2 | 800 | Daily office, general use |
| 3 | 1600 | High-resolution screens |
| 4 | 3200 | Fast movement, MOBA gaming |

**Modify DPI levels**:
1. Go to「Mouse Settings」
2. Click the level to modify in the「DPI Levels」area
3. Enter new DPI value (100-26000)
4. Click「Apply」

**Switch DPI**:
- **Key switch**: Set a key to「DPI Switch」, press to cycle through levels
- **Software switch**: Click corresponding level in mouse settings interface
- **Real-time adjust**: Drag DPI slider for real-time adjustment (temporary, not saved)

> 📷 *[Image: Mouse DPI settings interface screenshot]*

### 6.2 Custom DPI

If preset levels don't meet your needs, use custom DPI:

1. Select「Custom」in DPI settings
2. Enter specific DPI value (100-26000)
3. Click「Apply」
4. Device immediately uses the new DPI

> 💡 **Tip**: Test different DPI values in games or software first to find the one that suits you best.

### 6.3 Mouse Button Mapping

Keys on the device can be mapped to mouse functions:

1. Go to「Key Settings」
2. Click the key to modify
3. Select「Mouse Button」as function type
4. Select specific mouse button:
   - Mouse Left Button
   - Mouse Right Button
   - Mouse Middle Button
   - Mouse Side Button 1 (Back)
   - Mouse Side Button 2 (Forward)
5. Click「Apply」

### 6.4 Scroll Wheel Settings

1. Go to「Mouse Settings」→「Scroll Wheel」
2. Set scroll direction:
   - **Normal**: Scroll up = page up
   - **Inverted**: Scroll up = page down
3. Set scroll speed (1-10 levels)
4. Click「Apply」

---

## 7. Joystick Settings

### 7.1 Joystick Calibration

It's recommended to calibrate the joystick after first use or device replacement:

1. Go to「Joystick Settings」
2. Click「Start Calibration」
3. Follow the prompts:
   - Step 1: Release joystick, wait 2 seconds (calibrate center point)
   - Step 2: Push joystick to four extreme positions (up, down, left, right)
   - Step 3: Release joystick, wait for calibration to complete
4. After calibration, the interface displays calibration results

> 📷 *[Image: Joystick calibration interface screenshot]*

### 7.2 Deadzone Settings

Deadzone is the area near the joystick center where input is ignored. Increasing deadzone prevents drift but reduces sensitivity.

1. Go to「Joystick Settings」→「Deadzone」
2. Drag X-axis deadzone slider (0-4095)
3. Drag Y-axis deadzone slider (0-4095)
4. Observe joystick status graph in real-time, deadzone area shown in gray
5. Click「Apply」to save

> 💡 **Recommendations**:
> - Daily use: Set deadzone to 5-10%
> - Gaming (precision): Set deadzone to 2-5%
> - Joystick drift: Increase deadzone appropriately

### 7.3 Sensitivity Curve

The sensitivity curve determines the mapping from joystick input to cursor output:

1. Go to「Joystick Settings」→「Sensitivity」
2. Select curve type:
   - **Linear**: Input proportional to output (default)
   - **Exponential**: Slow at small input, fast at large input (suitable for gaming)
   - **Logarithmic**: Fast at small input, slow at large input (suitable for precision)
   - **Custom**: Manually adjust curve parameters
3. Drag sensitivity slider (50%-200%)
4. Observe curve preview in real-time
5. Click「Apply」

### 7.4 Axis Inversion

1. Go to「Joystick Settings」→「Direction」
2. Check「Invert X-axis」for left-right inversion
3. Check「Invert Y-axis」for up-down inversion
4. Click「Apply」

> 💡 **Tip**: Flight simulation games usually require Y-axis inversion (push/pull controls pitch).

### 7.5 Joystick Mode

The joystick can work in different modes:

1. Go to「Joystick Settings」→「Mode」
2. Select work mode:
   - **Mouse Mode**: Joystick controls mouse cursor movement
   - **Gamepad Mode**: Joystick acts as gamepad left stick
   - **Keyboard Mode**: Joystick maps to direction keys (WASD or arrow keys)
   - **Scroll Mode**: Joystick controls mouse wheel
3. Click「Apply」

---

## 8. Encoder Settings

### 8.1 Encoder Modes

The rotary encoder supports multiple work modes:

| Mode | Function | Use Case |
|------|----------|----------|
| Volume | Rotate to adjust system volume, press to mute | Daily use |
| Scroll | Rotate to control page scroll, press for middle click | Browsing web/documents |
| Zoom | Rotate to control zoom, press to reset | Design/drawing |
| Brightness | Rotate to adjust screen brightness | Laptop |
| Custom | Rotate maps to custom keys | Advanced users |

**Switch modes**:
1. Go to「Encoder Settings」
2. Select target mode from the「Mode」dropdown list
3. Click「Apply」
4. Encoder immediately switches to new mode

> 📷 *[Image: Encoder settings interface screenshot]*

### 8.2 Rotation Acceleration

When rotation acceleration is enabled, fast rotation produces larger step values:

1. Go to「Encoder Settings」→「Acceleration」
2. Check「Enable rotation acceleration」
3. Set acceleration parameters:
   - **Base step**: Step value per detent at slow rotation (1-10)
   - **Max step**: Maximum step value per detent at fast rotation (1-100)
   - **Acceleration curve**: Mapping curve from rotation speed to step value
4. Click「Apply」

> 💡 **Recommendations**:
> - Volume mode: Base step 2, max step 10
> - Scroll mode: Base step 3, max step 20
> - Zoom mode: Base step 1, max step 5

### 8.3 Encoder Button

The encoder push button can trigger different functions:

1. Go to「Encoder Settings」→「Button」
2. Select press function:
   - **Mode default**: Use default button function of current mode
   - **Mouse middle button**: Press triggers mouse middle button
   - **Play/Pause**: Press triggers media play/pause
   - **Custom key**: Press triggers any keyboard key
   - **Switch mode**: Press to cycle encoder modes
3. Click「Apply」

### 8.4 Debounce Settings

If the encoder has false triggers or jitter, you can adjust debounce parameters:

1. Go to「Encoder Settings」→「Advanced」
2. Set debounce time (1-10ms, default 3ms)
3. Set minimum valid rotation interval (1-50ms, default 5ms)
4. Click「Apply」

> ⚠️ **Note**: Excessive debounce parameters reduce encoder response speed. Default values are recommended.

---

## 9. Configuration Management

### 9.1 Save Configuration

After modifying settings, save to the device's Flash:

1. Complete all setting modifications
2. Click「Save Config」button at the bottom right of the software
3. Wait for save to complete (usually 1-2 seconds)
4. Device indicator blinks to indicate successful save

> 💡 **Tip**: Configurations are saved in the device's Flash and persist after power-off.

### 9.2 Restore Factory Defaults

If configurations are messed up or you forget modifications, restore factory defaults:

**Method 1: Via software**
1. Go to「Config Management」→「Restore Defaults」
2. Click「Restore Factory Defaults」
3. Click「OK」in the confirmation dialog
4. Device automatically restarts and restores default configuration

**Method 2: Via key combination**
1. Hold `Fn + Esc` keys
2. Hold for 3 seconds
3. Device indicator blinks rapidly to indicate successful restore
4. Release keys, device automatically restarts

> ⚠️ **Warning**: Restoring factory defaults clears all custom configurations, including key mappings, macros, DPI settings, etc. Please backup configuration before operation.

### 9.3 Export Configuration

Save current configuration to a file for backup or sharing:

1. Go to「Config Management」→「Export」
2. Select export format:
   - **JSON format** (`.json`): Readable, manually editable
   - **Binary format** (`.bin`): Compact, small file size
   - **Config file** (`.hidcfg`): Proprietary format, includes complete configuration
3. Select save location and filename
4. Click「Export」

### 9.4 Import Configuration

Restore settings from a configuration file:

1. Go to「Config Management」→「Import」
2. Click「Select File」, choose configuration file (`.json`/`.bin`/`.hidcfg`)
3. Software displays configuration preview (version, key count, macro count, etc.)
4. Click「Import」
5. Configuration is written to device and takes effect immediately

> ⚠️ **Note**:
> - Importing configuration overwrites all current settings
> - Different version configuration files may be incompatible, confirm version before import
> - It's recommended to export current configuration as backup before import

### 9.5 Configuration Presets

The software has built-in common configuration presets for one-click application:

1. Go to「Config Management」→「Presets」
2. Browse preset list:
   - **Office Mode**: Suitable for daily office, DPI 800, encoder volume mode
   - **Gaming Mode**: Suitable for FPS games, DPI 400, low deadzone
   - **Design Mode**: Suitable for drawing/design, high DPI, encoder zoom mode
   - **Media Mode**: Suitable for entertainment, encoder volume mode
3. Click preset to view details
4. Click「Apply」to write preset to device

> 💡 **Tip**: You can save your own configurations as custom presets for quick switching.

---

## 10. Firmware Update

### 10.1 Check Firmware Version

1. Connect device and open software
2. Check「Firmware Version」in device status bar
3. Or go to「Firmware Update」page to view current version

> 📷 *[Image: Firmware update interface screenshot]*

### 10.2 Download Latest Firmware

1. Visit the project GitHub Releases page
2. Download the latest firmware file (`.uf2` format)
3. Make sure to select the version matching your device hardware

### 10.3 Update via Software (Recommended)

1. Go to「Firmware Update」page
2. Click「Select Firmware File」, choose the downloaded `.uf2` file
3. Software displays firmware information (version, size, checksum)
4. Click「Start Update」
5. Device automatically enters DFU mode
6. Wait for update to complete (progress bar displayed, usually 10-30 seconds)
7. Device automatically restarts after update
8. Verify firmware version after restart

> ⚠️ **Important**:
> - **Do not disconnect USB** during update
> - **Do not close software** during update
> - Ensure computer doesn't enter sleep or hibernate mode
> - It's recommended to use a stable USB port (direct to motherboard, avoid USB Hub)

### 10.4 Manual Update via UF2

If software update fails, you can manually update using UF2 mode:

1. Go to「Firmware Update」page
2. Click「Enter DFU Mode」
3. Device restarts and appears as USB mass storage device (drive letter like `RPI-RP2`)
4. Copy the downloaded `.uf2` file to this drive
5. Device automatically restarts and completes update after copy
6. Reconnect software to verify version

### 10.5 Update Failure Recovery

If update fails, the device may not boot normally. Don't worry, RP2350 has built-in Bootloader for recovery:

1. Disconnect device USB
2. Hold the `BOOTSEL` button on the device (or specific key combination)
3. Connect USB while holding
4. Device appears in UF2 mode
5. Recopy firmware file
6. Wait for update to complete

> 💡 **Tip**: If unsure which button is BOOTSEL, refer to device hardware manual or contact technical support.

### 10.6 Firmware Changelog

After each firmware update, it's recommended to check the changelog for new features and fixes:

1. Visit GitHub Releases page
2. View Release Notes for corresponding version
3. Learn about new features, improvements, and known issues

---

## 11. FAQ

### 11.1 Device Connection Issues

**Q: Device has no response after connecting to computer?**

A:
1. Check if USB cable supports data transfer (some cables only charge)
2. Try changing USB port (direct to motherboard recommended)
3. Try changing USB cable
4. Check for unknown devices in Device Manager
5. Restart computer and retry

**Q: Software shows「No device detected」?**

A:
1. Confirm device is connected to computer via USB
2. Confirm device indicator light is on
3. Click「Refresh device list」button in software
4. Check if other programs are using the device (e.g., other HID tools)
5. Reconnect device
6. Run software as administrator

**Q: Device connects and disconnects repeatedly?**

A:
1. May be insufficient USB port power, try changing port
2. May be poor cable contact, change cable
3. Check device for short circuit or hardware issues
4. If problem persists, contact technical support

### 11.2 Key Issues

**Q: Key press has no response?**

A:
1. Check if the key is set to「No Function」
2. Check if current key layer is correct
3. Test if key is recognized in「Key Test」page
4. If key test also has no response, may be hardware issue
5. Try restoring factory defaults

**Q: Key delay or lag?**

A:
1. Check if too many macros or complex functions are enabled
2. Check if USB connection is stable
3. Try reducing report rate (if supported)
4. Close other CPU-intensive programs
5. Update to latest firmware

**Q: Key mapping doesn't take effect?**

A:
1. Confirm you clicked「Save Config」after modification
2. Confirm correct key layer is selected
3. Check if other configurations override this key
4. Reconnect device and retry
5. Restore factory defaults and reconfigure

### 11.3 Macro Issues

**Q: Macro execution incomplete or skips some actions?**

A:
1. Check if macro delay settings are reasonable
2. Some programs may not respond to fast key presses, increase delay appropriately
3. Check macro loop settings
4. Re-record macro

**Q: Macro can't be stopped?**

A:
1. If it's an infinite loop macro, press the bound key once to stop
2. If unable to stop, disconnect device USB
3. Check macro trigger method settings
4. Delete or modify the macro in software

**Q: Some keys not recorded during macro recording?**

A:
1. Ensure both key press and release are detected during recording
2. Some special keys (like Fn key) may not be recorded
3. Try manually editing macro to add missing actions
4. Update to latest firmware

### 11.4 Mouse Issues

**Q: Cursor movement not smooth or jittery?**

A:
1. Check if mouse sensor surface is clean
2. Use appropriate mouse pad (avoid reflective or uneven surfaces)
3. Adjust DPI settings, excessively high DPI may amplify jitter
4. Check if USB connection is stable
5. Update to latest firmware

**Q: DPI switch doesn't take effect?**

A:
1. Confirm DPI key mapping is correct
2. Confirm configuration is saved
3. Test DPI switching directly in software
4. Check if other configurations override DPI settings

**Q: Scroll wheel not working or reversed?**

A:
1. Check encoder mode settings
2. Check scroll direction in「Encoder Settings」
3. Test if encoder hardware works (does software respond when rotating?)
4. Restore factory defaults and retry

### 11.5 Joystick Issues

**Q: Joystick has drift (cursor moves by itself when not touched)?**

A:
1. Re-perform joystick calibration
2. Increase deadzone appropriately
3. Check if joystick is stuck or has foreign objects
4. If drift is severe, may be hardware issue, contact technical support

**Q: Joystick deadzone settings don't take effect?**

A:
1. Confirm you clicked「Apply」and「Save Config」after modification
2. Observe if deadzone displays correctly in joystick status graph
3. Re-perform joystick calibration
4. Update to latest firmware

**Q: Joystick sensitivity too low or too high?**

A:
1. Adjust sensitivity curve and sensitivity percentage
2. Select appropriate curve type (linear/exponential/logarithmic)
3. Check deadzone settings, excessive deadzone reduces sensitivity
4. Adjust according to use scenario

### 11.6 Firmware Update Issues

**Q: Firmware update failed, what to do?**

A:
1. Do not disconnect device
2. Wait 30 seconds to see if it auto-recovers
3. If device is unresponsive, use UF2 mode for manual recovery (see section 10.4)
4. Check if downloaded firmware file is complete (verify MD5)
5. Try using another USB port or computer

**Q: Device won't boot after update?**

A:
1. Use BOOTSEL button to enter UF2 mode
2. Reflash firmware
3. If still won't boot, may be hardware issue
4. Contact technical support

**Q: How to confirm firmware update succeeded?**

A:
1. Device restarts normally and is recognized by computer
2. Software displays new firmware version number
3. Device functions normally
4. Can view version info in software「About」page

### 11.7 Software Issues

**Q: Software won't start?**

A:
1. Confirm you downloaded the correct version (Windows/macOS/Linux)
2. Windows users: Confirm using self-contained version or .NET runtime is installed
3. macOS users: First run requires right-click → Open, confirm security settings
4. Linux users: Confirm execute permission is added
5. Check if system meets minimum requirements

**Q: Software interface displays abnormally or lags?**

A:
1. Ensure using latest software version
2. Check system display scaling settings (100% or 150% recommended)
3. Close other resource-intensive programs
4. Update graphics driver
5. Try running in compatibility mode

**Q: Configuration reverts after saving and restarting device?**

A:
1. Confirm you clicked「Save Config」not just「Apply」
2. Wait for save completion prompt
3. Check if device Flash has issues
4. Try restoring factory defaults then reconfigure
5. Update to latest firmware

---

## 12. Technical Specifications

### 12.1 Hardware Specifications

| Item | Specification |
|------|--------------|
| MCU | RP2350 (Dual-core ARM Cortex-M33 @ 150MHz) |
| Memory | 520KB SRAM |
| Storage | 2MB Flash (expandable to 4MB/8MB/16MB) |
| USB | USB 2.0 Full Speed, Type-C interface |
| Keys | Up to 64 keys (configurable) |
| Joystick | Dual-channel 12-bit ADC analog joystick |
| Encoder | Rotary encoder (with push button) |
| Mouse Sensor | Optical sensor, up to 26000 DPI |
| Indicator | RGB LED (configurable) |
| Operating Voltage | 5V (USB powered) |
| Operating Current | < 500mA |
| Dimensions | Depends on specific product model |
| Weight | Depends on specific product model |

### 12.2 Software Specifications

| Item | Specification |
|------|--------------|
| Firmware | Custom HID firmware based on TinyUSB |
| Software | .NET 10 + Avalonia UI (cross-platform) |
| Key Layers | 4 layers (Layer 0-3) |
| Macro Count | Up to 16 macros |
| Macro Length | Up to 64 actions per macro |
| DPI Levels | 4 presets + custom |
| DPI Range | 100 - 26000 DPI |
| Config Storage | Flash dual backup, CRC verification |
| Report Rate | 1000Hz (1ms) |
| Config Import/Export | JSON / BIN / HIDCFG |
| Firmware Update | USB DFU / UF2 |

### 12.3 USB Descriptors

| Item | Value |
|------|-------|
| VID | 0x1209 (pid.codes) |
| PID | 0x4004 (Application) / 0x4005 (Bootloader) |
| Manufacturer String | Zhidong Technology |
| Product String | Zhidong HID Composite Device |
| USB Version | 2.0 (Full Speed) |
| Device Class | Composite HID device |
| Interface Count | 2 HID interfaces |
| Endpoints | EP1 IN (standard HID), EP2 IN (config interface) |

### 12.4 System Compatibility

| OS | Minimum Version | Test Status |
|----|----------------|-------------|
| Windows | Windows 10 1809 | ✅ Tested |
| Windows 11 | 22H2+ | ✅ Tested |
| macOS | 12 Monterey | ✅ Tested |
| macOS | 13/14 Ventura/Sonoma | ✅ Tested |
| Linux | Ubuntu 20.04 | ✅ Tested |
| Linux | Ubuntu 22.04/24.04 | ✅ Tested |
| Android | OTG support | ⚠️ Partial functionality |
| iOS | - | ❌ Not supported |

### 12.5 Compliance Certification

| Certification | Status |
|--------------|--------|
| USB-IF Certification | Pending application |
| FCC | Pending application |
| CE | Pending application |
| RoHS | Compliant |
| REACH | Compliant |

> ⚠️ **Note**: This product is an open-source hardware project. Please ensure required compliance certifications are completed before commercial sale.

---

## Appendix

### A. Shortcut Key Reference

| Key Combination | Function |
|----------------|----------|
| Fn + Esc (hold 3s) | Restore factory defaults |
| Fn + F1 | Switch to Layer 1 |
| Fn + F2 | Switch to Layer 2 |
| Fn + F3 | Switch to Layer 3 |
| DPI Button | Cycle DPI levels |
| BOOTSEL | Enter UF2 update mode |

### B. Contact Us

- **Project Homepage**: https://github.com/JYW-zero/pico_hid_composite
- **Issue Feedback**: https://github.com/JYW-zero/pico_hid_composite/issues
- **Technical Support**: Please submit issues via GitHub Issues

### C. Version History

| Version | Date | Major Updates |
|---------|------|---------------|
| v1.0 | 2026-08-20 | Initial version user manual |

---

**Thank you for using the Zhidong HID Composite Device!**

For questions or suggestions, welcome to provide feedback via GitHub.
