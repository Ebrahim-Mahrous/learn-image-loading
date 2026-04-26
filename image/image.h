#ifndef _IMAGE_H
#define _IMAGE_H
#include <stdint.h>

typedef struct Image {
	uint32_t width;
	uint32_t height;
	uint32_t imageType;
	uint8_t* data; // size of data = width * height * bytesPerPixel (bytes)
} Image;

enum ImageType {
	IMAGE_PNG_RGB,
	IMAGE_PNG_RGBA,
	IMAGE_PNG_GRAYSCALE,
};

int32_t iReadImage(Image* image, const char* fileName);
void iFreeImage(Image* image);

int32_t iWriteImage(Image* image, const char* fileName);

#endif // !_IMAGE_H