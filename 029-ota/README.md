# ota


## http
029-ota 项目应用烧录到 ESP32 后，创建一个OTA请求任务。

001-hello-world 项目下的build目录存在 hello-world.bin 固件镜像，在该目录下运行以下命令： `python -m http.server 8070`


```bash
I (5893) simple_ota_example: Starting OTA example task
I (5893) simple_ota_example: Bind interface name is st1
I (5903) simple_ota_example: Attempting to download update from http://192.168.5.170:8070/hello_world.bin
I (5923) main_task: Returned from app_main()
I (6873) esp_https_ota: Starting OTA...
I (6873) esp_https_ota: Writing to partition subtype 16 at offset 0x110000
I (35773) esp_image: segment 0: paddr=00110020 vaddr=3f400020 size=0a240h ( 41536) map
I (35793) esp_image: segment 1: paddr=0011a268 vaddr=3ffb0000 size=02254h (  8788) 
I (35793) esp_image: segment 2: paddr=0011c4c4 vaddr=40080000 size=03b54h ( 15188) 
I (35803) esp_image: segment 3: paddr=00120020 vaddr=400d0020 size=14bach ( 84908) map
I (35833) esp_image: segment 4: paddr=00134bd4 vaddr=40083b54 size=08c9ch ( 35996) 
I (35853) esp_image: segment 0: paddr=00110020 vaddr=3f400020 size=0a240h ( 41536) map
I (35873) esp_image: segment 1: paddr=0011a268 vaddr=3ffb0000 size=02254h (  8788) 
I (35873) esp_image: segment 2: paddr=0011c4c4 vaddr=40080000 size=03b54h ( 15188) 
I (35883) esp_image: segment 3: paddr=00120020 vaddr=400d0020 size=14bach ( 84908) map
I (35913) esp_image: segment 4: paddr=00134bd4 vaddr=40083b54 size=08c9ch ( 35996) 
I (36013) simple_ota_example: OTA Succeed, Rebooting...
```


## https

执行命令： `python3 pytest_simple_ota.py ./ 8070 ./` ，首先要执行下面的命令

```bash
get_idf
export PYTHONPATH="$PYTHONPATH:$IDF_PATH/tools/ci/python_packages"
```