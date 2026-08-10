/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2004 Chris Schoeneman
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "platform/MSWindowsClipboardBitmapConverter.h"

#include "base/Log.h"

#include <cstdint>
#include <limits>

namespace inputleap {

MSWindowsClipboardBitmapConverter::MSWindowsClipboardBitmapConverter()
{
    // do nothing
}

MSWindowsClipboardBitmapConverter::~MSWindowsClipboardBitmapConverter()
{
    // do nothing
}

IClipboard::EFormat
MSWindowsClipboardBitmapConverter::getFormat() const
{
    return IClipboard::kBitmap;
}

UINT
MSWindowsClipboardBitmapConverter::getWin32Format() const
{
    return CF_DIB;
}

HANDLE MSWindowsClipboardBitmapConverter::fromIClipboard(const std::string& data) const
{
    // copy to memory handle
    HGLOBAL gData = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, data.size());
    if (gData != nullptr) {
        // get a pointer to the allocated memory
        char* dst = (char*)GlobalLock(gData);
        if (dst != nullptr) {
            memcpy(dst, data.data(), data.size());
            GlobalUnlock(gData);
        }
        else {
            GlobalFree(gData);
            gData = nullptr;
        }
    }

    return gData;
}

std::string MSWindowsClipboardBitmapConverter::toIClipboard(HANDLE data) const
{
    // Clipboard data is supplied by other applications. Validate every size
    // before using it for pointer arithmetic or string allocation.
    LPVOID src = GlobalLock(data);
    if (src == nullptr) {
        return {};
    }
    const auto unlock = [&]() { GlobalUnlock(data); };
    const SIZE_T srcSize = GlobalSize(data);
    if (srcSize < sizeof(BITMAPINFOHEADER)) {
        unlock();
        return {};
    }

    // check image type
    const BITMAPINFO* bitmap = static_cast<const BITMAPINFO*>(src);
    if (bitmap->bmiHeader.biSize < sizeof(BITMAPINFOHEADER) ||
        bitmap->bmiHeader.biSize > srcSize) {
        unlock();
        return {};
    }
    LOG_INFO("bitmap: %dx%d %d", bitmap->bmiHeader.biWidth, bitmap->bmiHeader.biHeight, (int)bitmap->bmiHeader.biBitCount);
    if (bitmap->bmiHeader.biPlanes == 1 &&
        (bitmap->bmiHeader.biBitCount == 24 ||
        bitmap->bmiHeader.biBitCount == 32) &&
        bitmap->bmiHeader.biCompression == BI_RGB) {
        // already in canonical form
        std::string image(static_cast<char const*>(src), srcSize);
        unlock();
        return image;
    }

    // create a destination DIB section
    LOG_INFO("convert image from: depth=%d comp=%d", bitmap->bmiHeader.biBitCount, bitmap->bmiHeader.biCompression);
    const std::int64_t width = bitmap->bmiHeader.biWidth;
    const std::int64_t height = bitmap->bmiHeader.biHeight;
    if (width <= 0 || height == 0 ||
        height == std::numeric_limits<LONG>::min()) {
        unlock();
        return {};
    }

    // A negative DIB height means top-down pixels. Keep that orientation in
    // the output header, but use the positive pixel count for all sizes and
    // Win32 API dimensions.
    const auto pixelHeight = static_cast<std::uint64_t>(height < 0 ? -height : height);
    const auto pixelWidth = static_cast<std::uint64_t>(width);
    const auto bitsPerPixel = static_cast<std::uint64_t>(bitmap->bmiHeader.biBitCount);
    const auto checkedMultiply = [](std::uint64_t left, std::uint64_t right,
                                    std::uint64_t& result) {
        if (left != 0 && right > std::numeric_limits<std::uint64_t>::max() / left) {
            return false;
        }
        result = left * right;
        return true;
    };
    std::uint64_t sourceBitsPerRow = 0;
    std::uint64_t sourceRowBytes = 0;
    std::uint64_t sourcePixelBytes = 0;
    std::uint64_t destinationPixelBytes = 0;
    if (bitsPerPixel == 0 ||
        !checkedMultiply(pixelWidth, bitsPerPixel, sourceBitsPerRow) ||
        sourceBitsPerRow > std::numeric_limits<std::uint64_t>::max() - 31 ||
        !checkedMultiply((sourceBitsPerRow + 31) / 32, 4, sourceRowBytes) ||
        !checkedMultiply(sourceRowBytes, pixelHeight, sourcePixelBytes) ||
        !checkedMultiply(pixelWidth, pixelHeight, destinationPixelBytes) ||
        !checkedMultiply(destinationPixelBytes, 4, destinationPixelBytes) ||
        sourceRowBytes == 0 ||
        sourcePixelBytes > srcSize - bitmap->bmiHeader.biSize) {
        unlock();
        return {};
    }

    if (destinationPixelBytes >
        std::numeric_limits<std::size_t>::max() - sizeof(BITMAPINFOHEADER)) {
        unlock();
        return {};
    }

    void* raw = nullptr;
    BITMAPINFOHEADER info;
    const LONG w = static_cast<LONG>(width);
    const LONG h = bitmap->bmiHeader.biHeight;
    const LONG positiveHeight = static_cast<LONG>(pixelHeight);
    info.biSize          = sizeof(BITMAPINFOHEADER);
    info.biWidth         = w;
    info.biHeight        = h;
    info.biPlanes        = 1;
    info.biBitCount      = 32;
    info.biCompression   = BI_RGB;
    info.biSizeImage     = 0;
    info.biXPelsPerMeter = 1000;
    info.biYPelsPerMeter = 1000;
    info.biClrUsed       = 0;
    info.biClrImportant  = 0;
    HDC dc = GetDC(nullptr);
    if (dc == nullptr) {
        unlock();
        return {};
    }
    HBITMAP dst = CreateDIBSection(dc, (BITMAPINFO*)&info,
                                   DIB_RGB_COLORS, &raw, nullptr, 0);
    if (dst == nullptr || raw == nullptr) {
        ReleaseDC(nullptr, dc);
        unlock();
        return {};
    }

    // find the start of the pixel data
    SIZE_T pixelOffset = bitmap->bmiHeader.biSize;
    if (bitmap->bmiHeader.biBitCount >= 16) {
        if (bitmap->bmiHeader.biCompression == BI_BITFIELDS &&
            (bitmap->bmiHeader.biBitCount == 16 ||
            bitmap->bmiHeader.biBitCount == 32)) {
            pixelOffset += 3 * sizeof(DWORD);
        }
    }
    else if (bitmap->bmiHeader.biClrUsed != 0) {
        pixelOffset += bitmap->bmiHeader.biClrUsed * sizeof(RGBQUAD);
    }
    else {
        //http://msdn.microsoft.com/en-us/library/ke55d167(VS.80).aspx
        pixelOffset += (1i64 << bitmap->bmiHeader.biBitCount) * sizeof(RGBQUAD);
    }
    if (pixelOffset > srcSize || sourcePixelBytes > srcSize - pixelOffset) {
        DeleteObject(dst);
        ReleaseDC(nullptr, dc);
        unlock();
        return {};
    }
    const char* srcBits = static_cast<const char*>(src) + pixelOffset;

    // copy source image to destination image
    HDC dstDC         = CreateCompatibleDC(dc);
    if (dstDC == nullptr) {
        DeleteObject(dst);
        ReleaseDC(nullptr, dc);
        unlock();
        return {};
    }
    HGDIOBJ oldBitmap = SelectObject(dstDC, dst);
    if (oldBitmap == nullptr ||
        SetDIBitsToDevice(dstDC, 0, 0, w, positiveHeight, 0, 0, 0,
                          positiveHeight, srcBits, bitmap, DIB_RGB_COLORS) == 0) {
        if (oldBitmap != nullptr) {
            SelectObject(dstDC, oldBitmap);
        }
        DeleteDC(dstDC);
        DeleteObject(dst);
        ReleaseDC(nullptr, dc);
        unlock();
        return {};
    }
    SelectObject(dstDC, oldBitmap);
    DeleteDC(dstDC);
    GdiFlush();

    // extract data
    std::string image;
    try {
        image.assign(reinterpret_cast<const char*>(&info), info.biSize);
        image.append(static_cast<const char*>(raw),
                     static_cast<std::size_t>(destinationPixelBytes));
    }
    catch (...) {
        DeleteObject(dst);
        ReleaseDC(nullptr, dc);
        unlock();
        return {};
    }

    // clean up GDI
    DeleteObject(dst);
    ReleaseDC(nullptr, dc);

    // release handle
    unlock();

    return image;
}

} // namespace inputleap
