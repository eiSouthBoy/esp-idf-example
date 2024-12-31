# ehternet

ESP32-DevKitC 和 LAN8720 接线：

| ESP32  | RMII Signal | LAN8720 | Notes        |
| ------ | ----------- |---------| ------------ |
| GPIO21 | TX_EN       | TX_EN | EMAC_TX_EN     |
| GPIO19 | TX0         | TX0   | EMAC_TXD0      |
| GPIO22 | TX1         | TX1   | EMAC_TXD1      |
| GPIO25 | RX0         | RX0   | EMAC_RXD0      |
| GPIO26 | RX1         | RX1   | EMAC_RXD1      |
| GPIO27 | CRS_DV      | CRS   | EMAC_RX_DRV    |
| 3V3    | /           | VCC   | /              |
| GND    | /           | GND   | /              |


`idf.py menuconfig` 菜单配置选项：

Ethernet PHY Device --> LAN8700
RMII clock mode --> Output RMII clock from internet