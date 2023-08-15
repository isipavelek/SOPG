import socket
import threading
import time
import os

HOST = "127.0.0.1"
PORT = 4040  
states=[0,0,0]

def printStates(states):
    os.system('clear')
    print("Emulador Hardware . Salidas:")
    print("OUT 1:%d - OUT 2:%d - OUT 3:%d" %(states[0],states[1],states[2]))
    print("Botones:")
    print("1) Presiono boton 1")
    print("2) Presiono boton 2")
    print("3) Presiono boton 3")
    print("Elija opcion:")

def rcvThread(sock,states):
	global socketOk
	print("INICIO thread recepcion")
	while True:
		data = sock.recv(128)
		if len(data)==0:
			print("Se cerro la conexion")
			break
		print("LLEGO:"+data.decode("utf-8"))
		data = data.decode("utf-8").split("OUT:")
		for d in data:
			if len(d)>=3:
				inNumber = d[0]
				inValue = d[2]
				states[int(inNumber)]=int(inValue)
				printStates(states)
				
	print("FIN thread recepcion")
	socketOk=False
	os._exit(1)

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.bind((HOST, PORT))
    s.listen()
    while True:
        conn, addr = s.accept()
        with conn:
            print(f"Connected by {addr}")
            # Creo thread para escuchar paquetes
            t = threading.Thread(target=rcvThread, args=(conn,states,))
            t.daemon=True
            t.start()        
            while True:
                printStates(states)
                opt = input("")
                if opt=="1":
                    if(states[0]): 
                        states[0]=False 
                    else: 
                        states[0]=True            	
                    conn.sendall((">SW:0,%d\r\n" % (states[0])).encode("utf-8"))
                if opt=="2":
                    if(states[1]): 
                        states[1]=False 
                    else: 
                        states[1]=True            	
                    conn.sendall((">SW:1,%d\r\n" % (states[1])).encode("utf-8"))
                if opt=="3":
                    if(states[2]): 
                        states[2]=False 
                    else: 
                        states[2]=True            	
                    conn.sendall((">SW:2,%d\r\n" % (states[2])).encode("utf-8"))
