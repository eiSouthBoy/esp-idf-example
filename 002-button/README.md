# button component的使用

## 设备

开发板：[ESP32-DevKitC](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32/esp32-devkitc/user_guide.html)，板载模组是 ESP32-WROOM-32E

LED： 3W的LED模块，HW-269，共有三个针脚，分别是 +（接开发板5V）、G（接开发板的GND）、S（接开发板的GPIO） 


## 步骤

`idf.py set-target esp32` ：选择目标SoC

`idf.py menuconfig` ：配置菜单 "Example Button" 中设置LED和BUTTON的GPIO，默认LED的GPIO是0，BUTTON的GPIO是4。ESP32-DevKitC上Boot 案件对应的GPIO就是GPIO 0

`idf.py build` ：编译代码

`idf.py -p /dev/ttyUSB0 flash` ：烧录固件

`idf.py -p /dev/ttyUSB0 monitor` ：监控日志
