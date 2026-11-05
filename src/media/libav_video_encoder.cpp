#include "media/libav_video_encoder.hpp"

#include "stb/stb_image.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace gen {
namespace {

[[noreturn]] void fail(int status) { throw VideoEncodingError(status == 0 ? -1 : status); }

struct FormatDeleter {
  void operator()(AVFormatContext* format) const {
    if (format == nullptr) {
      return;
    }
    if (format->pb != nullptr) {
      avio_closep(&format->pb);
    }
    avformat_free_context(format);
  }
};

struct CodecDeleter {
  void operator()(AVCodecContext* codec) const { avcodec_free_context(&codec); }
};

struct FrameDeleter {
  void operator()(AVFrame* frame) const { av_frame_free(&frame); }
};

struct PacketDeleter {
  void operator()(AVPacket* packet) const { av_packet_free(&packet); }
};

struct ScalerDeleter {
  void operator()(SwsContext* scaler) const { sws_freeContext(scaler); }
};

struct LoadedImage {
  int width{0};
  int height{0};
  std::vector<unsigned char> rgba;
};

LoadedImage loadFrame(const std::filesystem::path& path) {
  int width = 0;
  int height = 0;
  int channels = 0;
  unsigned char* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
  if (pixels == nullptr || width <= 0 || height <= 0) {
    if (pixels != nullptr) {
      stbi_image_free(pixels);
    }
    fail(-1);
  }
  LoadedImage image;
  image.width = width;
  image.height = height;
  image.rgba.assign(pixels, pixels + static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
  stbi_image_free(pixels);
  return image;
}

bool fileContainsFtyp(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return false;
  }
  const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  return bytes.size() > 16 && bytes.find("ftyp") != std::string::npos;
}

void drain(AVFormatContext* format, AVCodecContext* codec, AVStream* stream, AVPacket* packet) {
  while (true) {
    const int status = avcodec_receive_packet(codec, packet);
    if (status == AVERROR(EAGAIN) || status == AVERROR_EOF) {
      return;
    }
    if (status < 0) {
      fail(status);
    }
    av_packet_rescale_ts(packet, codec->time_base, stream->time_base);
    packet->stream_index = stream->index;
    const int written = av_interleaved_write_frame(format, packet);
    av_packet_unref(packet);
    if (written < 0) {
      fail(written);
    }
  }
}

void sendFrame(AVFormatContext* format, AVCodecContext* codec, AVStream* stream, AVFrame* frame, AVPacket* packet) {
  while (true) {
    const int status = avcodec_send_frame(codec, frame);
    if (status == 0) {
      drain(format, codec, stream, packet);
      return;
    }
    if (status == AVERROR(EAGAIN)) {
      drain(format, codec, stream, packet);
      continue;
    }
    fail(status);
  }
}

std::unique_ptr<AVCodecContext, CodecDeleter> openCodec(const AVCodec* codec, int width, int height, int fps,
                                                               bool globalHeader) {
  if (codec == nullptr) {
    return nullptr;
  }
  std::unique_ptr<AVCodecContext, CodecDeleter> context(avcodec_alloc_context3(codec));
  if (!context) {
    return nullptr;
  }
  context->width = width;
  context->height = height;
  context->pix_fmt = AV_PIX_FMT_YUV420P;
  context->time_base = AVRational{1, fps};
  context->framerate = AVRational{fps, 1};
  context->gop_size = fps;
  context->max_b_frames = 0;
  if (globalHeader) {
    context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }
  if (codec->id == AV_CODEC_ID_H264 && context->priv_data != nullptr) {
    av_opt_set(context->priv_data, "preset", "ultrafast", 0);
    av_opt_set(context->priv_data, "tune", "zerolatency", 0);
  }
  if (avcodec_open2(context.get(), codec, nullptr) < 0) {
    return nullptr;
  }
  return context;
}

std::unique_ptr<AVCodecContext, CodecDeleter> openFirstCodec(int width, int height, int fps, bool globalHeader,
                                                            const AVCodec** chosen) {
  const AVCodecID ids[] = {AV_CODEC_ID_H264, AV_CODEC_ID_MPEG4};
  for (const AVCodecID id : ids) {
    const AVCodec* codec = avcodec_find_encoder(id);
    auto context = openCodec(codec, width, height, fps, globalHeader);
    if (context) {
      *chosen = codec;
      return context;
    }
  }
  return nullptr;
}

}  // namespace

void LibavVideoEncoder::encode(const std::vector<std::filesystem::path>& frames, int fps,
                               const std::filesystem::path& output) {
  if (frames.empty() || fps <= 0) {
    fail(-1);
  }

  const LoadedImage first = loadFrame(frames.front());
  if (first.width % 2 != 0 || first.height % 2 != 0) {
    fail(AVERROR(EINVAL));
  }

  AVFormatContext* rawFormat = nullptr;
  const int allocated = avformat_alloc_output_context2(&rawFormat, nullptr, "mp4", output.string().c_str());
  if (allocated < 0 || rawFormat == nullptr) {
    fail(allocated);
  }
  std::unique_ptr<AVFormatContext, FormatDeleter> format(rawFormat);

  AVStream* stream = avformat_new_stream(format.get(), nullptr);
  if (stream == nullptr) {
    fail(-1);
  }

  const AVCodec* codec = nullptr;
  const bool globalHeader = (format->oformat->flags & AVFMT_GLOBALHEADER) != 0;
  std::unique_ptr<AVCodecContext, CodecDeleter> codecContext =
      openFirstCodec(first.width, first.height, fps, globalHeader, &codec);
  if (!codecContext || codec == nullptr) {
    fail(-1);
  }
  const int parameters = avcodec_parameters_from_context(stream->codecpar, codecContext.get());
  if (parameters < 0) {
    fail(parameters);
  }
  stream->time_base = codecContext->time_base;

  const int io = avio_open(&format->pb, output.string().c_str(), AVIO_FLAG_WRITE);
  if (io < 0) {
    fail(io);
  }
  const int header = avformat_write_header(format.get(), nullptr);
  if (header < 0) {
    fail(header);
  }

  std::unique_ptr<SwsContext, ScalerDeleter> scaler(sws_getContext(
      first.width, first.height, AV_PIX_FMT_RGBA, first.width, first.height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr,
      nullptr, nullptr));
  std::unique_ptr<AVFrame, FrameDeleter> picture(av_frame_alloc());
  std::unique_ptr<AVPacket, PacketDeleter> packet(av_packet_alloc());
  if (!scaler || !picture || !packet) {
    fail(-1);
  }
  picture->format = AV_PIX_FMT_YUV420P;
  picture->width = first.width;
  picture->height = first.height;
  const int buffer = av_frame_get_buffer(picture.get(), 32);
  if (buffer < 0) {
    fail(buffer);
  }

  for (std::size_t index = 0; index < frames.size(); ++index) {
    const LoadedImage image = index == 0 ? first : loadFrame(frames[index]);
    if (image.width != first.width || image.height != first.height) {
      fail(-1);
    }
    const int writable = av_frame_make_writable(picture.get());
    if (writable < 0) {
      fail(writable);
    }
    const uint8_t* source[1] = {image.rgba.data()};
    const int sourceStride[1] = {image.width * 4};
    sws_scale(scaler.get(), source, sourceStride, 0, image.height, picture->data, picture->linesize);
    picture->pts = static_cast<int64_t>(index);
    sendFrame(format.get(), codecContext.get(), stream, picture.get(), packet.get());
  }
  sendFrame(format.get(), codecContext.get(), stream, nullptr, packet.get());

  const int trailer = av_write_trailer(format.get());
  if (trailer < 0) {
    fail(trailer);
  }
  format.reset();

  std::error_code error;
  const auto size = std::filesystem::file_size(output, error);
  if (error || size == 0 || !fileContainsFtyp(output)) {
    std::filesystem::remove(output, error);
    fail(-1);
  }
}

}  // namespace gen
