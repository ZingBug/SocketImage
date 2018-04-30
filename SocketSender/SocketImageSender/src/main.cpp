#include "SocketImageSender.h"

void readImage(SocketImageSender *s)
{
	VideoCapture capture("1.mp4");
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
	std::string server_address="192.168.2.238";
	int port=5150;

	SocketImageSender s;
	std::thread read(readImage,&s);
	s.start(server_address,port);

	return 0;
}