#include "SocketImageSender.h"

SocketImageSender::SocketImageSender()
{
	imageRespository.read_position = 0;//初始化图像读取位置
	imageRespository.write_position = 0;//初始化图像写入位置
}

SocketImageSender::~SocketImageSender()
{}

int SocketImageSender::threadDelay(const long lTimeSec, const long lTimeUSec)
{
	timeval timeOut;  
    timeOut.tv_sec = lTimeSec;  
    timeOut.tv_usec = lTimeUSec;  
    if (0 != select(0, NULL, NULL, NULL, &timeOut))  
    {  
        return 1;  
    }  
    return 0;  
}

void SocketImageSender::produceImage(ImageRespository *ir, Mat image)
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

Mat SocketImageSender::consumeImage(ImageRespository *ir)
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

void SocketImageSender::openSocket(std::string server_address, int port)
{
	sClinet = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	memset(&addrServer,0,sizeof(addrServer));

	inet_pton(AF_INET, server_address.c_str(), &addrServer.sin_addr);
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(port);//连接端口

	connect(sClinet, (struct sockaddr*)&addrServer, sizeof(addrServer));//连接到服务器端

	std::cout<<"connect success"<<std::endl;
}

void SocketImageSender::closeSocket()
{
	close(sClinet);
}

void SocketImageSender::sendImage(Mat image)
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
	int tempSend=0;
	tempSend = send(sClinet, len.c_str(), strlen(len.c_str()), 0);
	if(tempSend<0)
	{
		throw "Socket is abnormal";
	}
	//分段发送编码
	char send_char[SENDBUFSIZE + 1];
	int temp = len_encode;
	int sended = 0;
	std::vector<uchar>::iterator b;
	int n=0;
	while (true)
	{
		n++;
		b = data_encode.begin() + sended;
		if (temp >= SENDBUFSIZE)
		{
			std::copy(b, b + SENDBUFSIZE, send_char);
			tempSend = send(sClinet, send_char, SENDBUFSIZE, 0);
			if (tempSend<0)
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
			if (tempSend<0)
			{
				throw "Socket is abnormal!";
			}
			//std::cout<<n<<" : "<<tempSend<<std::endl;
			sended += tempSend;
			//std::cout << sended << std::endl;
			break;
		}
	}
}

void SocketImageSender::consumerTask()
{
	while (true)
	{
		threadDelay(0,50);
		//sleep(1);
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
	}
}

void SocketImageSender::putImage(Mat image)
{
	produceImage(&imageRespository, image);
	threadDelay(0,50);
}

void SocketImageSender::start(std::string server_address,int port)
{
	openSocket(server_address,port);
	std::thread consumer(&SocketImageSender::consumerTask,this);
	consumer.join();
}
