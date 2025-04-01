#!/usr/bin/env python3

import socket
import getopt
import sys

def main():
    # 定义默认值（可选）
    server_ip = None
    server_port = None

    # 定义帮助信息
    usage_info = """
Usage: tcp_client.py -h <host> -p <port>
Options:
  -h, --host   The IP address of the server.
  -p, --port   The port number to connect to.
"""

    try:
        # 解析命令行参数
        opts, args = getopt.getopt(sys.argv[1:], "h:p:", ["host=", "port="])
        
        for opt, arg in opts:
            if opt in ("-h", "--host"):
                server_ip = arg
            elif opt in ("-p", "--port"):
                server_port = int(arg)

        # 检查是否提供了必要的参数
        if not server_ip or not server_port:
            print("Error: Host and Port must be provided.")
            print(usage_info)
            sys.exit(2)

        print(f"Connecting to {server_ip}:{server_port}")

        # 创建一个TCP/IP套接字
        client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        try:
            # 连接到服务器
            client_socket.connect((server_ip, server_port))
            print("Connected successfully")

            while True:
                try:
                    # 用户输入消息
                    message = input("Send (or type 'exit' to quit): ")
                    
                    if message.lower() == 'exit':
                        break

                    # 发送数据到服务器
                    client_socket.sendall(message.encode())

                    # 接收响应
                    response = client_socket.recv(1024)
                    if not response:  # 如果没有收到任何数据，可能是服务器断开了连接
                        print("Server closed the connection.")
                        break
                    
                    print(f"Received from server: {response.decode()}")

                except KeyboardInterrupt:
                    print("\nClient interrupted. Exiting...")
                    break
                except Exception as e:
                    print(f"An error occurred: {e}")
                    break

        finally:
            # 关闭连接
            client_socket.close()
            print("Connection closed")

    except getopt.GetoptError as err:
        # 如果参数格式错误，打印帮助信息并退出
        print(err)  # 打印错误信息
        print(usage_info)
        sys.exit(2)

if __name__ == '__main__':
    main()