#include "SocketImageSender.h"

void readImage(SocketImageSender *s)
{
	std::string path="//home//lzh//Desktop//SocketImage//SocketImageSender//1.mp4";
	VideoCapture capture(3);
	std::cout<<"video start"<<std::endl;
	Mat image;
	while (true)
	{
		
		if (!capture.read(image))
		{
			std::cout<<"video end"<<std::endl;
			break;
		}
		
		(*s).putImage(image);
	}
}

int main(int argc,char** argv)
{
	std::string server_address="10.193.1.62";
	int port=5150;

	SocketImageSender s;
	std::thread read(readImage,&s);
	s.start(server_address,port);

	std::cout<<"test"<<std::endl;

	return 0;
}