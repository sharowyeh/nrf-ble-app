Controls nRF5xDK BLE connectivity FW to communicate with target BLE devices.

The Nordic nRF Connect app is perfect, but I want to use it as fully tranditional windows desktop app

Note for my unreliable memory:
- original source code references from nrf-ble-driver example code and firmware testcases


### Environment requirement ###
- nRF5xDK board(PCA10059) support SoftDevice s132 or later version, refer to [IC description](https://github.com/NordicSemiconductor/pc-ble-driver/tree/master#softdevice-and-ic) section on offical github site
- Nordic connectivity firmware release binary [v4.1.4](https://github.com/NordicSemiconductor/pc-ble-driver/tree/v4.1.4-hex/hex/sd_api_v5) or further specific release branch
- pc-ble-driver development dependencies v4.1.4, can be downloaded from [pc-ble-driver/releases](https://github.com/NordicSemiconductor/pc-ble-driver/releases)
Recommeded v4.1.4 header and lib files to develop board programmed v4.1.4 connectivity firmware, for better data stacks performance while receving continueous packets from peripheral device. but also works with v4.1.2 version for backward compatible.


### Development dependencies ###
Basically use x86_32 architecture for platform compatible, or follow the same steps for x86_64 architecture
- Extract nrf-ble-driver-4.1.4-win_x86_32.zip to parent folder of this repo directory
- The path structure will be:
```
---/nrf-ble-app/...
 |-/nrf-ble-driver-4.1.4-win_x86_32/...
```
- The c++ project properties will look up given folder for header and lib files automatically
- (optional) For debug configuration, may pull nrf-ble-driver source code to parent folder of this repo directory
  - build the completed connectivity source
  - replace the built firmware hex and driver libs to offical binaries

- C++ UI example project using [imgui](https://github.com/ocornut/imgui) which required [GLFW](https://www.glfw.org/docs/3.3/index.html), refer to section `UI dependencies` for more details


### Install Nordic nRF Connect for Desktop ###
This launcher app includes related segger programmer nrfjprog and development board driver jlinkcdcarm,
or alternatively install by their own installer which can be downloaded from Nordic offical website.

(optional) BLE related apps for launcher:
- Programmer: for programming Nordic BLE connectivity FW to nRF5xDK board(or alternatively using segger nrfjprog.exe)
- Bluetooth Low Energy: for double check if our app works like this offical app(CANNOT run at the same time with this project's binary)

The next section contains FW programming steps by nRF Connect for Desktop


### Nordic connectivity firmware programming ###
Recommendation is v4.1.4 which function verified for this repo, it has 3 different ways to program firmware hex file to the board.

- Method1: Use Segger tool app(nrfjprog.exe) and dev kit driver(jlinkarm) from Nordic offical website, to program hex file to dev kit.

Also can simply install Nordic nRF Connect for Desktop:
- Method2: Use programming app
  - Install refconnect-setup from nordic offical website
  - Download Programmer app from offical(or alternative offline extract .nrfconnect-apps.zip to *<user_dir>*/.nefconnect-apps/
  - Launch nRF Connect for Desktop, Programmer app will show on apps list
  - Plug nRF5DK board(PCA10059) into USB port
  - Open Programmer app then select detected board device
  - Add hex file for downloaded connectivity firmware hex(eg. connectivity_4.1.4_usb_with_s132_5.1.0.hex)
  - Write and wait until programming finished
- Method3: Use bluethooth app to proceed connectivity firmware programming, steps as below:
  - Download Bluetooth Low Energy app from offical(or alternative offline extract .nrfconnect-apps.zip to *<user_dir>*/.nefconnect-apps/
  - Open Bluetooth Low Energy app than select device, it will ask the device must be programmed
  - Proceed firmware programming, log window will show up Nordic connectivity version and detailed information


### UI dependencies ###
The C++ UI example app integrated GLFW via imgui

- [imgui](https://github.com/ocornut/imgui) source code is fully copied to `imgui` folder in the project directory as including references.
- Download [GLFW binaries](https://www.glfw.org/download.html) both x86 and x64 for windows environment, extract to the parent of solution directory
- The path structure will be:
```
---/nrf-ble-app/...
 |-/nrf-ble-app/nrf_ble_sample_glfw/imgui/...
 |-/glfw-3.3.7.bin.WIN32/...
```

### BLE pairing refs ###
Bluetooth pairing - blog [LESC(p4)](https://www.bluetooth.com/blog/bluetooth-pairing-part-4/), [legacy(p1)](https://www.bluetooth.com/blog/bluetooth-pairing-passkey-entry/), [keygen(p2)](https://www.bluetooth.com/blog/bluetooth-pairing-part-2-key-generation-methods/), [passkey(p3)](https://www.bluetooth.com/blog/bluetooth-pairing-passkey-entry/)

[Deep dive into LESC](https://medium.com/rtone-iot-security/deep-dive-into-bluetooth-le-security-d2301d640bfc)

[auth responding: BLE_GAP_SEC_STATUS_AUTH_REQ](https://devzone.nordicsemi.com/f/nordic-q-a/16618/sd_ble_gap_encrypt-does-not-seem-to-encrypt-link-after-bonding-done)

[S132-v5 Message Sequence Charts](https://infocenter.nordicsemi.com/index.jsp?topic=%2Fcom.nordic.infocenter.s132.api.v5.0.0%2Fs132_msc_overview.html&cp=4_7_3_7_1)


### Other ways to win32 ###
- write javascript pc-nrfconnect-ble and pc-ble-driver-js projects from nRF Connect SDK
- provide interface for win32 app, likes local web api, OS dependent IPC, or so forth

pc-nrfconnect-ble/lib/actions/adapterActions.js
  - from:
  - adaptertoUse.xxxx
  - pc-ble-driver-js/api/adapter.js
  - xxxxAction to:
pc-nrfconnect-ble/lib/reducers/bleEventReducer.js
  - AdapterActions.DEVICE_xxxx_REQUEST
pc-nrfconnect-ble/lib/containers/BLEEventDialog.jsx
  - BLEEventType.xxxx_REQUEST
  - UI Submit export function xxxx(in adapterActions.js)

pc-nrfconnect-ble/lib/actions/deviceDetailsActions.js
  - adapterToUse.xxxx
  - pc-ble-driver-js/api/adapter.js

