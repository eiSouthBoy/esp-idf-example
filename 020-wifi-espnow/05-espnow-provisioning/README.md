# espnow provisioning

**initiator mode**

1、持续扫描 provision 信标(beacon)，直到收到它。
2、发送 "设备类型配置帧"，请求驻留在响应方设备中的 WiFi 凭证。
3、如果接收 WiFi 类型的 provision 帧，则从帧中获取 WiFi 凭证并连接到 Router。


**responder mode**

1、在 30 秒内每 100 毫秒广播一次 Provision 信标(beacon)。
2、如果接收 "设备类型配置帧"，则使用包含 WiFi 凭证的 WiFi 类型配置帧进行响应。