#pragma once

#include <afx.h>
#include <functional>

class ClipboardHelper
{
    static HGLOBAL globalMemoryHandle;
	
public:
    static bool OnEditCopy(CDocument& document, CWnd& view, CSize size, COLORREF backgroundColor, std::function<void(CDC&)> draw)
    {
        if (!view.OpenClipboard())
            return false;
        ::EmptyClipboard();
        CopyMetaFileToClipboard(document, view, draw);
        CopyImageToClipboard(size, backgroundColor, draw);
        CopyDataToClipboard(document);
        ::CloseClipboard();
        return true;
    }

    static bool OnEditCut(CDocument& document, CWnd& view, CSize size, COLORREF backgroundColor, std::function<void(CDC&)> draw)
    {
        if (OnEditCopy(document, view, size, backgroundColor, draw)) {
			document.DeleteContents();
            view.Invalidate();
            return true;
        }
        return false;
    }

    static bool OnEditPaste(CDocument& document, CWnd& view)
    {
        if (!view.OpenClipboard())
			return false;
        if (AddDataFromClipboard(document)) {
            document.SetModifiedFlag();
            view.Invalidate();
        }
        ::CloseClipboard();
        return true;
    }
	
    static void OnDestroyClipboard()
    {
        // on WM_DESTROYCLIPBOARD
        if (globalMemoryHandle != nullptr) {
            ::GlobalUnlock(globalMemoryHandle);
            ::GlobalFree(globalMemoryHandle);
            globalMemoryHandle = nullptr;
        }
    }

private:
    static void CopyMetaFileToClipboard(CDocument& document, CWnd& view, std::function<void(CDC&)> draw)
    {
        CClientDC   clientDC(&view);
        CMetaFileDC metaDC;
        metaDC.CreateEnhanced(&clientDC, NULL, NULL, NULL);
        draw(metaDC);
        const HENHMETAFILE enhancedMetaFileHandle = metaDC.CloseEnhanced();

        ::SetClipboardData(CF_ENHMETAFILE, CopyEnhMetaFile(enhancedMetaFileHandle, NULL));
        ::DeleteEnhMetaFile(enhancedMetaFileHandle);
    }
	
    static void CopyDataToClipboard(CDocument& document)
    {
        CSharedFile sharedFile;
        {
            CArchive ar(&sharedFile, CArchive::store);
            document.Serialize(ar);
        }
        globalMemoryHandle = sharedFile.Detach();

        ::SetClipboardData(CF_PRIVATEFIRST, globalMemoryHandle);
    }

    static bool AddDataFromClipboard(CDocument& document)
    {
        const HANDLE clipboardData = ::GetClipboardData(CF_PRIVATEFIRST);
        if (clipboardData == nullptr)
            return false;

        CSharedFile sharedFile;
        sharedFile.SetHandle(clipboardData);
        {
            CArchive ar(&sharedFile, CArchive::load);
            document.Serialize(ar);
        }
        sharedFile.Detach();
        document.SetModifiedFlag();
        return true;
    }

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

    static HGLOBAL ToImageStream(const CImage& image)
    {
        IStream*        stream       = nullptr;
        HRESULT         result       = ::CreateStreamOnHGlobal(0, TRUE, &stream);
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

        const ULONG     streamLength = (ULONG)streamSize.LowPart;
        result                       = ::IStream_Reset(stream);
        ASSERT(SUCCEEDED(result));

        constexpr ULONG bitmapFileHeaderSize = (ULONG)sizeof(BITMAPFILEHEADER);
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

    static bool CopyImageToClipboard(CSize size, COLORREF backgroundColor, std::function<void(CDC&)> draw)
    {
        CImage image;
        CreateImage(image, size, draw, backgroundColor);
        HGLOBAL imageStream = ToImageStream(image);
        if (imageStream == nullptr)
            return false;
        ::SetClipboardData(CF_DIB, imageStream);
        return true;
    }
};
