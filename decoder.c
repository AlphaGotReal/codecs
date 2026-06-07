#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#include "decoder.h"
#include "pixfmt.h"

static bool dend(decoder_t *it) {
  return it->_exhausted;
}

static bool dclose(decoder_t *it) {
  if (it == NULL) return false;
  sws_freeContext(it->sws_ctx);
  avcodec_free_context(&it->dec_ctx);
  avformat_close_input(&it->fmt_ctx);
  av_packet_free(&it->pkt);
  av_frame_free(&it->frame);
  return true;
}

static uint8_t *dnext(decoder_t *it) {
  if (it->_exhausted) return NULL;

  int ret;

  ret = avcodec_receive_frame(it->dec_ctx, it->frame);
  if (ret == 0)
    goto have_frame;
  if (ret == AVERROR_EOF) goto eof;

  while (av_read_frame(it->fmt_ctx, it->pkt) >= 0) {
    if (it->pkt->stream_index == (int)it->_vidx) {
      avcodec_send_packet(it->dec_ctx, it->pkt);
      av_packet_unref(it->pkt);

      ret = avcodec_receive_frame(it->dec_ctx, it->frame);
      if (ret == 0)
        goto have_frame;
      if (ret == AVERROR_EOF) goto eof;
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

eof: 
  it->_exhausted = true;
  return NULL;

have_frame:
  {
    enum AVPixelFormat src_fmt = it->dec_ctx->pix_fmt;
    enum AVPixelFormat dst_fmt = it->target_fmt;

    if (dst_fmt != AV_PIX_FMT_NONE && dst_fmt != src_fmt) {
      AVFrame *tmp = av_frame_alloc();
      if (tmp == NULL) return NULL;

      tmp->format = dst_fmt;
      tmp->width  = it->width;
      tmp->height = it->height;
      av_frame_get_buffer(tmp, 0);

      sws_scale(it->sws_ctx,
          (const uint8_t *const *)it->frame->data, it->frame->linesize,
          0, it->height,
          tmp->data, tmp->linesize);

      int buf_size = av_image_get_buffer_size(dst_fmt, it->width, it->height, 1);
      if (buf_size < 0) { av_frame_free(&tmp); av_frame_unref(it->frame); return NULL; }

      uint8_t *buf = (uint8_t *) malloc(buf_size);
      if (buf == NULL) { av_frame_free(&tmp); av_frame_unref(it->frame); return NULL; }

      av_image_copy_to_buffer(buf, buf_size,
          (const uint8_t **)tmp->data, (const int *)tmp->linesize,
          dst_fmt, it->width, it->height, 1);

      av_frame_free(&tmp);
      av_frame_unref(it->frame);
      return buf;
    }

    int buf_size = av_image_get_buffer_size(src_fmt, it->width, it->height, 1);
    if (buf_size < 0) return NULL;

    uint8_t *buf = (uint8_t *) malloc(buf_size);
    if (buf == NULL) return NULL;

    av_image_copy_to_buffer(buf, buf_size,
        (const uint8_t **) it->frame->data,
        (const int *) it->frame->linesize,
        src_fmt, it->width, it->height, 1);

    av_frame_unref(it->frame);
    return buf;
  }
}

decoder_t *new_decoder(const char *fname, const char *fmt) {
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
  uint64_t nframes = 0;
  if (dur > 0) {
    nframes = (uint64_t)(av_q2d(stream->time_base) * dur * fps);
  } else {
    dur = fmt_ctx->duration;
    if (dur > 0) {
      nframes = (uint64_t)(((double)dur / AV_TIME_BASE) * fps);
    }
  }

  it->fps      = fps;
  it->duration = dur;
  it->frames   = nframes;
  it->width    = cparams->width;
  it->height   = cparams->height;
  it->_vidx    = (size_t) vidx;
  it->target_fmt = AV_PIX_FMT_NONE;

  if (fmt != NULL) {
    init_pixfmt();
    void *f = non_static_call(pixfmt, find, fmt);
    if (f != NULL) {
      it->target_fmt = (enum AVPixelFormat)(intptr_t)f;
      if (it->target_fmt != dec_ctx->pix_fmt) {
        it->sws_ctx = sws_getContext(it->width, it->height, dec_ctx->pix_fmt,
                                      it->width, it->height, it->target_fmt,
                                      SWS_BILINEAR, NULL, NULL, NULL);
        if (it->sws_ctx == NULL) {
          fprintf(stderr, "could not create sws context\n");
          avcodec_free_context(&dec_ctx);
          avformat_close_input(&fmt_ctx);
          av_frame_free(&frame);
          av_packet_free(&pkt);
          free(it->fname); free(it);
          return NULL;
        }
      }
    }
  }

  it->fmt_ctx  = fmt_ctx;
  it->stream   = stream;
  it->cparams  = cparams;
  it->dec_ctx  = dec_ctx;
  it->frame    = frame;
  it->pkt      = pkt;

  it->end   = dend;
  it->close = dclose;
  it->next  = dnext;

  return it;
}

void free_decoder(decoder_t *it) {
  if (it == NULL) return;
  if (it->close) it->close(it);
  free(it->fname);
  free(it);
}
