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

#define RECVBUFSIZE 1000

using namespace cv;
static const int imageRepositorySize = 100;

struct ImageRespository {
	Mat image_buffer[imageRepositorySize];//图像缓冲区
	size_t read_position;//消费者读取产品位置
	size_t write_position;//生产者写入产品位置
	std::mutex mtx;//互斥锁，保护图像缓冲区
	std::condition_variable repo_not_full;//条件变量，指示图像缓冲区不为满
	std::condition_variable repo_not_empty;//条件变量，指示图像缓冲区不为空
} imageRespository;//图像库全局变量, 生产者和消费者操作该变量.

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

/*函数声明*/
Mat receiveImage();

void produceImage(ImageRespository *ir, Mat image)
{
	std::unique_lock<std::mutex> lock(ir->mtx);
	while (((ir->write_position + 1) % imageRepositorySize) == ir->read_position)
	{
		//图像缓冲区已满，等待
		(ir->repo_not_full).wait(lock);//生产者等待"图像库缓冲区不为满"这一条件发生.
	}
	(ir->image_buffer)[ir->write_position] = image;
	(ir->write_position)++;//写入位置后移

	if (ir->write_position == imageRepositorySize)// 写入位置若是在队列最后则重新设置为初始位置.
	{
		ir->write_position = 0;
	}
	(ir->repo_not_empty).notify_all();//通知消费者图像库不为空
	lock.unlock();//解锁
}

Mat consumeImage(ImageRespository *ir)
{
	Mat image;
	std::unique_lock<std::mutex> lock(ir->mtx);
	while (ir->write_position == ir->read_position)
	{
		//图像队列为空，等待
		(ir->repo_not_empty).wait(lock);//消费者等待"图像库缓冲区不为空"这一条件发生.
	}
	image = (ir->image_buffer)[ir->read_position];//读取图像
	(ir->read_position)++;

	if (ir->read_position >= imageRepositorySize)
	{
		ir->read_position = 0;
	}
	(ir->repo_not_full).notify_all();//通知消费者图像库不为满
	lock.unlock();//解锁

	return image;
}

void producerTask()//生产者任务
{
	Mat image;
	while (true)
	{
		try
		{
			image = receiveImage();
		}
		catch (const char* msg)
		{
			std::cout << msg << std::endl;
			break;
		}
		produceImage(&imageRespository, image);
	}
}

void consumerTask()//消费者任务
{
	int num = 0;
	while (true)
	{
		num++;
		Sleep(1);
		Mat image = consumeImage(&imageRespository);
		imshow("Image", image);
		if (waitKey(30) == 27)
		{
			break;
		}
	}
}

void openSocket(int port)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	sListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);//创建socket

	local.sin_family = AF_INET;//准备通信地址
	local.sin_port = htons(port);
	local.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	bind(sListen, (SOCKADDR *)&local, sizeof(SOCKADDR));//绑定

	listen(sListen, 5);

	int len = sizeof(SOCKADDR);
	sClient = accept(sListen, (SOCKADDR *)&client, &len);

	std::cout << "socket success" << std::endl;
}

Mat receiveImage()
{
	Mat img_decode;
	//double start = GetTickCount();
	if (recv(sClient, recvBuf, 16, 0) != INVALID_SOCKET)
	{
		for (int i = 0; i < 16; i++)
		{
			if (recvBuf[i]<'0' || recvBuf[i]>'9') recvBuf[i] = ' ';
		}
		int recvLen = atoi(recvBuf);
		data.resize(recvLen);
		//std::cout << recvLen << std::endl;
		int len = recvLen / RECVBUFSIZE + 1;
		//std::cout << "len: " << len << std::endl;
		int recved = 0;
		int tempRecv = 0;
		int n = 0;
		int m = 0;
		for (int i = 0; i < len - 1; i++)
		{
			n++;
			tempRecv = recv(sClient, recvBuf_1, RECVBUFSIZE, 0);
			if (tempRecv == SOCKET_ERROR)
			{
				throw "Socket is abnormal!";
			}
			if (tempRecv < 1000)
			{
				m++;
			}
			Sleep(1);
			//std::cout << tempRecv << std::endl;
			//std::cout << n << std::endl;
			//std::cout << n << " : " << tempRecv << std::endl;
			memcpy(&data[recved], recvBuf_1, tempRecv * sizeof(uchar));
			recved += tempRecv;
		}
		//double end = GetTickCount();
		//double cost = end - start;
		//std::cout << "时间: " << cost << std::endl;
		//最后一次接收
		if (recved < recvLen)
		{
			//std::cout << "最后一次: " << n << " : " << m << " " << recved << std::endl;
			tempRecv = recv(sClient, recvBuf_1, (atoi(recvBuf) - recved), 0);
			if (tempRecv == SOCKET_ERROR)
			{
				throw "Socket is abnormal!";
			}
			memcpy(&data[recved], recvBuf_1, tempRecv * sizeof(uchar));
			recved += tempRecv;
		}
		
		std::cout << atoi(recvBuf) << "->" << recved << std::endl;

		if (data.size() == 0)
		{
			throw "Data is abnormal!";
		}
		img_decode = imdecode(data, CV_LOAD_IMAGE_COLOR);
		if (img_decode.rows == 0)
		{
			throw "Image is abnormal!";
		}
		return img_decode;
	}
	else
	{
		throw "Socket is abnormal!";
	}

	
}

void closeSocket()
{
	closesocket(sListen);
	WSACleanup();
}

int main()
{
	std::cout << "Server" << std::endl;
	int port = 5150;
	openSocket(port);

	std::thread producer(producerTask);
	std::thread consumer(consumerTask);

	producer.join();
	consumer.join();

	std::cin.get();

	return 0;
}