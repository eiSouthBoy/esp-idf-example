import asyncio
import websockets
import signal
import sys
import getopt

async def test_websocket_server(host, port):
    uri = f"ws://{host}:{port}/ws"
    try:
        async with websockets.connect(uri) as websocket:
            print(f"websocket server connect successful, url: {uri}")
            
            while True:
                # 等待用户输入
                user_input = input("Please input message to send (or input 'exit' to quit): ")
                
                if user_input.lower() == 'exit':
                    print("start to exit...")
                    break
                
                # 发送用户输入的字符串
                await websocket.send(user_input)
                print(f"\033[1;93;49m Send \033[m: {user_input}")
                
                # 这里可以选择性地等待服务器的响应
                response = await websocket.recv()
                print(f"\033[1;92;49m Recv \033[m: {response}")
                
    except websockets.ConnectionClosedError:
        print("WebSocket connection closed")
    except Exception as e:
        print(f"get error: {e}")
    finally:
        # 关闭连接或执行其他清理工作
        print("client close...")

def main(host, port):
    loop = asyncio.get_event_loop()

    # handler signal Ctrl+C
    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, lambda: asyncio.create_task(shutdown(loop)))

    try:
        loop.run_until_complete(test_websocket_server(host, port))
    except KeyboardInterrupt:
        print("\nCapture Ctrl+C signal, start to exit...")
    finally:
        loop.close()

async def shutdown(loop):
    print("Recv stop signal, start to close signal loop...")
    tasks = [task for task in asyncio.all_tasks() if task is not
             asyncio.current_task()]
    [task.cancel() for task in tasks]
    results = await asyncio.gather(*tasks, return_exceptions=True)
    print(f'Nums of finished task: {len(results)}')
    loop.stop()

if __name__ == "__main__":
    try:
        opts, args = getopt.getopt(sys.argv[1:], "h:p:", ["host=", "port="])
    except getopt.GetoptError as err:
        print(str(err))
        sys.exit(2)

    host = None
    port = None

    for o, a in opts:
        if o in ("-h", "--host"):
            host = a
        elif o in ("-p", "--port"):
            try:
                port = int(a)
            except ValueError:
                print("port must is a int.")
                sys.exit(2)
        else:
            assert False, "unknown option."

    if host is None:
        print("Usage: {} <--host host>  [--port port]", args[0])
        sys.exit(2)
    if port is None:
        port = int("80")


    main(host, port)