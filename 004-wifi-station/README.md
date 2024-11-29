# wifi station

## 设备

开发板：[ESP32-DevKitC](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32/esp32-devkitc/user_guide.html)，板载模组是 ESP32-WROOM-32E


## 步骤

`idf.py set-target esp32` ：选择目标SoC

`idf.py menuconfig` ：配置菜单 "Example Configuration STA" 中设置 Remote AP 的SSID、密码、加密方式、最大重连次数。
    SSID: myssid
    Password: mypassword
    AUTH MODE: WPA2-PSK
    Maximum retry: 5


`idf.py build` ：编译代码

`idf.py -p /dev/ttyUSB0 flash` ：烧录固件

`idf.py -p /dev/ttyUSB0 monitor` ：监控日志