Controls nRF5xDK BLE connectivity FW to communicate with target BLE devices.

The Nordic nRF Connect app is perfect, but I want to use it as fully tranditional windows desktop app

Note for my unreliable memory:

- origin source code references from nrf-ble-driver heart beat collector and SDK testcases

- nRF5xDK board(PCA10069) support SoftDevice s132 or later version
  refer to offical github [pc-ble-driver](https://github.com/NordicSemiconductor/pc-ble-driver/tree/master#softdevice-and-ic)

- Nordic connectivity firmware release binary v4.1.2(or v4.1.4) with api_v5
  can be downloaded from [pc-ble-driver/hex](https://github.com/NordicSemiconductor/pc-ble-driver/tree/v4.1.2/hex/sd_api_v5)

- pc-ble-driver development dependencies v4.1.2(or v4.1.4)
  can be downloaded from [pc-ble-driver/releases](https://github.com/NordicSemiconductor/pc-ble-driver/releases)


another option: 
- write javascript pc-nrfconnect-ble and pc-ble-driver-js projects from nRF Connect SDK
- provide interface for win32 app, likes local web api or so forth

pc-nrfconnect-ble/lib/actions/adapterActions.js
  - adaptertoUse.xxxx
  - pc-ble-driver-js/api/adapter.js

pc-nrfconnect-ble/lib/actions/deviceDetailsActions.js
  - adapterToUse.xxxx
  - pc-ble-driver-js/api/adapter.js

