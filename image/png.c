#define _CRT_SECURE_NO_WARNINGS
#include "image_debug.h"
#include "png.h"
#include "../zlib/lz.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define DEFAULT_ALLOCATION_SIZE 65535

#define bswap32(x) \
	(((x >> 24) & 0x000000FF) | \
	 ((x >> 8) & 0x0000FF00)  | \
	 ((x << 8) & 0x00FF0000)  | \
	 ((x << 24) & 0xFF000000))

#define Skip(png, n) png->reader += n;

static const uint8_t PNG_SIG[] = { 137, 80, 78, 71, 13, 10, 26, 10 };

static uint32_t calculate_png_chunk_crc(uint32_t type, uint8_t* data, uint64_t data_len) {
	unsigned long c = 0xffffffffL;
	c = update_crc(c, (unsigned char*)&type, 4);
	c = update_crc(c, (unsigned char*)data, (int)data_len);
	return c ^ 0xffffffffL;
}

// These are static APIs, they don't require debug level NULL checking.
static inline uint8_t GetUInt8(PNG* png) {
	return *png->reader++;
}

static inline uint16_t GetUInt16be(PNG* png) {
	uint16_t b = GetUInt8(png);
	return (b << 8) | GetUInt8(png);
}

static inline uint32_t GetUInt32be(PNG* png) {
	uint32_t b = GetUInt16be(png);
	return (b << 16) | GetUInt16be(png);
}

static inline uint16_t GetUInt16(PNG* png) {
	uint16_t ret = *(uint16_t*)png->reader;
	png->reader += 2;
	return ret;
}

static inline uint32_t GetUInt32(PNG* png) {
	uint32_t ret = *(uint32_t*)png->reader;
	png->reader += 4;
	return ret;
}

static inline const uint8_t* GetBytes(PNG* png, uint64_t n) {
	const uint8_t* ret = png->reader;
	png->reader += n;
	return ret;
}

inline uint32_t BytesPerColorTypePNG(uint32_t type) {
	return (type == PNG_RGB || type == PNG_PLTE) ? 3 : (type == PNG_RGBA) ? 4 : (type == PNG_GRAYSCALE) ? 1 : 0;
}

static int32_t PaethPredictor(int32_t a, int32_t b, int32_t c) {
	int32_t p = a + b - c;
	int32_t pa = abs((int32_t)(p - a));
	int32_t pb = abs((int32_t)(p - b));
	int32_t pc = abs((int32_t)(p - c));
	if (pa <= pb && pa <= pc) return a;
	else if (pb <= pc) return b;
	else return c;
}

static void DefilterPNG(PNG* png, uint8_t* filtered, uint8_t* pixels) {
	uint64_t outIdx = 0;
	uint32_t bpp = png->isPlte ? 1 : BytesPerColorTypePNG(png->ihdr.colorType);
	uint32_t stride = png->ihdr.imageWidth * bpp;
	uint32_t filterType = 0;
	uint8_t* offset = filtered;
	uint8_t* prevRow = NULL;

	for (uint32_t i = 0; i < png->ihdr.imageHeight; ++i) {
		filterType = *offset++;

		switch (filterType) {
		case 0: // None: Raw(x)
		{
			for (uint32_t j = 0; j < stride; ++j) {
				pixels[outIdx++] = offset[j];
			}
			break;
		}
		case 1: // Sub: Sub(x) + Raw(x-bpp)
		{
			for (uint32_t j = 0; j < stride; ++j) {
				uint8_t left = (j >= bpp) ? pixels[outIdx - bpp] : 0;
				pixels[outIdx++] = offset[j] + left;
			}
			break;
		}

		case 2: // Up: Up(x) + Prior(x)
		{
			for (uint32_t j = 0; j < stride; ++j) {
				pixels[outIdx++] = offset[j] + (i ? prevRow[j] : 0);
			}
			break;
		}
		case 3: // Average: Average(x) + floor((Raw(x-bpp)+Prior(x))/2)
		{
			for (uint32_t j = 0; j < stride; ++j) {
				uint8_t left = (j >= bpp) ? pixels[outIdx - bpp] : 0;
				uint8_t up = prevRow ? prevRow[j] : 0;

				pixels[outIdx++] = offset[j] + ((left + up) >> 1);
			}
			break;
		}
		case 4: // Paeth: Paeth(x) + PaethPredictor(Raw(x-bpp), Prior(x), Prior(x-bpp))
		{
			for (uint32_t j = 0; j < stride; ++j) {
				uint8_t left = (j >= bpp) ? pixels[outIdx - bpp] : 0;
				uint8_t up = prevRow ? prevRow[j] : 0;
				uint8_t upLeft = (j >= bpp && prevRow) ? prevRow[j - bpp] : 0;
				pixels[outIdx++] = offset[j] + PaethPredictor(left, up, upLeft);
			}
			break;
		}
		default:
		{
			break;
		}
		}
		prevRow = pixels + i * stride;
		offset += stride;
	}
}

static void FilterPNG(const PNGWriter* png, uint8_t* pixels, uint8_t* filtered) {
	uint64_t outIdx = 0;
	uint32_t stride = png->imageWidth * BytesPerColorTypePNG(png->imageType);
	for (uint64_t i = 0; i < png->imageHeight; ++i) {
		filtered[outIdx++] = 0; // No Filter Byte.
		memcpy(filtered + outIdx, pixels + i * stride, stride);
		outIdx += stride;
	}
}

static void DeindexPNG(PNG* png, uint8_t* indices, uint8_t* pixels) {
	size_t size = (uint64_t)png->ihdr.imageWidth * (uint64_t)png->ihdr.imageHeight;
	for (size_t i = 0; i < size; ++i) {
		*(RGB*)&pixels[3 * i] = png->plte.pEntries[indices[i]];
	}
}

int32_t IsPNG(const uint8_t* data, uint64_t size) {
	DEBUG(data);
	if (size < 67 || *(uint64_t*)data != *(uint64_t*)PNG_SIG) {
		return 0;
	}
	return 1;
}

int32_t InitPNG(PNG* png, const uint8_t* data, uint64_t inSize)
{
	DEBUG(png);
	DEBUG(data);
	DEBUG(inSize);

	if (!IsPNG(data, inSize)) {
		return -3;
	}
	png->isInit = 1;
	png->reader = data;
	png->chunksSize = 0;
	png->chunksCapacity = 20;

	Skip(png, 8);

	png->pChunks = (PNGChunk*)calloc(png->chunksCapacity, sizeof(PNGChunk));
	if (!png->pChunks) {
		return -2;
	}

	PNGChunk chunk = { 0 };
	do {
		chunk.length = GetUInt32be(png);
		chunk.u32type = GetUInt32(png);
		chunk.data = GetBytes(png, chunk.length);
		chunk.crc = GetUInt32be(png);

		switch (chunk.u32type) {
		case 'RDHI':
		{
			png->ihdr = *(IHDR*)chunk.data;
			png->ihdr.imageWidth = bswap32(png->ihdr.imageWidth);
			png->ihdr.imageHeight = bswap32(png->ihdr.imageHeight);
			break;
		}
		case 'ETLP':
		{
			png->isPlte = 1;
			if (chunk.length % 3 != 0) {
				free(png->pChunks);
				return -3;
			}
			png->plte.nEntries = chunk.length / 3;
			png->plte.pEntries = (const RGB*)chunk.data;
			break;
		}
		case 'AMAg':
		{
			png->gama = bswap32(*(uint32_t*)chunk.data);
			break;
		}
		default:
		{
			break;
		}
		}

		if (png->chunksCapacity <= png->chunksSize) {
			png->chunksCapacity *= 2;
			PNGChunk* tmp = (PNGChunk*)realloc(png->pChunks, png->chunksCapacity * sizeof(PNGChunk));
			if (!tmp) {
				free(png->pChunks);
				return -2;
			}
			png->pChunks = tmp;
		}
		png->pChunks[png->chunksSize++] = chunk;

	} while (chunk.u32type != 'DNEI');

	return 1;
}

int32_t ReadPNG(PNG* png, uint8_t* output, uint64_t outSize)
{
	DEBUG(png);
	DEBUG(output);
	DEBUG(outSize);

	if (!png->isInit) {
		return -6;
	}

	int32_t error = 0;
	ZlibReader zlib = { 0 };
	uint8_t* uncompressed = NULL;

	uint64_t compressedCapacity = DEFAULT_ALLOCATION_SIZE;
	uint64_t compressedSize = 0;
	uint8_t* compressed = calloc(1, compressedCapacity);

	if (!compressed) {
		return -2;
	}

	for (uint32_t i = 0; i < png->chunksSize; ++i) {
		if (png->pChunks[i].u32type == 'TADI') {
			uint64_t newSize = compressedSize + png->pChunks[i].length;
			if (compressedCapacity <= newSize) {
				compressedCapacity *= 8;
				uint8_t* tmp = realloc(compressed, compressedCapacity);
				if (!tmp) {
					free(compressed);
					return -2;
				}
				compressed = tmp;
			}
			memcpy(compressed + compressedSize, png->pChunks[i].data, png->pChunks[i].length);
			compressedSize = newSize;
		}
	}

	uncompressed = (uint8_t*)calloc(1, outSize * 2);
	if (!uncompressed) {
		free(compressed);
		return -2;
	}

	error = lzInflateInit(&zlib, compressed, compressedSize);
	if (error < 0) {
		free(compressed);
		free(uncompressed);
		return error;
	}
	error = lzInflate(&zlib, uncompressed, outSize * 2);
	if (error < 0) {
		free(compressed);
		free(uncompressed);
		return error;
	}

	if (png->isPlte) {
		uint8_t* tmp = (uint8_t*)calloc(1, outSize);
		if (!tmp) {
			free(uncompressed);
			free(compressed);
			return -2;
		}
		DefilterPNG(png, uncompressed, tmp);
		DeindexPNG(png, tmp, output);
		free(tmp);
	}
	else {
		DefilterPNG(png, uncompressed, output);
	}

	free(uncompressed);
	free(compressed);

	return 1;
}

int32_t WritePNG(const PNGWriter* png, const char* fileName)
{
	DEBUG(png);
	DEBUG(fileName);

	int32_t error = 0;
	FILE* file = NULL;
	uint8_t* outputBuffer = NULL;
	uint64_t outputBufferSize = 0;
	ZlibWriter zlib = { 0 };
	uint32_t bpp = BytesPerColorTypePNG(png->imageType);
	uint64_t imageFilteredSize = (uint64_t)(png->imageWidth * png->imageHeight * bpp) + png->imageHeight;

	IHDR ihdr = {
		.imageWidth = bswap32(png->imageWidth),
		.imageHeight = bswap32(png->imageHeight),
		.bitDepth = 8,
		.colorType = png->imageType,
		.compressionMethod = 0,
		.filterMethod = 0,
		.interlaceMethod = 0
	};

	uint8_t* filteredData = (int8_t*)calloc(1, imageFilteredSize);
	if (!filteredData) {
		return -3;
	}

	FilterPNG(png, png->data, filteredData);

	outputBufferSize = imageFilteredSize + DEFAULT_ALLOCATION_SIZE;
	outputBuffer = (uint8_t*)calloc(1, outputBufferSize);
	if (!outputBuffer) {
		return -3;
	}

	error = lzDeflateInit(&zlib, filteredData, imageFilteredSize);
	if (error < 0) {
		free(filteredData);
		free(outputBuffer);
		return error;
	}
	error = lzDeflate(&zlib, outputBuffer, &outputBufferSize);
	if (error < 0) {
		free(filteredData);
		free(outputBuffer);
		return error;
	}

	file = fopen(fileName, "wb");
	if (!file) {
		free(filteredData);
		free(outputBuffer);
		return -3;
	}

	if (fwrite(PNG_SIG, 1, sizeof(PNG_SIG), file) != sizeof(PNG_SIG)) {
		goto writing_error;
	}

	PNGChunk chunks[3] = {
		{
			.length = bswap32(13),
			.u32type = 'RDHI',
			.data = (uint8_t*)&ihdr,
			.crc = bswap32(calculate_png_chunk_crc('RDHI', (uint8_t*)&ihdr, 13))
		},
		{
			.length = bswap32(outputBufferSize),
			.u32type = 'TADI',
			.data = outputBuffer,
			.crc = bswap32(calculate_png_chunk_crc('TADI', outputBuffer, outputBufferSize))
		},
		{
			.length = 0,
			.u32type = 'DNEI',
			.data = NULL,
			.crc = 0x826042ae
		}
	};

	for (int i = 0; i < 3; ++i) {
		uint32_t chunkLen = bswap32(chunks[i].length);
		fwrite(&chunks[i].length, 4, 1, file);
		fwrite(&chunks[i].u32type, 4, 1, file);
		if (fwrite(chunks[i].data, 1, chunkLen, file) != chunkLen) {
			goto writing_error;
		}
		fwrite(&chunks[i].crc, 4, 1, file);
	}

	free(filteredData);
	free(outputBuffer);
	fclose(file);
	return 1;

writing_error:
	free(filteredData);
	free(outputBuffer);
	fclose(file);
	return -9;
}

void FreePNG(PNG* png) {
	if (!png) return;
	if (!png->pChunks) return;
	free(png->pChunks);
	png->chunksSize = 0;
	png->chunksCapacity = 0;
}

// Code from libpng: https://www.libpng.org/pub/png/spec/1.2/PNG-CRCAppendix.html

/* Table of CRCs of all 8-bit messages. */
unsigned long crc_table[256];

/* Flag: has the table been computed? Initially false. */
int crc_table_computed = 0;

/* Make the table for a fast CRC. */
static void make_crc_table(void)
{
	unsigned long c;
	int n, k;

	for (n = 0; n < 256; n++) {
		c = (unsigned long)n;
		for (k = 0; k < 8; k++) {
			if (c & 1)
				c = 0xedb88320L ^ (c >> 1);
			else
				c = c >> 1;
		}
		crc_table[n] = c;
	}
	crc_table_computed = 1;
}

/* Update a running CRC with the bytes buf[0..len-1]--the CRC
   should be initialized to all 1's, and the transmitted value
   is the 1's complement of the final running CRC (see the
   crc() routine below)). */

static unsigned long update_crc(unsigned long crc, unsigned char* buf,
	int len)
{
	unsigned long c = crc;
	int n;

	if (!crc_table_computed)
		make_crc_table();
	for (n = 0; n < len; n++) {
		c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
	}
	return c;
}