# v0.1：ESP-01S AP Wi-Fi 门锁

第一版固件与硬件工程归档。ESP8266 提供本地 Wi-Fi 网页、密码开锁、管理员配置和舵机控制。

- [原理结构图、开锁流程和 Wi-Fi 策略](docs/architecture.md)
- [Arduino 固件入口](arduino-esp01s-esp01s-ap-wifi-io0/esp01s_ap_lock/esp01s_ap_lock.ino)
- [EPro 硬件工程](ProPrj_门锁升级_2026-10-03.epro2)
- [当前 v0.2](../v0.2/README.md)

归档固件使用 GPIO0 控制舵机电源、GPIO2 输出 PWM。`DEBUG_MODE=1`，默认 Wi-Fi 常开；切换为正常模式后，按时段、客户端连接及每5分钟开启1分钟的策略运行。

用 Arduino IDE 和 ESP8266 Arduino Core 打开上述草图编译。接线、启动电平及电源驱动应结合实际板卡核对。此目录新增文档，原有 v0.1 固件保持归档。
