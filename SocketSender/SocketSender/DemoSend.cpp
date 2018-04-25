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
#pragma comment(lib,"ws2_32.lib")

#define RECVBUFSIZE 32

#define SENDBUFSIZE 1000

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
SOCKET sClinet;//连接所用套节字
SOCKADDR_IN addrServer;//服务端地址
/*发送图像相关定义*/
std::vector<uchar> data_encode;//图像编码

void sendImage(Mat image);

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
	VideoCapture capture(0);
	Mat image;
	while (true)
	{
		//image = imread("F:\\GitHub\\FaceDetecte\\FaceDetecte\\1.jpg");
		if (!capture.read(image))
		{
			break;
		}
		produceImage(&imageRespository, image);
		Sleep(30);
		//break;
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
		try
		{
			sendImage(image);
		}
		catch (const char* msg)
		{
			std::cout << msg << std::endl;
			break;
		}
		std::cout << num << std::endl;
	}
}

void initImageRespository(ImageRespository *ir)
{
	ir->read_position = 0;//初始化图像读取位置
	ir->write_position = 0;//初始化图像写入位置

}

void openSocket(std::string server_address, int port)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);//Initialize Windows socket library

	sClinet = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);//AF_INET指明使用TCP/IP协议族；      
														//SOCK_STREAM, IPPROTO_TCP具体指明使用TCP协议 

	inet_pton(AF_INET, "127.0.0.1", &addrServer.sin_addr.S_un.S_addr);
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(port);//连接端口

	connect(sClinet, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));//连接到服务器端
}

void closeSocket()
{
	closesocket(sClinet);
	WSACleanup();
}

void sendImage(Mat image)
{
	imencode(".jpg", image, data_encode);
	int len_encode = data_encode.size();
	std::string len = std::to_string(len_encode);
	int length = len.length();
	for (int i = 0; i < 16 - length; i++)
	{
		len = len + " ";
	}
	//发送长度
	send(sClinet, len.c_str(), strlen(len.c_str()), 0);
	//分段发送编码
	char send_char[SENDBUFSIZE + 1];
	int temp = len_encode;
	int sended = 0;
	int tempSend = 0;
	std::vector<uchar>::iterator b;
	while (true)
	{
		b = data_encode.begin() + sended;
		if (temp >= SENDBUFSIZE)
		{
			std::copy(b, b + SENDBUFSIZE, send_char);
			tempSend = send(sClinet, send_char, SENDBUFSIZE, 0);
			if (tempSend == SOCKET_ERROR)
			{
				throw "Socket is abnormal!";
			}
			sended += tempSend;
			temp -= SENDBUFSIZE;
		}
		else if (temp == 0)
		{
			break;
		}
		else
		{
			std::copy(b, b + temp, send_char);
			tempSend = send(sClinet, send_char, temp, 0);
			if (tempSend == SOCKET_ERROR)
			{
				throw "Socket is abnormal!";
			}
			sended += tempSend;
			std::cout << sended << std::endl;
			break;
		}
	}
	
	//接受返回信息
	/*
	char recvBuf[RECVBUFSIZE];
	if (recv(sClinet, recvBuf, RECVBUFSIZE, 0) > 0)
	{
		printf("%s\n", recvBuf);
	}
	*/
}

int main()
{
	std::cout << "Client" << std::endl;
	std::string server_address = "127.0.0.1";
	int port = 5150;
	openSocket(server_address, port);

	std::thread producer(producerTask);
	std::thread consumer(consumerTask);

	producer.join();
	consumer.join();

	std::cin.get();

	return 0;
}