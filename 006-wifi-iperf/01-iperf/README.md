# wifi iperf

来自乐鑫 ESP-IDF 的项目 [wifi iperf](https://github.com/espressif/esp-idf/tree/v5.2.2/examples/wifi/iperf)

对于原来的项目基本没有做大修改，仅修改了组件引用，通过项目顶层 CMakeLists.txt 增加组件引用。而原来的方式是通过在main组件文件夹中 idf_component.yml 增加组件依赖。

目的：测试 STA 模式下，传输速率大小；测试 AP 模式下，传输速率大小。