Controls nRF5xDK BLE connectivity FW to communicate with target BLE devices.

The Nordic nRF Connect app is perfect, but I want to use it as fully tranditional windows desktop app

Note for my unreliable memory:

- project referenced from examples in nrf-ble-driver source code, but it is not well kowning for desktop app fully controls nRF5xDK BLE connectivity FW, to communicate with target BLE devices.

- nrf-ble-driver dependency libraries are required for building app, release build can be downloaded from [nordic repo](https://github.com/NordicSemiconductor/pc-ble-driver/releases), extract archives both x86 32 and 64, and place the same directory which contains this solution folder.

another opetion: reading pc-nrfconnect-ble and pc-ble-driver-js projects from nRF Connect SDK

pc-nrfconnect-ble/lib/actions/adapterActions.js
  - adaptertoUse.xxxx
  - pc-ble-driver-js/api/adapter.js

pc-nrfconnect-ble/lib/actions/deviceDetailsActions.js
  - adapterToUse.xxxx
  - pc-ble-driver-js/api/adapter.js

