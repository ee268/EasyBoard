#include "ebpdfimporter.h"

#include <QFileInfo>
#include <QImage>

#include <algorithm>
#include <cmath>

#include "../domain/ebdocument.h"

#ifdef Q_OS_WIN
#include <winrt/Windows.Data.Pdf.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#endif

namespace {
constexpr qint64 kMaxPdfBytes = 256 * 1024 * 1024;
constexpr int kMaxPages = 200;
constexpr int kMaxRenderedBytes = 64 * 1024 * 1024;
}

bool EBPDFImporter::importFile(const QString &path, EBDocument *document,
                               QString *error)
{
    const QFileInfo file(path);
    if (!document || !file.isFile() || file.size() <= 0
        || file.size() > kMaxPdfBytes) {
        if (error)
            *error = QStringLiteral("PDF 文件不存在、为空或超过 256 MB");
        return false;
    }
#ifdef Q_OS_WIN
    try {
        // WinRT 的异步结果必须在工作线程等待，由调用方负责提供工作线程。
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        using namespace winrt::Windows::Data::Pdf;
        using namespace winrt::Windows::Storage;
        using namespace winrt::Windows::Storage::Streams;
        const auto storageFile = StorageFile::GetFileFromPathAsync(
            winrt::hstring(file.absoluteFilePath().toStdWString())).get();
        const PdfDocument pdf = PdfDocument::LoadFromFileAsync(storageFile).get();
        const uint32_t pageCount = pdf.PageCount();
        if (pageCount == 0 || pageCount > kMaxPages) {
            if (error)
                *error = QStringLiteral("PDF 页数为空或超过 200 页");
            return false;
        }

        EBDocument imported;
        imported.setTitle(file.completeBaseName());
        for (uint32_t index = 0; index < pageCount; ++index) {
            const PdfPage pdfPage = pdf.GetPage(index);
            const auto size = pdfPage.Size();
            if (size.Width <= 0 || size.Height <= 0) {
                if (error)
                    *error = QStringLiteral("第 %1 页尺寸无效").arg(index + 1);
                return false;
            }
            const double ratio = double(size.Width) / double(size.Height);
            const EBPage::Size pageSize =
                std::abs(ratio - 16.0 / 9.0) < std::abs(ratio - 4.0 / 3.0)
                    ? EBPage::Size::Widescreen : EBPage::Size::Standard;
            const double targetRatio = pageSize == EBPage::Size::Widescreen
                                           ? 16.0 / 9.0 : 4.0 / 3.0;
            const uint32_t width = static_cast<uint32_t>(
                std::max(1.0, std::min(1600.0, 1200.0 * ratio / targetRatio)));
            const uint32_t height = static_cast<uint32_t>(
                std::max(1.0, std::min(1200.0, double(width) / ratio)));
            PdfPageRenderOptions options;
            options.DestinationWidth(width);
            options.DestinationHeight(height);
            InMemoryRandomAccessStream stream;
            pdfPage.RenderToStreamAsync(stream, options).get();
            if (stream.Size() == 0 || stream.Size() > kMaxRenderedBytes) {
                if (error)
                    *error = QStringLiteral("第 %1 页渲染结果过大").arg(index + 1);
                return false;
            }
            QByteArray png(int(stream.Size()), '\0');
            DataReader reader = DataReader::CreateInputStreamReader(
                stream.GetInputStreamAt(0));
            reader.LoadAsync(uint32_t(png.size())).get();
            reader.ReadBytes({reinterpret_cast<uint8_t *>(png.data()),
                              reinterpret_cast<uint8_t *>(png.data()) + png.size()});
            const QImage image = QImage::fromData(png, "PNG");
            if (image.isNull()) {
                if (error)
                    *error = QStringLiteral("第 %1 页渲染失败").arg(index + 1);
                return false;
            }
            if (index > 0) {
                imported.addPage();
                imported.setCurrentPageIndex(int(index));
            }
            EBPage *page = imported.currentPage();
            page->setSize(pageSize);
            page->setBackgroundImage(image);
        }
        imported.setCurrentPageIndex(0);
        *document = imported;
        return true;
    } catch (const winrt::hresult_error &failure) {
        if (error)
            *error = QStringLiteral("PDF 无法打开或渲染（0x%1）")
                .arg(quint32(failure.code()), 8, 16, QLatin1Char('0'));
        return false;
    }
#else
    if (error)
        *error = QStringLiteral("当前系统不支持 PDF 导入");
    return false;
#endif
}
