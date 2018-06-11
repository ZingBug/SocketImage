#include "SocketImageReceiver.h"
#include <highgui.h>
#include <core.hpp>

void showImageTask(SocketImageReceiver *s)
{
	int num = 0;
	while (true)
	{
		Sleep(1);
		Mat image = (*s).getImage();
		imshow("Image", image);
		if (waitKey(10) == 27)
		{
			break;
		}
	}
}

int main()
{
	/*
	Mat image1 = imread("C:\\Users\\lzh9619\\source\\repos\\SocketReceiver\\1.jpg");
	Mat image2 = imread("C:\\Users\\lzh9619\\source\\repos\\SocketReceiver\\2.jpg");
	Size size(image1.rows + image1.rows, MAX(image1.cols, image2.cols));

	Mat img_merge;
	img_merge.create(size, CV_MAKETYPE(image1.depth(), 3));

	img_merge = Scalar::all(0);
	Mat outImg_left, outImg_right;
	outImg_left = img_merge(Rect(0, 0, image1.cols, image1.rows));
	outImg_right = img_merge(Rect(image1.cols, 0, image1.cols, image1.rows));
	image1.copyTo(outImg_left);
	image2.copyTo(outImg_right);
	imshow("image1", img_merge);
	waitKey();
	*/
	
	
	int port = 5150;
	SocketImageReceiver s;
	std::thread showImage(showImageTask, &s);

	s.start(port);
	

	//showImage.join();
	
	std::cin.get();

	return 0;
}