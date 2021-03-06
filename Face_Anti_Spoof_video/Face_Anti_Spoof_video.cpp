
#include "stdafx.h"
#include <iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include "opencv\cv.h"
#include "opencv2\core\core.hpp"
#include "opencv2\highgui\highgui.hpp"
#include "opencv2\imgproc\imgproc.hpp"

using namespace std;
using namespace cv;
using namespace cv::dnn;

//人脸检测用的是SSD，因此需要将图片resize到300*300，如果替换方法，此处不必要
const size_t inWidth = 300;
const size_t inHeight = 300;
const double inScaleFactor = 1.0;
const Scalar meanVal(104.0, 177.0, 123.0);//人脸检测时所用均值
const Scalar meanCla(90.0, 198.0, 121.0);//分类网络所用均值

int main()
{
	//分类网络的输入需要resize到227*227 
	int inpWidth = 227;
	int inpHeight = 227;
	//人脸标签
	std::vector<std::string> classes = { "false", "true" };
	//Initialize Detection network(SSD)
	float min_confidence = 0.5;
	String modelConfiguration = "deploy.prototxt";
	String modelBinary = "res10_300x300_ssd_iter_140000.caffemodel";
	dnn::Net net = readNetFromCaffe(modelConfiguration, modelBinary);
	//Classification Model(SqueeseNet_v1.1)
	String  Classification_model = "deploy_Squeeze.prototxt";
	String  Classification_weights = "train_add_data_iter_100000.caffemodel";//40000,60000,80000,100000
	//Initialize Classification network
	Net net_classification = readNetFromCaffe(Classification_model, Classification_weights);
	//Object Detection
	VideoCapture cap(0);
	//VideoCapture cap("I:\\siw\\sample\\Test\\spoof\\002\\002-1-2-2-1.mov");

	if (!cap.isOpened())
	{
		cout << "Couldn't open camera : " << endl;
		return -1;
	}
	else
	{
		for (;;)
		{
			Mat img;
			cap >> img;
			Mat inputBlob = blobFromImage(img, inScaleFactor, Size(inWidth, inHeight), meanVal, false, false);
			net.setInput(inputBlob, "data");	// set the network input
			Mat detection = net.forward("detection_out");
			Mat detectionMat(detection.size[2], detection.size[3], CV_32F, detection.ptr<float>());
			float confidenceThreshold = min_confidence;
			for (int j = 0; j < detectionMat.rows; ++j)
			{
				float confidence = detectionMat.at<float>(j, 2);
				if (confidence > confidenceThreshold)
				{
					int xLeftBottom = static_cast<int>(detectionMat.at<float>(j, 3) * img.cols);
					int yLeftBottom = static_cast<int>(detectionMat.at<float>(j, 4) * img.rows);
					int xRightTop = static_cast<int>(detectionMat.at<float>(j, 5) * img.cols);
					int yRightTop = static_cast<int>(detectionMat.at<float>(j, 6) * img.rows);

					//将人脸区域扩张,长宽分别变为原来的1.8倍：上下扩张0.4倍的高，左右扩张0.4倍的宽
					int xLeftBottom_new = xLeftBottom - 0.4*(xRightTop - xLeftBottom);
					int yLeftBottom_new = yLeftBottom - 0.4*(yRightTop - yLeftBottom);
					if (xLeftBottom_new <= 0)
						xLeftBottom_new = 0;
					if (yLeftBottom_new <= 0)
						yLeftBottom_new = 0;
					int xRightTop_new = xRightTop + 0.4*(xRightTop - xLeftBottom);
					int yRightTop_new = yRightTop + 0.4*(yRightTop - yLeftBottom);
					if (xRightTop_new > img.cols)
						xRightTop_new = img.cols;
					if (yRightTop_new > img.rows)
						yRightTop_new = img.rows;

					if (xLeftBottom <= 0)
						xLeftBottom = 0;
					if (yLeftBottom <= 0)
						yLeftBottom = 0;
					if (xRightTop > img.cols)
						xRightTop = img.cols;
					if (yRightTop > img.rows)
						yRightTop = img.rows;
					Rect object((int)xLeftBottom, (int)yLeftBottom, (int)(xRightTop - xLeftBottom), (int)(yRightTop - yLeftBottom));//original 
					Rect object_new((int)xLeftBottom_new, (int)yLeftBottom_new, (int)(xRightTop_new - xLeftBottom_new), (int)(yRightTop_new - yLeftBottom_new));//expand
					Mat roi = img(object_new);
					Mat roi_ori = img(object);
					//Classification
					resize(roi_ori, roi_ori, cv::Size(227, 227));
					resize(roi, roi, cv::Size(227, 227));
					Mat blob, blob_ori;
					blobFromImage(roi, blob, inScaleFactor, Size(inpWidth, inpHeight), meanCla, false, false);
					blobFromImage(roi_ori, blob_ori, inScaleFactor, Size(inpWidth, inpHeight), meanCla, false, false);

					net_classification.setInput(blob);
					Mat prob = net_classification.forward();
					Point classIdPoint;
					double confidence;
					minMaxLoc(prob.reshape(1, 1), 0, &confidence, 0, &classIdPoint);
					int classId = classIdPoint.x;
					std::vector<double> layersTimes;
					double freq = getTickFrequency() / 1000;
					double t = net_classification.getPerfProfile(layersTimes) / freq;

					net_classification.setInput(blob_ori);
					Mat prob_ori = net_classification.forward();
					Point classIdPoint_ori;
					double confidence_ori;
					minMaxLoc(prob_ori.reshape(1, 1), 0, &confidence_ori, 0, &classIdPoint_ori);
					int classId_ori = classIdPoint_ori.x;
					//计算分类所用时间
					std::vector<double> layersTimes_ori;
					double freq_ori = getTickFrequency() / 1000;
					double t_ori = net_classification.getPerfProfile(layersTimes_ori) / freq_ori;

					std::string label = format("Inference time: %.2f ms", t + t_ori);
					putText(img, label, Point(0, 15), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 0, 255));
					std::cout << label << std::endl;
					
						if (classes[classId] == "true" && classes[classId_ori] == "true")
						{
							label = format("%s: %.4f", (classes.empty() ? format("Class #%d", classId).c_str() : classes[classId].c_str()), (confidence + confidence_ori) / 2);
							putText(img, label, Point(xLeftBottom, yLeftBottom), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 255, 0));
							rectangle(img, object, Scalar(0, 255, 0));
						}
						else if (classes[classId] == "true" && classes[classId_ori] == "false")
						{
							if (confidence > confidence_ori && confidence > 0.95) {
								label = format("%s: %.4f", (classes.empty() ? format("Class #%d", classId).c_str() : classes[classId].c_str()), confidence);
								putText(img, label, Point(xLeftBottom, yLeftBottom), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 255, 0));
								rectangle(img, object, Scalar(0, 255, 0));
							}
							else
							{
								label = format("%s: %.4f", (classes.empty() ? format("Class #%d", classId_ori).c_str() : classes[classId_ori].c_str()), confidence_ori);
								putText(img, label, Point(xLeftBottom, yLeftBottom), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 0, 255));
								rectangle(img, object, Scalar(0, 0, 255));
							}

						}
						else if (classes[classId] == "false" && classes[classId_ori] == "true")
						{
							if (confidence_ori > confidence && confidence_ori > 0.95) {
								label = format("%s: %.4f", (classes.empty() ? format("Class #%d", classId_ori).c_str() : classes[classId_ori].c_str()), confidence_ori);
								putText(img, label, Point(xLeftBottom, yLeftBottom), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 255, 0));
								rectangle(img, object, Scalar(0, 255, 0));
							}
							else
							{
								label = format("%s: %.4f", (classes.empty() ? format("Class #%d", classId).c_str() : classes[classId].c_str()), confidence);
								putText(img, label, Point(xLeftBottom, yLeftBottom), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 0, 255));
								rectangle(img, object, Scalar(0, 0, 255));
							}

						}
						else
						{
							label = format("%s: %.4f", (classes.empty() ? format("Class #%d", classId).c_str() : classes[classId].c_str()), (confidence + confidence_ori) / 2);
							putText(img, label, Point(xLeftBottom, yLeftBottom), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 0, 255));
							rectangle(img, object, Scalar(0, 0, 255));
						}
						namedWindow("Detected", 1);
						imshow("Detected", img);
						if (waitKey(1) >= 0) break;
					}
				}
		}
		}
					
	
	

	return 0;

}

