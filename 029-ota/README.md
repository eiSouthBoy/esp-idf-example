# ota

ota升级可以选择http 或 https 两种方式。

## http
029-ota 项目应用烧录到 ESP32 后，创建一个OTA请求任务。

001-hello-world 项目下的build目录存在 hello-world.bin 固件镜像，在该目录下运行以下命令： `python -m http.server 8070`

`idf.py menuconfig` 菜单中的 url 需要修改，http --> https

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

1、首先需要使用 openssl 为 https 服务器创建私钥和证书。

在项目下创建一个文件夹来保存服务器公钥、证书，以及待升级的固件文件。

```bash
mkdir -p https_server_test
cd https_server_test
# 生成公钥和证书。注意：命令执行后，需要用户逐步输入提示信息
openssl req -x509 -newkey rsa:2048 -keyout ca_key.pem -out ca_cert.pem -days 365 -nodes
```

2、使用python3运行一个https服务

首先要执行下面的命令，因为 `pytest_simple_ota.py` 需要依赖 `esp-idf` 工具库。

```bash
# 激活esp-idf
get_idf

# 设置 Python 工具库环境变量
export PYTHONPATH="$PYTHONPATH:$IDF_PATH/tools/ci/python_packages"
```

运行https服务，执行命令： 

```bash
cd https_server_test
python3 pytest_simple_ota.py ./ 8070 ./
```

3、验证https证书和网络

可以使用 curl 工具验证 https 证书是否可用。在 `https_server_test` 目录下创建一个html文件：index.html

```html
<!DOCTYPE html>
<html>
<head>
    <title>My First Web Page</title>
</head>
<body>

<h1>Hello World!</h1>

</body>
</html>

```

使用 curl 发送 http request，执行命令：

```bash
cd https_server_test
curl -v --cacert ca_cert.pem https://192.168.5.170:8070/index.html
```

*注意：192.168.5.170 是本地主机的IP地址*

4、烧录固件到ESP32，通过串口观察日志。

