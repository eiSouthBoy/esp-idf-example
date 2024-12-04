# wifi iperf softap-sta

修改代码逻辑：WiFi 初始化后，设置模式为 AP+STA，STA 连接指定的路由器AP。

组件的引用方式和乐鑫 wifi iperf 一样，都是通过 main 文件夹下的 idf_component.yml 增加组件依赖。

目的：测试 AP + STA 同时存在时，对于 STA 传输速率的影响。