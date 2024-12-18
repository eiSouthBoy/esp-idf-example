
idf.py monitor
```bash
I (203) cpu_start: Multicore app
I (212) cpu_start: Pro cpu start user code
I (212) cpu_start: cpu freq: 160000000 Hz
I (212) cpu_start: Application information:
I (215) cpu_start: Project name:     oneshot_read
I (220) cpu_start: App version:      db77699-dirty
I (226) cpu_start: Compile time:     Dec 10 2024 11:05:06
I (232) cpu_start: ELF file SHA256:  e2ed21d7d...
I (237) cpu_start: ESP-IDF:          v5.2.2-dirty
I (243) cpu_start: Min chip rev:     v0.0
I (247) cpu_start: Max chip rev:     v3.99 
I (252) cpu_start: Chip rev:         v3.1
I (257) heap_init: Initializing. RAM available for dynamic allocation:
I (264) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (270) heap_init: At 3FFB2AF8 len 0002D508 (181 KiB): DRAM
I (276) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (283) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (289) heap_init: At 4008C784 len 0001387C (78 KiB): IRAM
I (297) spi_flash: detected chip: generic
I (300) spi_flash: flash io: dio
I (305) main_task: Started on CPU0
I (315) main_task: Calling app_main()
I (315) oneshot_read: calibration scheme version is Line Fitting
I (315) oneshot_read: Calibration Success
I (315) oneshot_read: ADC1 Channel[4] Raw Data: 2702
I (325) oneshot_read: ADC1 Channel[4] Cali Voltage: 2402 mV
I (1335) oneshot_read: ADC1 Channel[4] Raw Data: 2710
I (1335) oneshot_read: ADC1 Channel[4] Cali Voltage: 2409 mV
I (2335) oneshot_read: ADC1 Channel[4] Raw Data: 2704
I (2335) oneshot_read: ADC1 Channel[4] Cali Voltage: 2404 mV
```

|电压输入(V)|数字输出|测量电压(V)|
|2.0|2215|1.995|
|2.1|2340|2.099|
|2.2|2486|2.221|
|2.3|2585|2.304|
|2.4|2705|2.404|
|2.5|2841|2.518|
|2.6|2959|2.598|
|2.7|3124|2.703|
|2.8|3291|2.798|
|2.9|3492|2.902|
|3.0|3703|2.999|
|3.1|3920|3.094|
|3.2|4095|3.165|
|3.3|4095|3.165|