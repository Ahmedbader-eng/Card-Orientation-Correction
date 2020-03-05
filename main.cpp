// impot the required libraries
#include <opencv2\opencv.hpp>
#include <iostream>
#include <opencv2/xfeatures2d.hpp>
#include "stdafx.h"
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <iostream>

//writeHandler. Called when asynchronous write is complete.
void writeHandler(const boost::system::error_code &e, std::size_t bytes_transferred) {
	std::cout << "writeHandler " << bytes_transferred << " written" << std::endl;
}

//readHandler. Called when asynchronous read is complete.
void readHandler(const boost::system::error_code& error, std::size_t bytes_transferred) {
	std::cout << "readHandler " << bytes_transferred << " read" << std::endl;
}

// create a function to check if rotation is require to correct the card orientation and the function returns an instruction charachter
char check_rotation(cv::Mat input, char side_status) {

	cv::Mat image_1;

	if (side_status == 'Y') {
		// read a reference image which is at the correct position
		image_1 = cv::imread("data/back image.jpg");
	}
	else {
		// read a reference image which is at the correct position
		image_1 = cv::imread("data/front image.jpg");
	}
	// read the input image which is captured by the camera when a new card is inserted under the camera
	cv::Mat image_2 = input;

	// Success if the reference image (image_1) and the input image (image_2) exist
	if (image_1.empty() || image_2.empty()) {
		std::cout << "Missing Images" << std::endl;
		return -1;
	}

	// convert the images to gray scale to detect the fetures better
	cv::cvtColor(image_1, image_1, cv::COLOR_RGB2GRAY);
	cv::cvtColor(image_2, image_2, cv::COLOR_RGB2GRAY);

	// detector is a SIFT object created to detect features from both images
	cv::Ptr<cv::xfeatures2d::SIFT> detector = cv::xfeatures2d::SIFT::create();

	// create vectors to store the detected keypoints locations of the images
	std::vector<cv::KeyPoint> keypoints1, keypoints2;

	// create Mat objects to store the keypoints description
	// keypoints only provide a location. Therefore, descriptors are used to describe the keypoints which will
	// identify how different or similar one keypoint to the other
	cv::Mat descriptors1, descriptors2;

	// the member function detectAndCompute do two things
	// * detect keypoints of an image by passing the image and a mask. The detected points are stored in the keypoints vector
	// * compute the descriptors from the detected keypoints
	detector->detectAndCompute(image_1, cv::Mat(), keypoints1, descriptors1);
	detector->detectAndCompute(image_2, cv::Mat(), keypoints2, descriptors2);

	// create a brute force descriptor matcher object to match the keypoints description between the two images
	cv::Ptr<cv::DescriptorMatcher> matcher = cv::DescriptorMatcher::create(cv::DescriptorMatcher::BRUTEFORCE);

	// a vector of DMatch objects to store the description when two descriptors matches
	std::vector<cv::DMatch> matches;

	// Match member function uses brute force to match between the query descriptor (reference image
	// descriptor1) and the train descriptor (input image descriptor2)
	// To match the function uses one feature descriptor in the reference image and match it with
	// all other feature descriptors in the input image using a distance measures
	matcher->match(descriptors1, descriptors2, matches);

	// Due to noise some points will be seen matched because they are close to a feature
	// Thus, the vector matches will contain some incorrect matches 
	// a vector of DMatch objects to filter the matches found previously and only keep good matches
	std::vector<cv::DMatch> good_matches;

	// loop through the matches and filter the second match if it is smaller than the first match by a given ratio
	/// for my set up a ratio of 0.38 worked best
	for (int i = 0; i < matches.size(); ++i)
	{
		const float ratio = 0.6f; // As in Lowe's paper; can be tuned


		if (matches[i].distance < ratio * matches[i + 1].distance)
		{
			// store the good matches in the good_matches vector
			good_matches.push_back(matches[i]);
		}
	}

	// create a Mat object to draw and display the matching between the reference and the input images
	cv::Mat output_image;

	// Draw the found matches between the two images
	// DrawMatches takes the good_matches vector as input which tells the corresponding points that matches 
	// between the reference and input image.
	// The matching line is drawn in red and the single points will not be drawn as the flag is set to NOT_DRAW_SINGLE_POINTS
	// The mask is set to draw all the matches
	cv::drawMatches(image_1, keypoints1, image_2, keypoints2, good_matches,
		output_image, cv::Scalar(0, 0, 255), cv::Scalar(0, 0, 255),
		std::vector<char>(), cv::DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS);


	// vectors to localize the object by utilising the matched points
	std::vector<cv::Point2f> image_1_src;
	std::vector<cv::Point2f> image_2_dis;

	std::cout << "Good matches size: " << good_matches.size() << std::endl;

	// Three good matched points are enough to calculate the affain transform which will determine if the input image is rotated
	if (good_matches.size() >= 3) {
		for (int i = 0; i < 3; i++) {
			// the attribute pt returns the coordinate of the keypoint
			// good_matches have points from the reference image and the input image. Thus, we can specify which one to read and then store them in different vectors
			image_1_src.push_back(keypoints1[good_matches[i].queryIdx].pt);
			image_2_dis.push_back(keypoints2[good_matches[i].trainIdx].pt);
		}
	}
	// if there are no matches an error signal will be sent to the microcontroller to reject the card
	else {
		//capital E means an error has occurred
		std::cout << "Not enough good matches : REJECTED" << std::endl;
		return 'E';
	}

	// Calculate the affain transformation which provide information about the rotation and translation of the input image
	cv::Mat affainMat = cv::getAffineTransform(image_1_src, image_2_dis);

	// create a Mat object to display the result which represent the new calculated orientation
	cv::Mat result;
	// Make the result object the same size and type as the input image
	result.create(image_2.size(), image_2.type());

	// Apply the transformation on the input image and store it in the result Mat objec
	cv::warpAffine(image_2, result, affainMat, result.size());

	// Read the values of the horizontal axis to check if a rotation has occurred  
	double at0_1 = affainMat.at<double>(0, 1);
	double at1_0 = affainMat.at<double>(1, 0);


	// show the matchigng between the reference image and the input image in the Matching window
	cv::imshow("Matching", output_image);


	if (abs(at0_1) <= 1.3 || abs(at1_0) <= 1.3) {
		if (at0_1 <= 0 && at1_0 >= 0) {
			// Returns L if left rotation (anti-clockwise) is neededd
			return 'L';
		}
		else if (at0_1 >= 0 && at1_0 <= 0) {
			// Returns R if right rotation (clockwise) is neededd
			return 'R';
		}
		else {
			// reject the card
			std::cout << "REJECTED: NOT INSERTED PROPERLY" << std::endl;
			return 'E';
		}
	}
	/// absolute values bigger than 1.3 are alsoan error because this result in bad transformation depending on the quality of the captured frame
	else {
		// reject the card
		std::cout << "REJECTED: NOT INSERTED PROPERLY / BAD IMAGE" << std::endl;
		return 'E';
	}



}

// create a function to check if the card is on the correct side and the function returns an instruction charachter
char check_side(cv::Mat input) {
	// read a reference image which is at the logo to check the side of the card
	cv::Mat image_1 = cv::imread("data/reference image logo.jpg");
	// read the input image which is captured by the camera when a new card is inserted under the camera
	cv::Mat image_2 = input;

	// Success if the reference image (image_1) and the input image (image_2) exist
	if (image_1.empty() || image_2.empty()) {
		std::cout << "Missing Images" << std::endl;
		return -1;
	}

	cv::cvtColor(image_1, image_1, cv::COLOR_RGB2GRAY);
	cv::cvtColor(image_2, image_2, cv::COLOR_RGB2GRAY);

	// The same detection and matching process in the check_rotation(cv::Mat input) function is applied to check the side of the card
	cv::Ptr<cv::xfeatures2d::SIFT> detector = cv::xfeatures2d::SIFT::create();


	std::vector<cv::KeyPoint> keypoints1, keypoints2;
	cv::Mat descriptors1, descriptors2;

	detector->detectAndCompute(image_1, cv::Mat(), keypoints1, descriptors1);
	detector->detectAndCompute(image_2, cv::Mat(), keypoints2, descriptors2);

	cv::Ptr<cv::DescriptorMatcher> matcher = cv::DescriptorMatcher::create(cv::DescriptorMatcher::BRUTEFORCE);

	std::vector<cv::DMatch> matches;

	matcher->match(descriptors1, descriptors2, matches);

	std::vector<cv::DMatch> good_matches;

	// Because the camera is not able to detect many features at the grooved logo side of the card the ratio is increased.
	/// for my setup a ratio of 0.6 provided the best results
	for (int i = 0; i < matches.size(); ++i)
	{
		const float ratio = 0.7f;
		if (matches[i].distance < ratio * matches[i + 1].distance)
		{
			good_matches.push_back(matches[i]);
		}
	}

	cv::Mat output_image;

	cv::drawMatches(image_1, keypoints1, image_2, keypoints2, good_matches,
		output_image, cv::Scalar(0, 0, 255), cv::Scalar(0, 0, 255),
		std::vector<char>(), cv::DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS);

	// Display the matching in the window named Matching
	cv::imshow("Matching", output_image);

	// If there are 3 matches or more then the printed logo can be seen in the input image and the card is on the correct side
	if (good_matches.size() >= 4) {
		return 'N';
	}
	else {
		// if there is less than 3 matches it means the camera is giving false matching  and the card must be flipped
		return 'Y';
	}
}

// Create a function that captures an image and returns the captured image if possible
cv::Mat capture_an_image(cv::VideoCapture &camera) {

	// initialise a Mat object to store the input image
	cv::Mat image_2;

	// if the camera is not accessible the function will return an empty image
	if (!camera.isOpened()) {
		return image_2;
	}

	// count varible is used to pass several frames before capturing a still image
	// this will allow the camera sensor to tune the image before capturing
	int count = 0;

	while (true) {
		// frame is a mat object to display every frame captured by camera to display a video
		cv::Mat frame;
		//copy the captured image in camera object to frame object
		camera >> frame;

		// crop the frame to include only the card
		cv::Rect croppedFrame = cv::Rect(150, 140, 300, 200);
		frame = frame(croppedFrame);

		// show the frame i.e the input image in the frame window
		cv::imshow("frame", frame);

		// increase the count variable by 1
		count++;
		// wait to display the frame
		cv::waitKey(1);
		// let several frames to pass then capture an image
		if (count > 30) {
			// clone the frame to image_2 variable 
			image_2 = frame.clone();
			// return the captured image
			return image_2;
		}
	}
}


// the program main function
int main(int argc, char* argv[]) {

	boost::asio::io_context io;

	boost::asio::serial_port port(io);

	// Open a valid port; here, COM6 is an Arduino Uno.
	port.open("COM5");


	// Configure the port to use 8N1 format. Eight bits, no parity, one stop bit.
	port.set_option(boost::asio::serial_port_base::baud_rate(9600));
	port.set_option(boost::asio::serial_port_base::character_size(8));
	port.set_option(boost::asio::serial_port_base::parity(boost::asio::serial_port_base::parity::none));
	port.set_option(boost::asio::serial_port_base::stop_bits(boost::asio::serial_port_base::stop_bits::one));

	if (port.is_open()) {
		std::string s{ 'S' };
		boost::asio::write(port, boost::asio::buffer(&s, 1)); //Synchronous write; a blocking function.
	}

	// create windows to display the images
	// the window name is specified first and then a flag WINDOW_AUTOSIZE is passed to display the image with its original size
	// this is limited to the screen resolution, so the window may scale the image to fit
	cv::namedWindow("Matching", cv::WINDOW_AUTOSIZE);
	cv::namedWindow("frame", cv::WINDOW_AUTOSIZE);

	//initialise camera object in VideoCapture class to display the webcam view 
	/// my webcam index is 0
	cv::VideoCapture camera(0);

	// A set of command to communicate with the micro-controller
	std::cout << "This app receives 'C' charachter to capture an image of the card" << std::endl;
	std::cout << "The app will return '1', '2', '3' '4', 'E', and 'e'" << std::endl;
	std::cout << "'1' is case one only rotaion is required" << std::endl;
	std::cout << "'2' is case tow only rotaion is required" << std::endl;
	std::cout << "'3' is case Three flipping and rotaion are required" << std::endl;
	std::cout << "'4' is case four flipping and rotaion are required" << std::endl;
	std::cout << "'E' if there are error reading in this case it is possible to recapture for better reading" << std::endl;
	std::cout << "'e' means the camera cannot be accessed" << std::endl;


	// varibles to determine which of the four cases should be accepted.
	char flip = 'X';
	char rotate = 'X';

	// a veraible to track errors to be handled on the micro-controller side
	int error_counter = 0;

	//// A while loop to receive commands
	while (true) {

		if (!port.is_open()) {
			continue;
		}

		std::string commandS{ 's' }, commandR{ '1' };

		boost::asio::read(port, boost::asio::buffer(&commandR, 1)); //Synchronous read; a blocking function

		if (commandR[0] == 'C') {	// 'C' command
			cv::Mat input = capture_an_image(camera);
			if (input.empty()) {
				std::cout << "e" << std::endl;
				commandS = "e";
				boost::asio::write(port, boost::asio::buffer(&commandS, 1)); //Synchronous write; a blocking function.

				std::cout << "The camera access is denied. Once the problem is solved please type 'S' to run the system again" << std::endl;
				std::cin >> commandS;
				boost::asio::write(port, boost::asio::buffer(&commandS, 1)); //Synchronous write; a blocking function.
			}
			else {	// if image is captured

					// call the check_side(cv::Mat input) function which will return a character to give the micro-controller instruction
					// returns 'Y' means: yes, flipping is required
					// 'N' No need to flip

					// call the check_rotation(cv::Mat input) function which will return a character to give the micro-controller instruction
					// returns 'L' means: Left rotation is required
					// 'R' Right rotation is required
					// or 'E' to handle errors
				flip = check_side(input);
				rotate = check_rotation(input, flip);

				std::cout << "Flip Return: " << flip << std::endl;
				std::cout << "Rotate Return: " << rotate << std::endl;

				/// send the correct command to the micro-controller
				if (flip == 'N' && rotate == 'R') {
					std::cout << "Case 1 send charachter '1' to serial port" << std::endl;
					commandS = "1";
					boost::asio::write(port, boost::asio::buffer(&commandS, 1)); //Synchronous write; a blocking function.
				}
				else if (flip == 'N' && rotate == 'L') {
					std::cout << "Case 2 send charachter '2' to serial port" << std::endl;
					commandS = "2";
					boost::asio::write(port, boost::asio::buffer(&commandS, 1)); //Synchronous write; a blocking function.
				}
				else if (flip == 'Y' && rotate == 'L') {
					std::cout << "Case 3 send charachter '3' to serial port" << std::endl;
					commandS = "3";
					boost::asio::write(port, boost::asio::buffer(&commandS, 1)); //Synchronous write; a blocking function.
				}
				else if (flip == 'Y' && rotate == 'R') {
					std::cout << "Case 4 send charachter '4' to serial port" << std::endl;
					commandS = "4";
					boost::asio::write(port, boost::asio::buffer(&commandS, 1)); //Synchronous write; a blocking function.
				}
				else {
					std::cout << "Error 'E'" << std::endl;
					commandS = "E";
					boost::asio::write(port, boost::asio::buffer(&commandS, 1)); //Synchronous write; a blocking function.
					error_counter++;

					if (error_counter >= 10) {
						std::cout << "Please Type 'S' once the problem is solved" << std::endl;
						std::cin >> commandS;
						boost::asio::write(port, boost::asio::buffer(&commandS, 1)); //Synchronous write; a blocking function.
						error_counter = 0;
					}
				}

			}
		}
		else { // if no command continue
			continue;
		}
	}

	// release the camera
	camera.release();

	// close all windows
	cv::destroyAllWindows();
	// close the port
	port.close();

	// terminate the program
	return 0;
}