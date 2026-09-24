#include <QCoreApplication>
#include <QDir>
#include <QFile>
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
constexpr int kWrongPassword = 0x8007052B;

int render(const QString &path, const QString &directory, int firstPage,
           int lastPage, double scale, bool inspectOnly, const QString &password)
{
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        using namespace winrt::Windows::Data::Pdf;
        using namespace winrt::Windows::Storage;
        using namespace winrt::Windows::Storage::Streams;
        const auto storageFile = StorageFile::GetFileFromPathAsync(
            winrt::hstring(QDir::toNativeSeparators(
                QFileInfo(path).absoluteFilePath()).toStdWString())).get();
        const PdfDocument pdf = password.isEmpty()
            ? PdfDocument::LoadFromFileAsync(storageFile).get()
            : PdfDocument::LoadFromFileAsync(storageFile,
                winrt::hstring(password.toStdWString())).get();
        if (pdf.PageCount() == 0 || pdf.PageCount() > kMaxPages) {
            fputs("PDF 页数为空或超过 200 页\n", stderr);
            return 2;
        }
        if (inspectOnly) {
            fprintf(stdout, "COUNT %u\n", pdf.PageCount());
            return 0;
        }
        if (lastPage == 0)
            lastPage = int(pdf.PageCount());
        if (firstPage < 1 || lastPage < firstPage
            || lastPage > int(pdf.PageCount())
            || (scale != 1.0 && scale != 1.5 && scale != 2.0)) {
            fputs("PDF 页码范围或清晰度无效\n", stderr);
            return 10;
        }
        fprintf(stdout, "TOTAL %d\n", lastPage - firstPage + 1);
        fflush(stdout);
        QJsonArray pages;
        for (int sourcePage = firstPage; sourcePage <= lastPage; ++sourcePage) {
            const uint32_t index = uint32_t(sourcePage - 1);
            const int outputPage = sourcePage - firstPage + 1;
            const PdfPage page = pdf.GetPage(index);
            const auto size = page.Size();
            if (size.Width <= 0 || size.Height <= 0) {
                fprintf(stderr, "PDF 第 %u 页尺寸无效\n", index + 1);
                return 3;
            }
            const double ratio = double(size.Width) / double(size.Height);
            const double pageWidth = 900.0 * ratio;
            if (!std::isfinite(pageWidth) || pageWidth < 100.0
                || pageWidth > 5000.0) {
                fprintf(stderr, "PDF 第 %u 页比例超出支持范围\n", index + 1);
                return 4;
            }
            const double baseWidth = std::max(1.0,
                std::min(1600.0, 1200.0 * ratio));
            const double baseHeight = std::max(1.0,
                std::min(1200.0, baseWidth / ratio));
            const uint32_t width = static_cast<uint32_t>(scale * baseWidth);
            const uint32_t height = static_cast<uint32_t>(scale * baseHeight);
            PdfPageRenderOptions options;
            options.DestinationWidth(width);
            options.DestinationHeight(height);
            InMemoryRandomAccessStream stream;
            page.RenderToStreamAsync(stream, options).get();
            if (stream.Size() == 0 || stream.Size() > kMaxRenderedBytes) {
                fprintf(stderr, "PDF 第 %u 页渲染图像过大\n", index + 1);
                return 5;
            }
            QByteArray png(int(stream.Size()), '\0');
            DataReader reader(stream.GetInputStreamAt(0));
            reader.LoadAsync(uint32_t(png.size())).get();
            reader.ReadBytes({reinterpret_cast<uint8_t *>(png.data()),
                              reinterpret_cast<uint8_t *>(png.data()) + png.size()});
            const QImage image = QImage::fromData(png, "PNG");
            if (image.isNull()) {
                fprintf(stderr, "PDF 第 %u 页渲染图像无效\n", index + 1);
                return 6;
            }
            const QString name = QStringLiteral("page-%1.png").arg(outputPage);
            if (!image.save(QDir(directory).filePath(name), "PNG")) {
                fprintf(stderr, "PDF 第 %u 页图像无法保存\n", index + 1);
                return 7;
            }
            pages.append(QJsonObject{{QStringLiteral("file"), name},
                                     {QStringLiteral("width"), pageWidth},
                                     {QStringLiteral("sourcePage"), sourcePage}});
            fprintf(stdout, "PAGE %d\n", outputPage);
            fflush(stdout);
        }
        QSaveFile manifest(QDir(directory).filePath(QStringLiteral("pages.json")));
        if (!manifest.open(QIODevice::WriteOnly)
            || manifest.write(QJsonDocument(QJsonObject{
                {QStringLiteral("pages"), pages}}).toJson()) < 0
            || !manifest.commit()) {
            fputs("PDF 页面索引无法保存\n", stderr);
            return 8;
        }
        return 0;
    } catch (const winrt::hresult_error &failure) {
        if (unsigned(failure.code()) == unsigned(kWrongPassword)) {
            fputs("PASSWORD_REQUIRED\n", stderr);
            return 11;
        }
        fprintf(stderr, "PDF 打开或渲染失败（0x%08x）\n",
                unsigned(failure.code()));
        return 9;
    }
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QFile input;
    if (!input.open(stdin, QIODevice::ReadOnly))
        return 1;
    const QByteArray rawPassword = input.readAll();
    if (rawPassword.size() > 4096)
        return 1;
    const QString password = QString::fromUtf8(rawPassword);
    const QStringList args = app.arguments();
    if (args.size() == 3 && args.at(1) == QStringLiteral("--inspect"))
        return render(args.at(2), QString(), 1, 0, 1.0, true, password);
    if (args.size() != 6)
        return 1;
    bool firstOk = false;
    bool lastOk = false;
    bool scaleOk = false;
    const int first = args.at(3).toInt(&firstOk);
    const int last = args.at(4).toInt(&lastOk);
    const double scale = args.at(5).toDouble(&scaleOk);
    if (!firstOk || !lastOk || !scaleOk)
        return 1;
    return render(args.at(1), args.at(2), first, last, scale, false, password);
}
