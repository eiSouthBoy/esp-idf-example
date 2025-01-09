# mdns

[esp32 mDNS](https://docs.espressif.com/projects/esp-idf/zh_CN/v5.3.2/esp32/api-reference/protocols/mdns.html)

mDNS 是一种组播 UDP 服务 (使用端口号：5353)，用来提供 **本地网络服务** 和 **主机发现**。

自 v5.0 版本起，ESP-IDF 组件 mDNS 已从 ESP-IDF 中迁出至独立的仓库：[GitHub 上 mDNS 组件](https://github.com/espressif/esp-protocols/tree/master/components/mdns)

在项目下运行 `idf.py add-dependency espressif/mdns`，在项目中添加 mDNS 组件。