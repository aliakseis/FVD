#include "imagecache.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>

#include <algorithm>
#include <streambuf>

#include "global_functions.h"
#include "player/ffmpegdecoder.h"
#include "settings_declaration.h"
#include "utilities/filesystem_utils.h"
#include "utilities/utils.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

namespace
{

class ByteStreamBuffer : public std::streambuf
{
public:
    ByteStreamBuffer(char* base, size_t length) { setg(base, base, base + length); }

protected:
    pos_type seekoff(off_type offset, std::ios_base::seekdir dir, std::ios_base::openmode) override
    {
        char* whence = eback();
        if (dir == std::ios_base::cur)
        {
            whence = gptr();
        }
        else if (dir == std::ios_base::end)
        {
            whence = egptr();
        }
        char* to = whence + offset;

        // check limits
        if (to >= eback() && to <= egptr())
        {
            setg(eback(), to, egptr());
            return gptr() - eback();
        }

        return -1;
    }
};


class IOContext
{
private:
    AVIOContext* ioCtx;
    uint8_t* buffer;  // internal buffer for ffmpeg
    int bufferSize;
    std::unique_ptr<std::streambuf> stream;

public:
    IOContext(std::unique_ptr<std::streambuf> s);
    ~IOContext();

    void initAVFormatContext(AVFormatContext* /*pCtx*/);

    static int IOReadFunc(void* data, uint8_t* buf, int buf_size);
    static int64_t IOSeekFunc(void* data, int64_t pos, int whence);
};

// static
int IOContext::IOReadFunc(void* data, uint8_t* buf, int buf_size)
{
    auto* hctx = static_cast<IOContext*>(data);
    auto len = hctx->stream->sgetn((char*)buf, buf_size);
    if (len <= 0)
    {
        // Let FFmpeg know that we have reached EOF, or do something else
        return AVERROR_EOF;
    }
    return static_cast<int>(len);
}

// whence: SEEK_SET, SEEK_CUR, SEEK_END (like fseek) and AVSEEK_SIZE
// static
int64_t IOContext::IOSeekFunc(void* data, int64_t pos, int whence)
{
    auto* hctx = static_cast<IOContext*>(data);

    if (whence == AVSEEK_SIZE)
    {
        // return the file size if you wish to
        auto current = hctx->stream->pubseekoff(0, std::ios_base::cur, std::ios_base::in);
        auto result = hctx->stream->pubseekoff(0, std::ios_base::end, std::ios_base::in);
        hctx->stream->pubseekoff(current, std::ios_base::beg, std::ios_base::in);
        return result;
    }

    std::ios_base::seekdir dir;
    switch (whence)
    {
    case SEEK_SET:
        dir = std::ios_base::beg;
        break;
    case SEEK_CUR:
        dir = std::ios_base::cur;
        break;
    case SEEK_END:
        dir = std::ios_base::end;
        break;
    default:
        return -1LL;
    }

    return hctx->stream->pubseekoff(pos, dir);
}

IOContext::IOContext(std::unique_ptr<std::streambuf> s) : stream(std::move(s))
{
    // allocate buffer
    bufferSize = 1024 * 64;                                 // FIXME: not sure what size to use
    buffer = static_cast<uint8_t*>(av_malloc(bufferSize));  // see destructor for details

    // allocate the AVIOContext
    ioCtx = avio_alloc_context(buffer, bufferSize,  // internal buffer and its size
                               0,                   // write flag (1=true,0=false)
                               (void*)this,         // user data, will be passed to our callback functions
                               IOReadFunc,
                               nullptr,  // no writing
                               IOSeekFunc);
}

IOContext::~IOContext()
{
    //CHANNEL_LOG(ffmpeg_closing) << "In IOContext::~IOContext()";

    // NOTE: ffmpeg messes up the buffer
    // so free the buffer first then free the context
    av_free(ioCtx->buffer);
    ioCtx->buffer = nullptr;
    av_free(ioCtx);
}

void IOContext::initAVFormatContext(AVFormatContext* pCtx)
{
    pCtx->pb = ioCtx;
    pCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

    // you can specify a format directly
    // pCtx->iformat = av_find_input_format("h264");

    // or read some of the file and let ffmpeg do the guessing
    auto len = stream->sgetn((char*)buffer, bufferSize);
    if (len <= 0)
    {
        return;
    }
    // reset to beginning of file
    stream->pubseekoff(0, std::ios_base::beg, std::ios_base::in);

    AVProbeData probeData = {nullptr};
    probeData.buf = buffer;
    probeData.buf_size = bufferSize - 1;
    probeData.filename = "";
    pCtx->iformat = av_probe_input_format(&probeData, 1);
}


QImage getImage(QByteArray& imageArray)
{
    IOContext ioCtx(std::make_unique<ByteStreamBuffer>(imageArray.data(), imageArray.size())); 

    // Open the image file.
    AVFormatContext* pFormatCtx = avformat_alloc_context();

    ioCtx.initAVFormatContext(pFormatCtx);

    if (avformat_open_input(&pFormatCtx, NULL, NULL, NULL) != 0 
        || avformat_find_stream_info(pFormatCtx, NULL) < 0)
    {
        avformat_close_input(&pFormatCtx);
        return {};
    }

    // Find the first video stream.
    int videoStream = -1;
    for (unsigned i = 0; i < pFormatCtx->nb_streams; i++)
    {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStream = i;
            break;
        }
    }

    if (videoStream == -1)
    {
        avformat_close_input(&pFormatCtx);
        return {};
    }

    // Get a pointer to the codec context for the video stream.
    AVCodecContext* pCodecCtx = avcodec_alloc_context3(NULL);
    avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[videoStream]->codecpar);

    // Find the decoder for the video stream.
    auto pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == NULL || avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
    {
        avcodec_close(pCodecCtx);
        avformat_close_input(&pFormatCtx);
        return {};
    }

    // Allocate video frame.
    AVFrame* pFrame = av_frame_alloc();

    // Read the image.
    QImage result;

    bool stop = false;
    while (!stop)
    {
        AVPacket packet;

        if (av_read_frame(pFormatCtx, &packet) < 0)
        {
            // flush
            memset(&packet, 0, sizeof(packet));
            packet.stream_index = videoStream;
            packet.pts = AV_NOPTS_VALUE;
            packet.dts = AV_NOPTS_VALUE;

            stop = true;
        }

        if (packet.stream_index != videoStream)
        {
            av_packet_unref(&packet);
            continue;
        }

        const int ret = avcodec_send_packet(pCodecCtx, &packet);

        av_packet_unref(&packet);

        if (ret < 0)
        {
            break;
        }

        while (avcodec_receive_frame(pCodecCtx, pFrame) == 0)
        {
            // The frame is now decoded and can be processed.
            result = QImage(pFrame->width, pFrame->height, QImage::Format_RGB888);

            const auto data = result.bits();
            const int stride = result.bytesPerLine();

            auto img_convert_ctx = sws_getCachedContext(
                NULL, pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width,
                                     pCodecCtx->height,
                                     AV_PIX_FMT_BGR24, SWS_FAST_BILINEAR, NULL, NULL, NULL);
            sws_scale(img_convert_ctx, pFrame->data, pFrame->linesize, 0,
                      pCodecCtx->height,
                      &data,
                      &stride);

            stop = true;
            break;
        }
    }

    // Clean up.
    av_free(pFrame);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);

    return result;
}

int doGetRandomFrame(const QString& filePath, const QString& imageFilePath)
{
    if (imageFilePath.isEmpty())
    {
        return 1;
    }

    if (QDir(filePath).exists())
    {
        const auto videoFilesMask = global_functions::GetVideoFileExts();
        QDirIterator it(filePath, videoFilesMask, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext())
        {
            auto p = it.next();
            FFmpegDecoder decoder;
            auto image = decoder.getRandomFrame(p);
            if (!image.isEmpty())
            {
                QFile imageFile(imageFilePath);
                if (imageFile.open(QIODevice::WriteOnly))
                {
                    imageFile.write(image);
                    imageFile.close();
                    return 0;
                }
            }
        }
    }
    else if (QFile::exists(filePath))
    {
        FFmpegDecoder decoder;
        auto image = decoder.getRandomFrame(filePath);
        if (!image.isEmpty())
        {
            QFile imageFile(imageFilePath);
            if (imageFile.open(QIODevice::WriteOnly))
            {
                imageFile.write(image);
                imageFile.close();
                return 0;
            }
        }
    }

    return 1;
}

}  // namespace

int getRandomFrame() { return doGetRandomFrame(QApplication::arguments().at(2), QApplication::arguments().at(3)); }

const QString ImageCache::imagesCacheDir = "images";

const char propImageFilePath[] = "imageFilePath";

ImageCache::ImageCache() : m_networkImageManager(nullptr), m_isAsync(false), m_lastReply(nullptr) {}

ImageCache::~ImageCache() = default;

void ImageCache::getAsync(const QString& id, const QString& url, const QString& filePath)
{
    m_isAsync = true;
    get(id, url, filePath);
}

QImage ImageCache::getSync(const QString& id, const QString& url, const QString& filePath)
{
    m_isAsync = false;
    get(id, url, filePath);
    return m_image;
}

void ImageCache::get(const QString& id, const QString& url, const QString& filePath)
{
    QString imageFilePath;

    if (!id.isEmpty())
    {
        imageFilePath = utilities::PrepareCacheFolder(imagesCacheDir) + fileNameFromId(id);
        if (QFile::exists(imageFilePath))
        {
            if (m_image.load(imageFilePath))
            {
                if (m_isAsync)
                {
                    emit getImageFinished(m_image);
                }
                return;
            }
        }
    }
    if (!url.isEmpty())
    {
        if (nullptr == m_networkImageManager)
        {
            m_networkImageManager = new QNetworkAccessManager(this);
            VERIFY(
                connect(m_networkImageManager, SIGNAL(finished(QNetworkReply*)), SLOT(imageReceived(QNetworkReply*))));
        }
        m_lastReply = m_networkImageManager->get(QNetworkRequest(url));
        if (!imageFilePath.isEmpty())
        {
            m_lastReply->setProperty(propImageFilePath, imageFilePath);
        }
        if (!m_isAsync)
        {
            m_eventLoop.exec();
        }
        return;
    }
    if (!filePath.isEmpty())
    {
        if (QFile::exists(filePath))
        {
            auto it = m_missingLastModified.find(id);
            if (it != m_missingLastModified.end() && it->second <= QFileInfo(filePath).lastModified())
            {
                QImage().swap(m_image);  // reset
            }
            else
            {
                //*
                const int status =
                    QProcess::execute(QCoreApplication::applicationFilePath(),
                                      QStringList() << GET_RANDOM_FRAME_OPTION << filePath << imageFilePath);
                //*/
                // const int status = doGetRandomFrame(filePath, imageFilePath);
                if (0 == status)
                {
                    m_image.load(imageFilePath);
                    m_missingLastModified.erase(id);
                }
                else
                {
                    QImage().swap(m_image);  // reset
                    m_missingLastModified.insert({id, QFileInfo(filePath).lastModified()});
                }
            }
        }
        else
        {
            emit FileMissingSignaller::Instance().fileMissing(filePath);
        }
    }
    if (m_isAsync)
    {
        emit getImageFinished(m_image);
    }
}

/* static */ QString ImageCache::fileNameFromId(QString id)
{
    return id.remove('\"').replace(QRegExp("[/\\\\:*?<>|]"), "_").right(80) + ".jpg";
}

void ImageCache::imageReceived(QNetworkReply* reply)
{
    const quint64 MAX_SIZE = 1024 * 1024;
    QByteArray imageArray(reply->read(MAX_SIZE));
    const bool valid = !imageArray.isEmpty() && (uint)imageArray.size() < MAX_SIZE 
        && (m_image.loadFromData(imageArray) || (m_image = getImage(imageArray), !m_image.isNull()));
    auto imageFilePath = reply->property(propImageFilePath).value<QString>();
    if (valid)
    {
        if (!imageFilePath.isEmpty())
        {
            m_image.save(imageFilePath, "jpg");
        }
    }
    else
    {
        m_image = QImage();  // reset

        imageArray.truncate(512);
        qDebug() << "Incorrect image data:" << imageArray;
        utilities::PrintReplyHeader(reply);
    }

    if (m_isAsync && (m_lastReply == reply))
    {
        emit getImageFinished(m_image);
    }
    else
    {
        m_eventLoop.quit();
    }
}

/* static */ bool ImageCache::clear(const QString& id)
{
    return QFile::remove(utilities::PrepareCacheFolder(imagesCacheDir) + fileNameFromId(id));
}

/* static */ void ImageCache::clearAll() { QDir(utilities::PrepareCacheFolder(imagesCacheDir)).removeRecursively(); }
