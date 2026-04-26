#include "image/image.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[]) {
	// example loading image

	int32_t error = 0;
	//Image image1 = { 0 };
	//Image image2 = { 0 };
	//Image image3 = { 0 };
	//Image image4 = { 0 };

	//error = iLoadImage(&image1, "example1.png");
	//if (error < 0) {
	//	return error;
	//}
	//iFreeImage(&image1);
	//
	//error = iLoadImage(&image2, "example2.png");
	//if (error < 0) {
	//	return error;
	//}
	//iFreeImage(&image2);
	//
	//error = iLoadImage(&image3, "example3.png");
	//if (error < 0) {
	//	return error;
	//}
	//iFreeImage(&image3);

	//error = iLoadImage(&image4, "example4.png");
	//if (error < 0) {
	//	return error;
	//}
	//iFreeImage(&image4);

	// example writing image

	static uint8_t redpixels[40000] = {0, 255, 255, 255};
	for (uint32_t i = 0; i < sizeof(redpixels) - 4; ++i) {
		redpixels[i + 4] = redpixels[i];
	}
	Image image1 = { 0 };
	image1.width = 100;
	image1.height = 100;
	image1.imageType = IMAGE_PNG_RGBA;
	image1.data = redpixels;
	error = iWriteImage(&image1, "example_output1.png");
	return 0;
}