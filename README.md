# onlien_translate_english_to_chinese
这是一个python+c语言的在线翻译c/s客户端
利用的技术：sqlite数据库，socket套接字，tcp协议，python爬虫，多线程

Linux：
1.服务器端下载python
sudo apt install python3

2.服务器端下载sqlite数据库
sudo apt install sqlite3

额外的：没有gcc记得下载
sudo apt install gcc

3.编译服务端和客户端
gcc -o server server.c -lsqlite3
gcc -o client client.c

4.运行服务端
./server 8888

5.运行客户端
./client 127.0.0.1 8888
中间的ip是服务端的ip，请自行查看自己服务端的ip

备注：
如果客户端和服务端不是同一台机器测试，那么请打开你选择的端口，我这里是8888
那么要：
sudo iptables -A INPUT -p tcp --dport 8888 -j ACCEPT
