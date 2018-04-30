#include "SocketImageReceiver.h"

void showImageTask(SocketImageReceiver *s)
{
	int num = 0;
	while (true)
	{
		Sleep(1);
		Mat image = (*s).getImage();
		imshow("Image", image);
		if (waitKey(30) == 27)
		{
			break;
		}
	}
}

int main()
{
	int port = 5150;
	SocketImageReceiver s;
	std::thread showImage(showImageTask, &s);

	s.start(port);
	

	//showImage.join();

	std::cin.get();

	return 0;
}