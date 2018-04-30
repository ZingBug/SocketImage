#include "SocketImageReceiver.h"

SocketImageReceiver::SocketImageReceiver()
{
	imageRespository.read_position = 0;//初始化图像读取位置
	imageRespository.write_position = 0;//初始化图像写入位置
}

SocketImageReceiver::~SocketImageReceiver()
{
}

void SocketImageReceiver::openSocket(int port)
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

void SocketImageReceiver::closeSocket()
{
	closesocket(sListen);
	WSACleanup();
}

Mat SocketImageReceiver::receiveImage()
{
	Mat img_decode;
	
	
	if (recv(sClient, recvBuf, 16, 0) != INVALID_SOCKET)
	{
		
		for (int i = 0; i < 16; i++)
		{
			if (recvBuf[i]<'0' || recvBuf[i]>'9') recvBuf[i] = ' ';
		}
		int recvLen = atoi(recvBuf);
		data.resize(recvLen);

		int len = recvLen / RECVBUFSIZE + 1;
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
			
			memcpy(&data[recved], recvBuf_1, tempRecv * sizeof(uchar));
			recved += tempRecv;
		}
	
		//最后一次接收
		if (recved < recvLen)
		{
			
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

void SocketImageReceiver::producerTask()
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

void SocketImageReceiver::start(int port)
{
	openSocket(port);

	//producerTask();

	std::thread producer(&SocketImageReceiver::producerTask, this);

	producer.join();
}

Mat SocketImageReceiver::getImage()
{
	Mat image = consumeImage(&imageRespository);
	return image;
}

void SocketImageReceiver::produceImage(ImageRespository *ir, Mat image)
{
	std::unique_lock<std::mutex> lock(ir->mtx);
	while (((ir->write_position + 1) % IMAGE_REPOSITORY_SIZE) == ir->read_position)
	{
		//图像缓冲区已满，等待
		(ir->repo_not_full).wait(lock);//生产者等待"图像库缓冲区不为满"这一条件发生.
	}
	(ir->image_buffer)[ir->write_position] = image;
	(ir->write_position)++;//写入位置后移

	if (ir->write_position == IMAGE_REPOSITORY_SIZE)// 写入位置若是在队列最后则重新设置为初始位置.
	{
		ir->write_position = 0;
	}
	(ir->repo_not_empty).notify_all();//通知消费者图像库不为空
	lock.unlock();//解锁
}

Mat SocketImageReceiver::consumeImage(ImageRespository *ir)
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

	if (ir->read_position >= IMAGE_REPOSITORY_SIZE)
	{
		ir->read_position = 0;
	}
	(ir->repo_not_full).notify_all();//通知消费者图像库不为满
	lock.unlock();//解锁

	return image;
}