# coding: utf-8

import socket

hote = "192.168.11.176"
port = 8080

socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
socket.connect((hote, port))
print ("Connection on {}".format(port))
socket.send(b"on")

print ("Close")
socket.close()
