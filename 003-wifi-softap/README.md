# wifi softap


## 设备

开发板：[ESP32-DevKitC](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32/esp32-devkitc/user_guide.html)，板载模组是 ESP32-WROOM-32E

LED： 3W的LED模块，HW-269，共有三个针脚，分别是 +（接开发板5V）、G（接开发板的GND）、S（接开发板的GPIO） 


## 步骤

`idf.py set-target esp32` ：选择目标SoC

`idf.py menuconfig` ：配置菜单 "Example AP Configuration" 中设置 AP 的SSID、密码、信道、最大连接数、加密方式。也可以不做修改直接编译，但会使用默认值。
    SSID: ESP32
    Password: 12345678
    Channel: 1
    AUTH MODE: WPA-WPA2-PSK

`idf.py build` ：编译代码

`idf.py -p /dev/ttyUSB0 flash` ：烧录固件

`idf.py -p /dev/ttyUSB0 monitor` ：监控日志

**注意事项：**
    1、不同国家对于 2.4G 频段的信道有要求，如美国仅支持1~11信道，不支持12、13信道；如中国和欧洲支持1~13信道；如日本支持1~14信道。对于AP默认的国家码和信道范围：01 和 1~11

## 参考
[ESP 错误处理](https://docs.espressif.com/projects/esp-idf/zh_CN/v5.3.1/esp32/api-guides/error-handling.html)
[ESP-NETIF](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/network/esp_netif.html)
[ESP Wi-Fi 安全性](https://docs.espressif.com/projects/esp-idf/zh_CN/v5.2.3/esp32/api-guides/wifi-security.html)
[ESP8266 国家码](https://blog.csdn.net/espressif/article/details/78673702)
[ESP-FAQ Wi-Fi 国家/地区代码](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32/api-guides/wifi.html#id45)

