#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavutil/imgutils.h>

#include "decoder.h"

static bool decoder_end(decoder_t *it) {
  return it->_exhausted;
}

static bool decoder_close(decoder_t *it) {
  if (it == NULL) return false;
  avcodec_free_context(&it->dec_ctx);
  avformat_close_input(&it->fmt_ctx);
  av_packet_free(&it->pkt);
  av_frame_free(&it->frame);
  return true;
}

static uint8_t *decoder_next(decoder_t *it) {
  int ret;

  ret = avcodec_receive_frame(it->dec_ctx, it->frame);
  if (ret == 0)
    goto have_frame;

  while (av_read_frame(it->fmt_ctx, it->pkt) >= 0) {
    if (it->pkt->stream_index == (int)it->_vidx) {
      avcodec_send_packet(it->dec_ctx, it->pkt);
      av_packet_unref(it->pkt);

      ret = avcodec_receive_frame(it->dec_ctx, it->frame);
      if (ret == 0)
        goto have_frame;
    } else {
      av_packet_unref(it->pkt);
    }
  }

  avcodec_send_packet(it->dec_ctx, NULL);
  ret = avcodec_receive_frame(it->dec_ctx, it->frame);
  if (ret == 0)
    goto have_frame;

  it->_exhausted = true;
  return NULL;

have_frame:
  {
    int buf_size = av_image_get_buffer_size(
        it->dec_ctx->pix_fmt, it->width, it->height, 1);
    uint8_t *buf = (uint8_t *) malloc(buf_size);
    if (buf == NULL) return NULL;

    av_image_copy_to_buffer(buf, buf_size,
        (const uint8_t **) it->frame->data,
        (const int *) it->frame->linesize,
        it->dec_ctx->pix_fmt, it->width, it->height, 1);
    return buf;
  }
}

decoder_t *new_decoder(const char *fname) {
  decoder_t *it = (decoder_t *)malloc(sizeof(decoder_t));
  if (it == NULL) return NULL;
  memset(it, 0, sizeof(decoder_t));

  it->fname = strdup(fname);
  if (it->fname == NULL) { free(it); return NULL; }

  AVFormatContext *fmt_ctx = NULL;
  if (avformat_open_input(&fmt_ctx, fname, NULL, NULL) < 0) {
    fprintf(stderr, "could not open: %s\n", fname);
    free(it->fname); free(it);
    return NULL;
  }

  if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
    fprintf(stderr, "could not find stream info\n");
    avformat_close_input(&fmt_ctx);
    free(it->fname); free(it);
    return NULL;
  }

  int vidx = -1;
  for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
    if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      vidx = (int) i;
      break;
    }
  }

  if (vidx < 0) {
    fprintf(stderr, "no video stream found\n");
    avformat_close_input(&fmt_ctx);
    free(it->fname); free(it);
    return NULL;
  }

  AVStream *stream = fmt_ctx->streams[vidx];
  AVCodecParameters *cparams = stream->codecpar;

  const AVCodec *codec = avcodec_find_decoder(cparams->codec_id);
  if (codec == NULL) {
    fprintf(stderr, "unsupported codec\n");
    avformat_close_input(&fmt_ctx);
    free(it->fname); free(it);
    return NULL;
  }

  AVCodecContext *dec_ctx = avcodec_alloc_context3(codec);
  if (dec_ctx == NULL) {
    fprintf(stderr, "could not alloc codec context\n");
    avformat_close_input(&fmt_ctx);
    free(it->fname); free(it);
    return NULL;
  }

  avcodec_parameters_to_context(dec_ctx, cparams);

  if (avcodec_open2(dec_ctx, codec, NULL) < 0) {
    fprintf(stderr, "could not open decoder\n");
    avcodec_free_context(&dec_ctx);
    avformat_close_input(&fmt_ctx);
    free(it->fname); free(it);
    return NULL;
  }

  AVFrame *frame = av_frame_alloc();
  if (frame == NULL) {
    avcodec_free_context(&dec_ctx);
    avformat_close_input(&fmt_ctx);
    free(it->fname); free(it);
    return NULL;
  }

  AVPacket *pkt = av_packet_alloc();
  if (pkt == NULL) {
    av_frame_free(&frame);
    avcodec_free_context(&dec_ctx);
    avformat_close_input(&fmt_ctx);
    free(it->fname); free(it);
    return NULL;
  }

  AVRational fps_rat = av_guess_frame_rate(fmt_ctx, stream, NULL);
  double fps = av_q2d(fps_rat);
  if (fps <= 0.0 || fps > 1000.0) fps = 30.0;

  int64_t dur = stream->duration;
  if (dur <= 0) dur = fmt_ctx->duration;
  uint64_t nframes = (uint64_t) (av_q2d(stream->time_base) * dur * fps);
  if (nframes <= 0) nframes = 300;

  it->fps      = fps;
  it->duration = dur;
  it->frames   = nframes;
  it->width    = cparams->width;
  it->height   = cparams->height;
  it->_vidx    = (size_t) vidx;

  it->fmt_ctx  = fmt_ctx;
  it->stream   = stream;
  it->cparams  = cparams;
  it->dec_ctx  = dec_ctx;
  it->frame    = frame;
  it->pkt      = pkt;

  it->end   = decoder_end;
  it->close = decoder_close;
  it->next  = decoder_next;

  return it;
}

void free_decoder(decoder_t *it) {
  if (it == NULL) return;
  if (it->close) it->close(it);
  free(it->fname);
  free(it);
}
