#include "SocketImageReceiver.h"

SocketImageReceiver::SocketImageReceiver()
{
	imageRespository.read_position = 0;//��ʼ��ͼ���ȡλ��
	imageRespository.write_position = 0;//��ʼ��ͼ��д��λ��
}

SocketImageReceiver::~SocketImageReceiver()
{
}

void SocketImageReceiver::openSocket(int port)
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
	
		//���һ�ν���
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
		//ͼ�񻺳����������ȴ�
		(ir->repo_not_full).wait(lock);//�����ߵȴ�"ͼ��⻺������Ϊ��"��һ��������.
	}
	(ir->image_buffer)[ir->write_position] = image;
	(ir->write_position)++;//д��λ�ú���

	if (ir->write_position == IMAGE_REPOSITORY_SIZE)// д��λ�������ڶ����������������Ϊ��ʼλ��.
	{
		ir->write_position = 0;
	}
	(ir->repo_not_empty).notify_all();//֪ͨ������ͼ��ⲻΪ��
	lock.unlock();//����
}

Mat SocketImageReceiver::consumeImage(ImageRespository *ir)
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

	if (ir->read_position >= IMAGE_REPOSITORY_SIZE)
	{
		ir->read_position = 0;
	}
	(ir->repo_not_full).notify_all();//֪ͨ������ͼ��ⲻΪ��
	lock.unlock();//����

	return image;
}