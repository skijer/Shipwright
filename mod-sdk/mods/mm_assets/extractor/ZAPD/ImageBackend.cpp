#define _CRT_SECURE_NO_WARNINGS
#include "ImageBackend.h"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

#include "Utils/StringHelper.h"
#include "WarningHandler.h"

// libpng's pixel layout names without libpng: the sandboxed extractor builds .o2r archives, which never hold PNGs.
namespace
{
constexpr uint8_t PNG_COLOR_TYPE_RGB = 2;
constexpr uint8_t PNG_COLOR_TYPE_PALETTE = 3;
constexpr uint8_t PNG_COLOR_TYPE_RGBA = 6;

struct png_color
{
	uint8_t red;
	uint8_t green;
	uint8_t blue;
};
}

/* ImageBackend */

ImageBackend::~ImageBackend()
{
	FreeImageData();
}

void ImageBackend::ReadPng(const char* filename)
{
	HANDLE_ERROR(WarningType::InvalidPNG, StringHelper::Sprintf("cannot read '%s': no PNG support", filename), "");
}

void ImageBackend::ReadPng(const fs::path& filename)
{
	ReadPng(filename.string().c_str());
}

void ImageBackend::WritePng(const char* filename)
{
	HANDLE_ERROR(WarningType::InvalidPNG, StringHelper::Sprintf("cannot write '%s': no PNG support", filename), "");
}

void ImageBackend::WritePng(const fs::path& filename)
{
	WritePng(filename.string().c_str());
}

void ImageBackend::SetTextureData(const std::vector<std::vector<RGBAPixel>>& texData,
                                  uint32_t nWidth, uint32_t nHeight, uint8_t nColorType,
                                  uint8_t nBitDepth)
{
	FreeImageData();

	width = nWidth;
	height = nHeight;
	colorType = nColorType;
	bitDepth = nBitDepth;

	size_t bytePerPixel = GetBytesPerPixel();

	pixelMatrix = static_cast<uint8_t**>(malloc(sizeof(uint8_t*) * height));
	for (size_t y = 0; y < height; y++)
	{
		pixelMatrix[y] = static_cast<uint8_t*>(malloc(sizeof(uint8_t*) * width * bytePerPixel));
		for (size_t x = 0; x < width; x++)
		{
			pixelMatrix[y][x * bytePerPixel + 0] = texData.at(y).at(x).r;
			pixelMatrix[y][x * bytePerPixel + 1] = texData.at(y).at(x).g;
			pixelMatrix[y][x * bytePerPixel + 2] = texData.at(y).at(x).b;

			if (colorType == PNG_COLOR_TYPE_RGBA)
				pixelMatrix[y][x * bytePerPixel + 3] = texData.at(y).at(x).a;
		}
	}
	hasImageData = true;
}

void ImageBackend::InitEmptyRGBImage(uint32_t nWidth, uint32_t nHeight, bool alpha)
{
	FreeImageData();

	width = nWidth;
	height = nHeight;
	colorType = PNG_COLOR_TYPE_RGB;
	if (alpha)
		colorType = PNG_COLOR_TYPE_RGBA;
	bitDepth = 8;  // nBitDepth;

	size_t bytePerPixel = GetBytesPerPixel();

	pixelMatrix = static_cast<uint8_t**>(malloc(sizeof(uint8_t*) * height));
	for (size_t y = 0; y < height; y++)
	{
		pixelMatrix[y] = static_cast<uint8_t*>(calloc(width * bytePerPixel, sizeof(uint8_t*)));
	}

	hasImageData = true;
}

void ImageBackend::InitEmptyPaletteImage(uint32_t nWidth, uint32_t nHeight)
{
	FreeImageData();

	width = nWidth;
	height = nHeight;
	colorType = PNG_COLOR_TYPE_PALETTE;
	bitDepth = 8;

	size_t bytePerPixel = GetBytesPerPixel();

	pixelMatrix = (uint8_t**)malloc(sizeof(uint8_t*) * height);
	for (size_t y = 0; y < height; y++)
	{
		pixelMatrix[y] = static_cast<uint8_t*>(calloc(width * bytePerPixel, sizeof(uint8_t*)));
	}
	colorPalette = calloc(paletteSize, sizeof(png_color));
	alphaPalette = static_cast<uint8_t*>(calloc(paletteSize, sizeof(uint8_t)));

	hasImageData = true;
	isColorIndexed = true;
}

RGBAPixel ImageBackend::GetPixel(size_t y, size_t x) const
{
	assert(y < height);
	assert(x < width);
	assert(!isColorIndexed);

	RGBAPixel pixel;
	size_t bytePerPixel = GetBytesPerPixel();
	pixel.r = pixelMatrix[y][x * bytePerPixel + 0];
	pixel.g = pixelMatrix[y][x * bytePerPixel + 1];
	pixel.b = pixelMatrix[y][x * bytePerPixel + 2];
	if (colorType == PNG_COLOR_TYPE_RGBA)
		pixel.a = pixelMatrix[y][x * bytePerPixel + 3];
	return pixel;
}

uint8_t ImageBackend::GetIndexedPixel(size_t y, size_t x) const
{
	assert(y < height);
	assert(x < width);
	assert(isColorIndexed);

	return pixelMatrix[y][x];
}

void ImageBackend::SetRGBPixel(size_t y, size_t x, uint8_t nR, uint8_t nG, uint8_t nB, uint8_t nA)
{
	assert(hasImageData);
	assert(y < height);
	assert(x < width);

	size_t bytePerPixel = GetBytesPerPixel();
	pixelMatrix[y][x * bytePerPixel + 0] = nR;
	pixelMatrix[y][x * bytePerPixel + 1] = nG;
	pixelMatrix[y][x * bytePerPixel + 2] = nB;
	if (colorType == PNG_COLOR_TYPE_RGBA)
		pixelMatrix[y][x * bytePerPixel + 3] = nA;
}

void ImageBackend::SetGrayscalePixel(size_t y, size_t x, uint8_t grayscale, uint8_t alpha)
{
	assert(hasImageData);
	assert(y < height);
	assert(x < width);

	size_t bytePerPixel = GetBytesPerPixel();
	pixelMatrix[y][x * bytePerPixel + 0] = grayscale;
	pixelMatrix[y][x * bytePerPixel + 1] = grayscale;
	pixelMatrix[y][x * bytePerPixel + 2] = grayscale;
	if (colorType == PNG_COLOR_TYPE_RGBA)
		pixelMatrix[y][x * bytePerPixel + 3] = alpha;
}

void ImageBackend::SetIndexedPixel(size_t y, size_t x, uint8_t index, uint8_t grayscale)
{
	assert(hasImageData);
	assert(y < height);
	assert(x < width);

	size_t bytePerPixel = GetBytesPerPixel();
	pixelMatrix[y][x * bytePerPixel + 0] = index;

	assert(index < paletteSize);
	png_color* pal = static_cast<png_color*>(colorPalette);
	pal[index].red = grayscale;
	pal[index].green = grayscale;
	pal[index].blue = grayscale;
	alphaPalette[index] = 255;
}

void ImageBackend::SetPaletteIndex(size_t index, uint8_t nR, uint8_t nG, uint8_t nB, uint8_t nA)
{
	assert(isColorIndexed);
	assert(index < paletteSize);

	png_color* pal = static_cast<png_color*>(colorPalette);
	pal[index].red = nR;
	pal[index].green = nG;
	pal[index].blue = nB;
	alphaPalette[index] = nA;
}

void ImageBackend::SetPalette(const ImageBackend& pal, uint32_t offset)
{
	assert(isColorIndexed);
	size_t bytePerPixel = pal.GetBytesPerPixel();

	for (size_t y = 0; y < pal.height; y++)
	{
		for (size_t x = 0; x < pal.width; x++)
		{
			size_t index = y * pal.width + x;
			if (index >= paletteSize)
			{
				/*
				 * Some TLUTs are bigger than 256 colors.
				 * For those cases, we will only take the first 256
				 * to colorize this CI texture.
				 */
				return;
			}

			uint8_t r = pal.pixelMatrix[y][x * bytePerPixel + 0];
			uint8_t g = pal.pixelMatrix[y][x * bytePerPixel + 1];
			uint8_t b = pal.pixelMatrix[y][x * bytePerPixel + 2];
			uint8_t a = pal.pixelMatrix[y][x * bytePerPixel + 3];
			SetPaletteIndex(index + offset, r, g, b, a);
		}
	}
}

uint32_t ImageBackend::GetWidth() const
{
	return width;
}

uint32_t ImageBackend::GetHeight() const
{
	return height;
}

uint8_t ImageBackend::GetColorType() const
{
	return colorType;
}

uint8_t ImageBackend::GetBitDepth() const
{
	return bitDepth;
}

double ImageBackend::GetBytesPerPixel() const
{
	switch (colorType)
	{
	case PNG_COLOR_TYPE_RGBA:
		return 4 * bitDepth / 8;

	case PNG_COLOR_TYPE_RGB:
		return 3 * bitDepth / 8;

	case PNG_COLOR_TYPE_PALETTE:
		return 1 * bitDepth / 8;

	default:
		HANDLE_ERROR(WarningType::InvalidPNG, "invalid color type", "");
	}
}

void ImageBackend::FreeImageData()
{
	if (hasImageData)
	{
		for (size_t y = 0; y < height; y++)
			free(pixelMatrix[y]);
		free(pixelMatrix);
		pixelMatrix = nullptr;
	}

	if (isColorIndexed)
	{
		free(colorPalette);
		free(alphaPalette);
		colorPalette = nullptr;
		alphaPalette = nullptr;
		isColorIndexed = false;
	}

	hasImageData = false;
}

/* RGBAPixel */

void RGBAPixel::SetRGBA(uint8_t nR, uint8_t nG, uint8_t nB, uint8_t nA)
{
	r = nR;
	g = nG;
	b = nB;
	a = nA;
}

void RGBAPixel::SetGrayscale(uint8_t grayscale, uint8_t alpha)
{
	r = grayscale;
	g = grayscale;
	b = grayscale;
	a = alpha;
}
