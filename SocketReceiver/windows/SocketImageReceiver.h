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
	Mat image_buffer[IMAGE_REPOSITORY_SIZE];//ͼ�񻺳���
	size_t read_position;//�����߶�ȡ��Ʒλ��
	size_t write_position;//������д���Ʒλ��
	std::mutex mtx;//������������ͼ�񻺳���
	std::condition_variable repo_not_full;//����������ָʾͼ�񻺳�����Ϊ��
	std::condition_variable repo_not_empty;//����������ָʾͼ�񻺳�����Ϊ��
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
	/*���������������ģʽ*/
	ImageRespository imageRespository;//ͼ���ȫ�ֱ���, �����ߺ������߲����ñ���.
	void produceImage(ImageRespository *ir, Mat image);
	Mat consumeImage(ImageRespository *ir);
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
};

#endif // !SOCKETIMAGERECEIVER_H

