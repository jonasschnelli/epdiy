//
// JPEGDEC C++ wrapper implementation for www-image2 example
// This file provides the C++ class interface that wraps the C functions
//
#include "JPEGDEC.h"

extern "C" {
// Forward declarations of C functions that actually exist
int JPEG_openRAM(JPEGIMAGE *pJPEG, uint8_t *pData, int iDataSize, JPEG_DRAW_CALLBACK *pfnDraw);
int JPEG_openFile(JPEGIMAGE *pJPEG, const char *szFilename, JPEG_DRAW_CALLBACK *pfnDraw);
void JPEG_setFramebuffer(JPEGIMAGE *pJPEG, void *pFramebuffer);
void JPEG_setCropArea(JPEGIMAGE *pJPEG, int x, int y, int w, int h);
void JPEG_getCropArea(JPEGIMAGE *pJPEG, int *x, int *y, int *w, int *h);
void JPEG_close(JPEGIMAGE *pJPEG);
int JPEG_decode(JPEGIMAGE *pJPEG, int x, int y, int iOptions);
int JPEG_decodeDither(JPEGIMAGE *pJPEG, uint8_t *pDither, int iOptions);
int JPEG_getOrientation(JPEGIMAGE *pJPEG);
int JPEG_getWidth(JPEGIMAGE *pJPEG);
int JPEG_getHeight(JPEGIMAGE *pJPEG);
int JPEG_getBpp(JPEGIMAGE *pJPEG);
int JPEG_getSubSample(JPEGIMAGE *pJPEG);
int JPEG_hasThumb(JPEGIMAGE *pJPEG);
int JPEG_getThumbWidth(JPEGIMAGE *pJPEG);
int JPEG_getThumbHeight(JPEGIMAGE *pJPEG);
int JPEG_getLastError(JPEGIMAGE *pJPEG);
void JPEG_setPixelType(JPEGIMAGE *pJPEG, int iType);
void JPEG_setMaxOutputSize(JPEGIMAGE *pJPEG, int iMaxMCUs);
}

// C++ wrapper method implementations
int JPEGDEC::openRAM(uint8_t *pData, int iDataSize, JPEG_DRAW_CALLBACK *pfnDraw)
{
    return JPEG_openRAM(&_jpeg, pData, iDataSize, pfnDraw);
}

int JPEGDEC::openFLASH(const uint8_t *pData, int iDataSize, JPEG_DRAW_CALLBACK *pfnDraw)
{
    // openFLASH function not available in C implementation, using openRAM instead
    return JPEG_openRAM(&_jpeg, (uint8_t*)pData, iDataSize, pfnDraw);
}

int JPEGDEC::open(const char *szFilename, JPEG_OPEN_CALLBACK *pfnOpen, JPEG_CLOSE_CALLBACK *pfnClose, JPEG_READ_CALLBACK *pfnRead, JPEG_SEEK_CALLBACK *pfnSeek, JPEG_DRAW_CALLBACK *pfnDraw)
{
    // File open with callbacks not supported by this C implementation
    (void)pfnOpen; (void)pfnClose; (void)pfnRead; (void)pfnSeek; // silence warnings
    return JPEG_openFile(&_jpeg, szFilename, pfnDraw);
}

int JPEGDEC::open(void *fHandle, int iDataSize, JPEG_CLOSE_CALLBACK *pfnClose, JPEG_READ_CALLBACK *pfnRead, JPEG_SEEK_CALLBACK *pfnSeek, JPEG_DRAW_CALLBACK *pfnDraw)
{
    // Handle-based open not supported by this C implementation
    (void)fHandle; (void)iDataSize; (void)pfnClose; (void)pfnRead; (void)pfnSeek; (void)pfnDraw;
    return -1; // Not supported
}

void JPEGDEC::setFramebuffer(void *pFramebuffer)
{
    JPEG_setFramebuffer(&_jpeg, pFramebuffer);
}

void JPEGDEC::setCropArea(int x, int y, int w, int h)
{
    JPEG_setCropArea(&_jpeg, x, y, w, h);
}

void JPEGDEC::getCropArea(int *x, int *y, int *w, int *h)
{
    JPEG_getCropArea(&_jpeg, x, y, w, h);
}

void JPEGDEC::close()
{
    JPEG_close(&_jpeg);
}

int JPEGDEC::decode(int x, int y, int iOptions)
{
    return JPEG_decode(&_jpeg, x, y, iOptions);
}

int JPEGDEC::decodeDither(uint8_t *pDither, int iOptions)
{
    return JPEG_decodeDither(&_jpeg, pDither, iOptions);
}

int JPEGDEC::decodeDither(int x, int y, uint8_t *pDither, int iOptions)
{
    // decodeDitherXY not available, using regular decodeDither
    (void)x; (void)y; // silence warnings
    return JPEG_decodeDither(&_jpeg, pDither, iOptions);
}

int JPEGDEC::getOrientation()
{
    return JPEG_getOrientation(&_jpeg);
}

int JPEGDEC::getWidth()
{
    return JPEG_getWidth(&_jpeg);
}

int JPEGDEC::getHeight()
{
    return JPEG_getHeight(&_jpeg);
}

int JPEGDEC::getBpp()
{
    return JPEG_getBpp(&_jpeg);
}

void JPEGDEC::setUserPointer(void *p)
{
    // setUserPointer not available in this C implementation
    _jpeg.pUser = p; // Set directly in the JPEGIMAGE structure
}

int JPEGDEC::getSubSample()
{
    return JPEG_getSubSample(&_jpeg);
}

int JPEGDEC::getJPEGType()
{
    // getJPEGType not available in this C implementation
    return 0; // Default return value
}

int JPEGDEC::hasThumb()
{
    return JPEG_hasThumb(&_jpeg);
}

int JPEGDEC::getThumbWidth()
{
    return JPEG_getThumbWidth(&_jpeg);
}

int JPEGDEC::getThumbHeight()
{
    return JPEG_getThumbHeight(&_jpeg);
}

int JPEGDEC::getLastError()
{
    return JPEG_getLastError(&_jpeg);
}

void JPEGDEC::setPixelType(int iType)
{
    JPEG_setPixelType(&_jpeg, iType);
}

void JPEGDEC::setMaxOutputSize(int iMaxMCUs)
{
    JPEG_setMaxOutputSize(&_jpeg, iMaxMCUs);
}