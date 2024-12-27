# SNTP

在 Linux 上，时区通常使用文件设置/etc/localtime。此文件通常是指向实际二进制 tzfile 的符号链接，/usr/share/zoneinfo/以时区标识符命名，例如“America/Edmonton”。

tzfile 包含 `libc库` 中 `tzset()` 函数使用的时区信息。文件格式在 tzfile(5) 中描述。


没有时区数据库文件的系统（例如嵌入式系统）通常使用环境变量设置时区TZ。嵌入式系统通常会TZ从诸如之类的文件中读取并导出变量/etc/TZ。


|地区|时区|编码|
|---|----|---|
|America/New York||EST5EDT,M3.2.0,M11.1.0|
|Asia/Shanghai|东八区|CST-8|
|Asia/Singapore||SGT-8|
|Asia/Taipei||CST-8|
其它地区对应的时区编码，可以查阅 [时区](https://leo.leung.xyz/wiki/Timezone)

## 参考来源

[系统时间](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32/api-reference/system/system_time.html)

