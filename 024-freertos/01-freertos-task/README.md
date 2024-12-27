# ESP-IDF freeRTOS

## freeRTOS
官方 FreeRTOS （下文称为原生 FreeRTOS）是一个单核 RTOS。为了支持各种多核 ESP 目标芯片，ESP-IDF 支持下述不同的 FreeRTOS 实现。

ESP-IDF FreeRTOS 是基于原生 FreeRTOS v10.5.1 的 FreeRTOS 实现，其中包含支持 SMP 的大量更新。ESP-IDF FreeRTOS 最多支持两个核（即双核 SMP），但在设计上对这种场景进行了优化。

原生 FreeRTOS 自带 堆实现选择，然而 ESP-IDF 已经实现了自己的堆（参见 堆内存分配），因此不使用原生 FreeRTOS 的堆实现。



## 参考引用

[ESP-IDF freeRTOS](https://docs.espressif.com/projects/esp-idf/zh_CN/v5.2.3/esp32/api-reference/system/freertos_idf.html)

