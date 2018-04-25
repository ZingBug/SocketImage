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

#define RECVBUFSIZE 1000

using namespace cv;
static const int imageRepositorySize = 100;

struct ImageRespository {
	Mat image_buffer[imageRepositorySize];//ͼ�񻺳���
	size_t read_position;//�����߶�ȡ��Ʒλ��
	size_t write_position;//������д���Ʒλ��
	std::mutex mtx;//������������ͼ�񻺳���
	std::condition_variable repo_not_full;//����������ָʾͼ�񻺳�����Ϊ��
	std::condition_variable repo_not_empty;//����������ָʾͼ�񻺳�����Ϊ��
} imageRespository;//ͼ���ȫ�ֱ���, �����ߺ������߲����ñ���.

/*Socket��ض���*/
WSADATA wsaData;
SOCKET sListen;
SOCKET sClient;
SOCKADDR_IN local;
SOCKADDR_IN client;
/*����ͼ����ض���*/
char recvBuf[16];
char recvBuf_1[RECVBUFSIZE];
std::vector<uchar> data;

/*��������*/
Mat receiveImage();

void produceImage(ImageRespository *ir, Mat image)
{
	std::unique_lock<std::mutex> lock(ir->mtx);
	while (((ir->write_position + 1) % imageRepositorySize) == ir->read_position)
	{
		//ͼ�񻺳����������ȴ�
		(ir->repo_not_full).wait(lock);//�����ߵȴ�"ͼ��⻺������Ϊ��"��һ��������.
	}
	(ir->image_buffer)[ir->write_position] = image;
	(ir->write_position)++;//д��λ�ú���

	if (ir->write_position == imageRepositorySize)// д��λ�������ڶ����������������Ϊ��ʼλ��.
	{
		ir->write_position = 0;
	}
	(ir->repo_not_empty).notify_all();//֪ͨ������ͼ��ⲻΪ��
	lock.unlock();//����
}

Mat consumeImage(ImageRespository *ir)
{
	Mat image;
	std::unique_lock<std::mutex> lock(ir->mtx);
	while (ir->write_position == ir->read_position)
	{
		//ͼ�����Ϊ�գ��ȴ�
		(ir->repo_not_empty).wait(lock);//�����ߵȴ�"ͼ��⻺������Ϊ��"��һ��������.
	}
	image = (ir->image_buffer)[ir->read_position];//��ȡͼ��
	(ir->read_position)++;

	if (ir->read_position >= imageRepositorySize)
	{
		ir->read_position = 0;
	}
	(ir->repo_not_full).notify_all();//֪ͨ������ͼ��ⲻΪ��
	lock.unlock();//����

	return image;
}

void producerTask()//����������
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

void consumerTask()//����������
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

	sListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);//����socket

	local.sin_family = AF_INET;//׼��ͨ�ŵ�ַ
	local.sin_port = htons(port);
	local.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	bind(sListen, (SOCKADDR *)&local, sizeof(SOCKADDR));//��

	listen(sListen, 5);

	int len = sizeof(SOCKADDR);
	sClient = accept(sListen, (SOCKADDR *)&client, &len);
}

Mat receiveImage()
{
	Mat img_decode;
	if (recv(sClient, recvBuf, 16, 0) != INVALID_SOCKET)
	{
		for (int i = 0; i < 16; i++)
		{
			if (recvBuf[i]<'0' || recvBuf[i]>'9') recvBuf[i] = ' ';
		}
		data.resize(atoi(recvBuf));
		//std::cout << atoi(recvBuf) << std::endl;
		int len = atoi(recvBuf) / RECVBUFSIZE + 1;
		int recved = 0;
		int tempRecv = 0;

		for (int i = 0; i < len - 1; i++)
		{
			tempRecv = recv(sClient, recvBuf_1, RECVBUFSIZE, 0);
			if (tempRecv == SOCKET_ERROR)
			{
				throw "Socket is abnormal!";
			}
			memcpy(&data[recved], recvBuf_1, tempRecv * sizeof(uchar));
			recved += tempRecv;
		}
		//���һ�ν���
		if (recved < atoi(recvBuf))
		{
			tempRecv = recv(sClient, recvBuf_1, (atoi(recvBuf) - recved + 1), 0);
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