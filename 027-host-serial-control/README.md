# Host Serial Control

通过主机程序控制ESP32开发板的串口芯片CP2102N，包括控制DTR和RTS，以及对串口的发送和接收。

ESP32开发板通过 `USB-A 转 USB-MicroB` 线连接到电脑的 `USB-A` 接口，操作系统会加载相应的 USB 驱动程序（例如：cp210x.ko）。这个驱动程序负责处理 `USB设备` 的枚举过程，并提供一个虚拟 `COM端口` (即VCP)，具体的体现形式是在电脑上出现 `/dev/ttyUSB0` 或 `/dev/ttyACM0` 或 `COM0`，不同的系统体现形式有所不同，使得软件可以像使用传统的串行端口一样使用该设备。



实现的方式：C、Python、Web

