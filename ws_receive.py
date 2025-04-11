from websocket import create_connection
import sys
import traceback
ws = None
try:
  ws = create_connection("ws://eqis-1.local/ws")
  cnt = 0
  while True:
    recv = ws.recv()
    print(recv)
    cnt +=1
    if cnt == 20:
      break

except Exception as e:
  traceback.print_exc()
  print("except")
  print(e)

finally:
  if ws is not None:
    ws.close()
    print("connection close")
