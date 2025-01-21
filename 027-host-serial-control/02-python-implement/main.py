# -*- coding: utf-8 -*-

# 第一步：确定串口名，分为两种情况 
#   1）对于给定目标串口名(例如 /dev/ttyUSB0 or /dev/ttyACM0 or COM14)
#   2）没有指定目标串口名，需要在计算机上编译所有的的串口，然后指定一个可用串口名
# 第二步：打开串口
# 第三步：使用串口Read or Write 数据
# 第四步：关闭串口

import serial # 导入串口库 
import time
import threading
import getopt
import sys


def open_serial(port, baudrate=9600):
    """
    打开指定的串口。
    
    :param port: 串口号，例如 'COM1' 或 '/dev/ttyUSB0'
    :param baudrate: 波特率，默认为 9600
    :return: Serial 对象
    """
    try:
        ser = serial.Serial(port, baudrate, timeout=1)
        print(f"Open port {port} success")
        return ser
    except serial.SerialException as e:
        print(f"Open port {port} fail: {e}")
        return None

def read_data(ser):
    """
    从串口读取数据。
    
    :param ser: Serial 对象
    :return: 读取的数据
    """
    if ser is not None and ser.is_open:
        # 读取一行数据
        line = ser.readline()
        if line:
            print(f"Recv Data: {line.decode('utf-8').strip()}")
        else:
            print("Not Recv Data")
    else:
        print("serial not open")

def read_data_cb(ser, stop_event):
    """
    从串口读取数据的函数，运行在一个单独的线程中。
    
    :param ser: Serial 对象
    """
    try:
        received_data = []
        while not stop_event.is_set():
            if ser.in_waiting > 0:
                line = ser.readline()
                if line:
                    data = line.decode('utf-8').strip()
                    received_data.append(data)
                    print(f"Recv Data: {data}")
            time.sleep(0.01)  # 避免CPU占用过高
    except KeyboardInterrupt:
        print("Data reading stopped.") 

def write_data(ser, data):
    """
    向串口发送数据。
    
    :param ser: Serial 对象
    :param data: 要发送的数据
    """
    if ser is not None and ser.is_open:
        # 发送数据
        ser.write(data.encode('utf-8'))
        print(f"Send Data: {data.strip()}")
    else:
        print("Serial not open")

def ClassicReset(ser):
    """
    Classic reset sequence, sets DTR and RTS lines sequentially.
    """
    ser.dtr = False  # IO0=HIGH
    ser.rts = True  # EN=LOW, chip in reset
    time.sleep(0.1)
    ser.dtr = True  # IO0=LOW
    ser.rts = False  # EN=HIGH, chip out of reset
    # default time (0.05) to wait before releasing boot pin after reset
    time.sleep(0.05)
    ser.dtr = False  # IO0=HIGH, done

def HardReset(ser):
    """
    Reset sequence for hard resetting the chip.
    Can be used to reset out of the bootloader or to restart a running app.
    """
    ser.dtr = False
    ser.rts = True  # EN->LOW
    # Give the chip some time to come out of reset,
    # to be able to handle further DTR/RTS transitions
    time.sleep(0.2)
    ser.rts = False
    time.sleep(0.2)

def main():
    # 设置串口参数
    port = '/dev/ttyUSB0'  # 根据你的设备修改此值
    baudrate = 115200

    # 打开串口
    ser = open_serial(port, baudrate)

    if ser is not None:
        try:
            # 创建一个线程来读取串口数据
            stop_event = threading.Event()
            read_thread = threading.Thread(target=read_data_cb, args=(ser, stop_event))
            read_thread.start()

            # 发送数据，让ESP32进入boot mode
            print("--> ESP32 autoboot mode")
            ClassicReset(ser) # 设置DTR 和 RTS，将ESP32进入烧录模式
            time.sleep(3)

            # 发送数据，让ESP32复位
            print("--> ESP32 reset")
            HardReset(ser) # 设置RTS，复位ESP32
            time.sleep(2)

            # 主线程阻塞在这，等待用户输入
            while True:
                # 输入数据
                user_input = input("Please input send data:")
                if user_input.lower() in ['q', 'quit', 'exit']:
                    stop_event.set()   # 通知子线程结束
                    read_thread.join() # 等待子线程结束
                    break
                
                # 发送数据
                write_data(ser, user_input + '\n')
        except KeyboardInterrupt: # 接收用户按下 ctrl+c
            print("Program is interrupted(Ctrl+C) by user")
            # 优雅的结束子线程
            stop_event.set()   # 通知子线程结束
            read_thread.join() # 等待子线程结束
        finally:
            # 关闭串口
            ser.close()
            print(f"Port {port} close")

if __name__ == "__main__":
    main()
