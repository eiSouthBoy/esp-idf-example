# ethernet to ap

乐鑫提供了 ESP-IOT-Bridge 联网方案，其中 `eth2ap` 就是其中之一，其功能是："Wi-Fi 路由器"。

应用场景可以参考下图：

![Wi-Fi 路由器](./doc/wifi-router.png)

*注意：eth2ap demo工程，ESP32其实不是完整的路由器，因为它本身不提供DHCP Server功能，而是有它的上层路由器分配IP地址，我更愿意称它为 “瘦AP”。*
