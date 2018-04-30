#ifndef SOCKETIMAGERECEIVER_H
#define SOCKETIMAGERECEIVER_H

#include <iostream>
#include <opencv2/opencv.hpp>
#include <string>
#include <WinSock2.h>
#include <vector>
#include <stdio.h>
#include <WS2tcpip.h>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <time.h> 
#include <windows.h>  
#pragma comment(lib,"ws2_32.lib")

#define IMAGE_REPOSITORY_SIZE 100
#define RECVBUFSIZE 1000

using namespace cv;

struct ImageRespository {
	Mat image_buffer[IMAGE_REPOSITORY_SIZE];//图像缓冲区
	size_t read_position;//消费者读取产品位置
	size_t write_position;//生产者写入产品位置
	std::mutex mtx;//互斥锁，保护图像缓冲区
	std::condition_variable repo_not_full;//条件变量，指示图像缓冲区不为满
	std::condition_variable repo_not_empty;//条件变量，指示图像缓冲区不为空
};



class SocketImageReceiver
{
public:
	SocketImageReceiver();
	~SocketImageReceiver();
	void start(int port);
	Mat getImage();

private:
	void openSocket(int port);
	void closeSocket();
	void producerTask();
	Mat receiveImage();
	/*生产者消费者设计模式*/
	ImageRespository imageRespository;//图像库全局变量, 生产者和消费者操作该变量.
	void produceImage(ImageRespository *ir, Mat image);
	Mat consumeImage(ImageRespository *ir);
	/*Socket相关定义*/
	WSADATA wsaData;
	SOCKET sListen;
	SOCKET sClient;
	SOCKADDR_IN local;
	SOCKADDR_IN client;
	/*接受图像相关定义*/
	char recvBuf[16];
	char recvBuf_1[RECVBUFSIZE];
	std::vector<uchar> data;
};

#endif // !SOCKETIMAGERECEIVER_H

