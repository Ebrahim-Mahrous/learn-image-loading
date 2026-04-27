#ifndef _PNG_H
#define _PNG_H

#include <stdint.h>

typedef struct RGB {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} RGB;

typedef struct RGBA {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
} RGBA;

enum PNG_IMAGE_TYPE {
	PNG_GRAYSCALE = 0,
	PNG_RGB = 2,
	PNG_PLTE = 3,
	PNG_RGBA = 6
};

typedef struct IHDR {
	uint32_t imageWidth;
	uint32_t imageHeight;
	uint8_t bitDepth;
	uint8_t colorType;
	uint8_t compressionMethod;
	uint8_t filterMethod;
	uint8_t interlaceMethod;
} IHDR;

typedef struct PLTE {
	uint64_t nEntries;
	union {
		const uint8_t(*p3Entries)[3];
		const RGB *pEntries;
	};
} PLTE;

typedef struct PNGChunk {
	uint32_t length;
	union {
		uint32_t u32type;
		uint8_t type[4];
	};
	const uint8_t* data;
	uint32_t crc;
} PNGChunk;

typedef struct PNG
{
	IHDR ihdr;
	PLTE plte;
	uint32_t isPlte;
	uint32_t gama;

	uint32_t isInit;
	const uint8_t* reader;

	uint64_t chunksCapacity;
	uint64_t chunksSize;
	PNGChunk* pChunks;
} PNG;

typedef struct PNGWriter {
	uint32_t imageWidth;
	uint32_t imageHeight;
	uint32_t imageType;
	uint8_t* data;
} PNGWriter;

#ifdef __cplusplus
extern "C" {
#endif

int32_t IsPNG(const uint8_t* data, uint64_t size);
int32_t InitPNG(PNG* png, const uint8_t *data, uint64_t inSize);
int32_t ReadPNG(PNG* png, uint8_t* ouput, uint64_t outSize);
int32_t WritePNG(const PNGWriter* png, const char* fileName);

// -- code from libpng https://www.libpng.org/pub/png/spec/1.2/PNG-CRCAppendix.html
static unsigned long update_crc(unsigned long crc, unsigned char* buf, int len);
// --

inline uint32_t BytesPerColorTypePNG(uint32_t type);
void FreePNG(PNG* png);

#ifdef __cplusplus
}
#endif

#endif // _PNG_H