#pragma once

// Clipboard copy / paste of figures, in three formats (see ClipboardHelper).

#include <afx.h>
#include <exception>
#include <functional>

namespace Shos::MiniCadSample::Views {

// Copies figures to the clipboard and pastes them from it.
//
// Copy puts three formats on the clipboard at once:
//   - Enhanced metafile (CF_ENHMETAFILE) and bitmap (CF_DIB): images for other applications.
//   - The custom format (GetFormat()): the data written by store, read back by Paste.
// The callbacks keep this class independent of the model.
class ClipboardHelper
{
    // Closes the clipboard when it goes out of scope, even if an exception is thrown.
    class ClipboardCloser final
    {
    public:
        ClipboardCloser() = default;
        ~ClipboardCloser() { ::CloseClipboard(); }
        ClipboardCloser(const ClipboardCloser&)            = delete;
        ClipboardCloser& operator=(const ClipboardCloser&) = delete;
    };

public:
    // The custom clipboard format of this application.
    // Registered by name, so that it is unique in the system and the same in every instance.
    static UINT GetFormat()
    {
        static const UINT format = ::RegisterClipboardFormat(_T("Shos.MiniCadSample.Figures"));
        return format;
    }

    // Whether the clipboard has data that Paste can read.
    static bool CanPaste()
    {
        return ::IsClipboardFormatAvailable(GetFormat()) != FALSE;
    }

    // Copies to the clipboard.
    //   area  : the logical area to output. Its top-left corner becomes the origin of the images,
    //           and the images are clipped to it.
    //   store : writes the custom-format data.
    //   draw  : draws the content in logical coordinates (the coordinate transformation is done here).
    // Returns false if the clipboard could not be opened or area is empty.
    // Exceptions from store / draw propagate (the clipboard is closed).
    static bool Copy(CWnd& window, const CRect& area, COLORREF backgroundColor,
                     std::function<void(CArchive&)> store, std::function<void(CDC&)> draw)
    {
        if (area.IsRectEmpty())
            return false;

        // Made before opening the clipboard, so that a failure does not leave it emptied.
        const HGLOBAL data = CreateData(store);

        if (!window.OpenClipboard()) {
            ::GlobalFree(data);
            return false;
        }
        ClipboardCloser closer;
        ::EmptyClipboard();

        const auto drawInArea = [&](CDC& dc) {
            dc.SetViewportOrg(-area.left, -area.top); // area.TopLeft() is drawn at the origin.
            dc.IntersectClipRect(&area);
            draw(dc);
        };
        CopyMetaFileToClipboard(window, drawInArea);
        CopyImageToClipboard(area.Size(), backgroundColor, drawInArea);

        // On success, the clipboard owns data.
        if (::SetClipboardData(GetFormat(), data) == nullptr)
            ::GlobalFree(data);
        return true;
    }

    // Reads the custom-format data on the clipboard with load.
    // Returns false if there is no such data, or if it cannot be read (load threw CException* or std::exception);
    // load is expected to leave its target unchanged in that case.
    static bool Paste(CWnd& window, std::function<void(CArchive&)> load)
    {
        if (!window.OpenClipboard())
            return false;
        ClipboardCloser closer;

        // Owned by the clipboard: it must not be freed or grown here.
        const HANDLE data = ::GetClipboardData(GetFormat());
        if (data == nullptr)
            return false;

        CSharedFile file;
        file.SetHandle(static_cast<HGLOBAL>(data), FALSE);
        bool succeeded = false;
        try {
            Load(file, load);
            succeeded = true;
        } catch (CException* exception) {
            exception->Delete();
        } catch (const std::exception&) {
        }
        file.Detach(); // Gives the memory back without freeing it.
        return succeeded;
    }

private:
    // Serializes with store into a new global memory block (movable) and returns it. The caller owns it.
    static HGLOBAL CreateData(const std::function<void(CArchive&)>& store)
    {
        CSharedFile file; // Frees the memory if an exception is thrown before Detach.
        {
            CArchive ar(&file, CArchive::store);
            try {
                store(ar);
                ar.Close();
            } catch (...) {
                ar.Abort(); // Closing in the destructor would flush, which can throw again.
                throw;
            }
        }
        return file.Detach();
    }

    // Deserializes file with load. Exceptions propagate after the archive is aborted.
    static void Load(CSharedFile& file, const std::function<void(CArchive&)>& load)
    {
        CArchive ar(&file, CArchive::load);
        try {
            load(ar);
            ar.Close();
        } catch (...) {
            ar.Abort();
            throw;
        }
    }

    // Puts an enhanced metafile (vector image) drawn by draw on the clipboard. Silently does nothing on failure.
    static void CopyMetaFileToClipboard(CWnd& window, std::function<void(CDC&)> draw)
    {
        CClientDC   clientDC(&window);
        CMetaFileDC metaDC;
        if (!metaDC.CreateEnhanced(&clientDC, nullptr, nullptr, nullptr))
            return;
        draw(metaDC);
        const HENHMETAFILE enhancedMetaFileHandle = metaDC.CloseEnhanced();

        // On success, the clipboard owns the handle.
        if (::SetClipboardData(CF_ENHMETAFILE, enhancedMetaFileHandle) == nullptr)
            ::DeleteEnhMetaFile(enhancedMetaFileHandle);
    }

    // Creates a 24-bit image of size, filled with color (black is the initial color, so no fill is needed), and draws on it.
    static bool CreateImage(CImage& image, CSize size, std::function<void(CDC&)> draw, COLORREF color = RGB(0x00, 0x00, 0x00))
    {
        if (!image.Create(size.cx, size.cy, 24))
            return false;

        CDC* dc = CDC::FromHandle(image.GetDC());
        if (color != RGB(0x00, 0x00, 0x00)) {
            const CRect area(CPoint(), size);
            dc->FillSolidRect(&area, color);
        }
        draw(*dc);
        image.ReleaseDC();

        return true;
    }

    // Converts image to the CF_DIB layout: a BMP stream without BITMAPFILEHEADER (a DIB is
    // BITMAPINFOHEADER and the pixels). Returns a global memory block that the caller owns, or nullptr on failure.
    static HGLOBAL ToImageStream(const CImage& image)
    {
        IStream*        stream       = nullptr;
        HRESULT         result       = ::CreateStreamOnHGlobal(nullptr, TRUE, &stream);
        if (!SUCCEEDED(result))
            return nullptr;

        result                       = image.Save(stream, Gdiplus::ImageFormatBMP);
        if (!SUCCEEDED(result)) {
            stream->Release();
            return nullptr;
        }

        ULARGE_INTEGER  streamSize{};
        result                       = ::IStream_Size(stream, &streamSize);
        ASSERT(SUCCEEDED(result));

        const ULONG     streamLength = static_cast<ULONG>(streamSize.LowPart);
        result                       = ::IStream_Reset(stream);
        ASSERT(SUCCEEDED(result));

        // Skips the file header: reads it into the buffer and then overwrites it with the rest of the data.
        constexpr ULONG bitmapFileHeaderSize = static_cast<ULONG>(sizeof(BITMAPFILEHEADER));
        const ULONG     actualStreamLength   = streamLength - bitmapFileHeaderSize;
        const HGLOBAL   imageHandle          = ::GlobalAlloc(GHND, actualStreamLength);
        if (imageHandle == nullptr) {
            stream->Release();
            return nullptr;
        }

        auto        byteArray      = static_cast<BYTE*>(::GlobalLock(imageHandle));
        result                          = ::IStream_Read(stream, byteArray, bitmapFileHeaderSize);
        ASSERT(SUCCEEDED(result));

        result                           = ::IStream_Read(stream, byteArray, actualStreamLength);
        ASSERT(SUCCEEDED(result));
        ::GlobalUnlock(imageHandle);
        stream->Release();

        return imageHandle;
    }

    // Puts a bitmap (CF_DIB) of size drawn by draw on the clipboard. Returns false on failure.
    static bool CopyImageToClipboard(CSize size, COLORREF backgroundColor, std::function<void(CDC&)> draw)
    {
        CImage image;
        CreateImage(image, size, draw, backgroundColor);
        HGLOBAL imageStream = ToImageStream(image);
        if (imageStream == nullptr)
            return false;
        // On success, the clipboard owns imageStream.
        if (::SetClipboardData(CF_DIB, imageStream) == nullptr) {
            ::GlobalFree(imageStream);
            return false;
        }
        return true;
    }
};

} // namespace Shos::MiniCadSample::Views
