# hello world project

## 设备

开发板：[ESP32-DevKitC](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32/esp32-devkitc/user_guide.html)，板载模组是 ESP32-WROOM-32E


通过这个程序可以获取芯片信息、flash信息、head信息，通过EN按键或Boot按键触发ESP32系统重启，但导致重启原因不一样。

- 按下EN按键，ESP32芯片复位，导致系统重启，再次启动系统时，获取的最新一次重启事件是 ESP_RST_POWERON。

- 按下Boot按键，调用 esp_restart() 使系统软件复位，导致系统重启，
这个程序没有使用button component，而是注册一个Task持续扫描按键的状态来判断是否按下。再次启动系统时，获取的最新一次重启事件是 ESP_RST_SW

## 步骤

`idf.py set-target esp32` ：选择目标SoC

`idf.py menuconfig` ： 配置菜单

`idf.py build` ：编译代码

`idf.py -p /dev/ttyUSB0 flash` ：烧录固件

`idf.py -p /dev/ttyUSB0 monitor` ：监控日志