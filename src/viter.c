#include <stdio.h>
#include <stdlib.h>

#include "viter.h"

viter_t *new_viter(const char *in_filename) {

  static int errno;

  viter_t *it = (viter_t *) malloc(sizeof(viter_t));

  it->in_filename = in_filename;
  it->format_ctx  = NULL;

  errno = avformat_open_input(
    &(it->format_ctx), in_filename, NULL, NULL);

  if (errno != 0) {
    fprintf(stderr, "could not open file %s\n", in_filename);
    free(it);
    return NULL;
  }

  errno = avformat_find_stream_info(it->format_ctx, NULL); 
  if (errno < 0) {
    fprintf(stderr, "could not retrieve stream info\n");
    free(it->format_ctx);
    free(it);
    return NULL;
  }

  int vidx = -1; // first video index
  const AVCodec *codec = NULL;
  AVCodecParameters *codec_params = NULL;
  AVCodecParameters *tmp;

  for (int t = 0; t < it->format_ctx->nb_streams; ++t) {
    tmp = it->format_ctx->streams[t]->codecpar;
    if (tmp->codec_type == AVMEDIA_TYPE_VIDEO) {
      vidx         = t;
      codec        = avcodec_find_decoder(tmp->codec_id);
      codec_params = tmp;
      break;
    }
  }

  if (vidx == -1) {
    fprintf(stderr, "no video streams found in the video\n");
    free(it->format_ctx);
    free(it);
    free(codec);
    free(codec_params);
    free(tmp);
    return NULL;
  }

  return it;
}
