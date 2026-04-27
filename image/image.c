#define _CRT_SECURE_NO_WARNINGS
#include "image_debug.h"
#include "image.h"
#include "png.h"
#include <stdio.h>
#include <stdlib.h>

static int32_t iLoadImagePNG(Image* image, const uint8_t* data, uint64_t size) {
    // This is a static API, it doesn't need debug level NULL checking.
    int32_t error = 0;
    uint64_t imageSizeBytes = 0;
    PNG png = { 0 };
    CHECK(InitPNG(&png, data, size), error);
  
    image->width = png.ihdr.imageWidth;
    image->height = png.ihdr.imageHeight;
    uint32_t bpp = BytesPerColorTypePNG(png.ihdr.colorType);
    imageSizeBytes = (uint64_t)image->width * image->height * bpp;
    if (!imageSizeBytes) {
        FreePNG(&png);
        return -10;
    }
    image->data = (uint8_t*)calloc(1, imageSizeBytes);
    if (!image->data) {
        return -2;
    }
    if ((error = ReadPNG(&png, image->data, imageSizeBytes)) < 0) {
        free(image->data);
        FreePNG(&png);
        return error;
    }
    FreePNG(&png);
    return 1;
}

static int32_t iWriteImagePNG(Image* image, const char* fileName) {
    PNGWriter png = { 0 };
    png.imageWidth = image->width;
    png.imageHeight = image->height;
    png.imageType = (image->imageType == IMAGE_PNG_RGB) ? PNG_RGB : 
                    (image->imageType == IMAGE_PNG_RGBA) ? PNG_RGBA : 
                    (image->imageType == IMAGE_PNG_GRAYSCALE) ? PNG_GRAYSCALE : 0;
    png.data = image->data;
    return WritePNG(&png, fileName);
}

int32_t iReadImage(Image* image, const char* fileName)
{
    DEBUG(image);
    DEBUG(fileName);

    uint64_t size = 0;
    uint8_t* data = NULL;
    FILE* file = NULL;

    file = fopen(fileName, "rb");
    if (!file) {
        return -2;
    }
    
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (!size) {
        return -3;
    }

    data = (uint8_t*)malloc(size);
    if (!data) {
        return -2;
    }

    if (fread(data, 1, size, file) < size) {
        free(data);
        fclose(file);
        return -2;
    }

    int32_t error = 0;
    if (IsPNG(data, size) > 0) {
        if ((error = iLoadImagePNG(image, data, size)) < 0) {
            free(data);
            fclose(file);
            return error;
        }
    }

    free(data);
    fclose(file);
    return 1;
}

int32_t iWriteImage(Image* image, const char* fileName)
{
    DEBUG(image);
    DEBUG(fileName);
    DEBUG(image->imageType == IMAGE_PNG_RGB || image->imageType == IMAGE_PNG_RGBA || image->imageType == IMAGE_PNG_GRAYSCALE);

    int32_t error = 0;

    switch (image->imageType) {
    case IMAGE_PNG_RGB:
    case IMAGE_PNG_GRAYSCALE:
    case IMAGE_PNG_RGBA:
    {
        if ((error = iWriteImagePNG(image, fileName) < 0)) {
            return error;
        }
    }
    default:
    {
        return -4;
    }
    }
    return 1;
}

#ifdef _WIN32
#pragma optimize( "", off )
void iFreeImage(Image* image)
{
    if (!image) return; // NOP
    if (!image->data) return;
    image->width = 0;
    image->height = 0;
    free(image->data);
}
#pragma optimize( "", on )
#else
void iFreeImage(Image* image)
{
    if (!image) return; // NOP
    if (!image->data) return;
    image->width = 0;
    image->height = 0;
    image->bytesPerPixel = 0;
    free(image->data);
}
#endif