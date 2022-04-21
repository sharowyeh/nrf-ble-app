Controls nRF5xDK BLE connectivity FW to communicate with target BLE devices.

The Nordic nRF Connect app is perfect, but I want to use it as fully tranditional windows desktop app

Note for my unreliable memory:
- original source code references from nrf-ble-driver example code and firmware testcases


### Environment requirement ###
- nRF5xDK board(PCA10059) support SoftDevice s132 or later version, refer to [IC description](https://github.com/NordicSemiconductor/pc-ble-driver/tree/master#softdevice-and-ic) section on offical github site
- Nordic connectivity firmware release binary [v4.1.4](https://github.com/NordicSemiconductor/pc-ble-driver/tree/v4.1.4-hex/hex/sd_api_v5) or v4.1.2 with api_v5, can be downloaded from offical github source file
- pc-ble-driver development dependencies v4.1.2(or v4.1.4), can be downloaded from [pc-ble-driver/releases](https://github.com/NordicSemiconductor/pc-ble-driver/releases)
This project references v4.1.2 header and lib files, but programmed v4.1.4 firmware to dev kit will get better data stacks performance, while receving continueous packets from peripheral device


### Development dependencies ###
Basically use x86_32 architecture for backward compatible, or follow the same steps for x86_64 architecture
- Extract nrf-ble-driver-4.1.2-win_x86_32.zip to parent folder of this repo directory
- The path structure will be:
```
---/nrf-ble-app/...
 |-/nrf-ble-driver-4.1.2-win_x86_32/...
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
Recommendation is v4.1.4 which function verified for this repo

Use Segger tool app(nrfjprog.exe) and dev kit driver(jlinkarm) from Nordic offical website, to program hex file to dev kit.

Also can simply install Nordic nRF Connect for Desktop and use bluethooth app to proceed connectivity firmware programming,
refer steps as below:
- Install nrfconnect-setup-3.8.0.exe
- Download Bluetooth Low Energy app(offical, v3.0.0)
- (offline alternative) Extract pc-nrfconnect-ble_3.0.0_offical.zip to *<user_dir>*/.nrfconnect-apps/node_modules/ , rename unzipped folder to **pc-nrfconnect-ble** 
- Launch nRF Connect for Desktop, will show Bluetooth Low Energy(offical, v3.0.0) on apps list
- Plug nRF5DK board(PCA10059) in USB port
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

### Other ways to win32 ###
- write javascript pc-nrfconnect-ble and pc-ble-driver-js projects from nRF Connect SDK
- provide interface for win32 app, likes local web api, OS dependent IPC, or so forth

pc-nrfconnect-ble/lib/actions/adapterActions.js
  - adaptertoUse.xxxx
  - pc-ble-driver-js/api/adapter.js

pc-nrfconnect-ble/lib/actions/deviceDetailsActions.js
  - adapterToUse.xxxx
  - pc-ble-driver-js/api/adapter.js

