#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Data.Pdf.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>

namespace {
constexpr int kMaxPages = 200;
constexpr int kMaxRenderedBytes = 64 * 1024 * 1024;

int render(const QString &path, const QString &directory)
{
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        using namespace winrt::Windows::Data::Pdf;
        using namespace winrt::Windows::Storage;
        using namespace winrt::Windows::Storage::Streams;
        const auto storageFile = StorageFile::GetFileFromPathAsync(
            winrt::hstring(QDir::toNativeSeparators(
                QFileInfo(path).absoluteFilePath()).toStdWString())).get();
        const PdfDocument pdf = PdfDocument::LoadFromFileAsync(storageFile).get();
        if (pdf.PageCount() == 0 || pdf.PageCount() > kMaxPages) {
            fputs("PDF page count is empty or exceeds 200\n", stderr);
            return 2;
        }
        QJsonArray pages;
        for (uint32_t index = 0; index < pdf.PageCount(); ++index) {
            const PdfPage page = pdf.GetPage(index);
            const auto size = page.Size();
            if (size.Width <= 0 || size.Height <= 0) {
                fprintf(stderr, "PDF page %u has invalid size\n", index + 1);
                return 3;
            }
            const double ratio = double(size.Width) / double(size.Height);
            const double pageWidth = 900.0 * ratio;
            if (!std::isfinite(pageWidth) || pageWidth < 100.0
                || pageWidth > 5000.0) {
                fprintf(stderr, "PDF page %u has unsupported ratio\n", index + 1);
                return 4;
            }
            const uint32_t width = static_cast<uint32_t>(
                std::max(1.0, std::min(1600.0, 1200.0 * ratio)));
            const uint32_t height = static_cast<uint32_t>(
                std::max(1.0, std::min(1200.0, double(width) / ratio)));
            PdfPageRenderOptions options;
            options.DestinationWidth(width);
            options.DestinationHeight(height);
            InMemoryRandomAccessStream stream;
            page.RenderToStreamAsync(stream, options).get();
            if (stream.Size() == 0 || stream.Size() > kMaxRenderedBytes) {
                fprintf(stderr, "PDF page %u rendered image is too large\n", index + 1);
                return 5;
            }
            QByteArray png(int(stream.Size()), '\0');
            DataReader reader(stream.GetInputStreamAt(0));
            reader.LoadAsync(uint32_t(png.size())).get();
            reader.ReadBytes({reinterpret_cast<uint8_t *>(png.data()),
                              reinterpret_cast<uint8_t *>(png.data()) + png.size()});
            const QImage image = QImage::fromData(png, "PNG");
            if (image.isNull()) {
                fprintf(stderr, "PDF page %u rendered image is invalid\n", index + 1);
                return 6;
            }
            const QString name = QStringLiteral("page-%1.png").arg(index + 1);
            if (!image.save(QDir(directory).filePath(name), "PNG")) {
                fprintf(stderr, "PDF page %u image cannot be saved\n", index + 1);
                return 7;
            }
            pages.append(QJsonObject{{QStringLiteral("file"), name},
                                     {QStringLiteral("width"), pageWidth}});
        }
        QSaveFile manifest(QDir(directory).filePath(QStringLiteral("pages.json")));
        if (!manifest.open(QIODevice::WriteOnly)
            || manifest.write(QJsonDocument(QJsonObject{
                {QStringLiteral("pages"), pages}}).toJson()) < 0
            || !manifest.commit()) {
            fputs("PDF page index cannot be saved\n", stderr);
            return 8;
        }
        return 0;
    } catch (const winrt::hresult_error &failure) {
        fprintf(stderr, "PDF rendering failed (0x%08x)\n",
                unsigned(failure.code()));
        return 9;
    }
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().size() != 3)
        return 1;
    return render(app.arguments().at(1), app.arguments().at(2));
}
