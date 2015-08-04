import socket
import sys


if __name__ == '__main__':

	if len(sys.argv) != 3:
		print "[*] Usage: python %s <ipaddr> <port>" % sys.argv[0]
	else:
		target_host = sys.argv[1]
		target_port = int(sys.argv[2])

		client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		client.connect((target_host, target_port))
		client.send("OPTIONS / HTTP/1.1\r\nHost: " + target_host + "\r\n\r\n")
		response = client.recv(4096)
		print response
