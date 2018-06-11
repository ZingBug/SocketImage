#ifndef SOCKETIMAGESENDER_H
#define SOCKETIMAGESENDER_H

#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <errno.h>  
#include <sys/types.h>  
#include <sys/socket.h>  
#include <netinet/in.h>
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <mutex>
//#include <pthread.h>
#include <condition_variable>
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <thread>
#include <unistd.h>
#include <sys/select.h> 

#define IMAGE_REPOSITORY_SIZE 5
#define SENDBUFSIZE 3000

using namespace cv;

struct ImageRespository {
	Mat image_buffer[IMAGE_REPOSITORY_SIZE];//图像缓冲区
	size_t read_position;//消费者读取产品位置
	size_t write_position;//生产者写入产品位置
	std::mutex mtx;//互斥锁，保护图像缓冲区
	std::condition_variable repo_not_full;//条件变量，指示图像缓冲区不为满
	std::condition_variable repo_not_empty;//条件变量，指示图像缓冲区不为空
};

class SocketImageSender
{
public:
	SocketImageSender();
	~SocketImageSender();
	void start(std::string server_address,int port);
	void putImage(Mat image);
private:
	int threadDelay(const long lTimeSec, const long lTimeUSec);
	void produceImage(ImageRespository *ir, Mat image);
	Mat consumeImage(ImageRespository *ir);
	void openSocket(std::string server_address, int port);
	void closeSocket();
	void sendImage(Mat image);
	void consumerTask();//消费者任务
	//static void * SocketImageSender::consumerTask(void *arg);

	ImageRespository imageRespository;
	int sClinet;
	struct sockaddr_in addrServer;//服务端地址
	std::vector<uchar> data_encode;//图像编码

};

#endif