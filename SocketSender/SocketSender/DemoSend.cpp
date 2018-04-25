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
	Mat image_buffer[imageRepositorySize];//ͼ�񻺳���
	size_t read_position;//�����߶�ȡ��Ʒλ��
	size_t write_position;//������д���Ʒλ��
	std::mutex mtx;//������������ͼ�񻺳���
	std::condition_variable repo_not_full;//����������ָʾͼ�񻺳�����Ϊ��
	std::condition_variable repo_not_empty;//����������ָʾͼ�񻺳�����Ϊ��
} imageRespository;//ͼ���ȫ�ֱ���, �����ߺ������߲����ñ���.

/*Socket��ض���*/
WSADATA wsaData;
SOCKET sClinet;//���������׽���
SOCKADDR_IN addrServer;//����˵�ַ
/*����ͼ����ض���*/
std::vector<uchar> data_encode;//ͼ�����

void sendImage(Mat image);

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

void consumerTask()//����������
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
	ir->read_position = 0;//��ʼ��ͼ���ȡλ��
	ir->write_position = 0;//��ʼ��ͼ��д��λ��

}

void openSocket(std::string server_address, int port)
{
	WSAStartup(MAKEWORD(2, 2), &wsaData);//Initialize Windows socket library

	sClinet = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);//AF_INETָ��ʹ��TCP/IPЭ���壻      
														//SOCK_STREAM, IPPROTO_TCP����ָ��ʹ��TCPЭ�� 

	inet_pton(AF_INET, "127.0.0.1", &addrServer.sin_addr.S_un.S_addr);
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(port);//���Ӷ˿�

	connect(sClinet, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));//���ӵ���������
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
	//���ͳ���
	send(sClinet, len.c_str(), strlen(len.c_str()), 0);
	//�ֶη��ͱ���
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
	
	//���ܷ�����Ϣ
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