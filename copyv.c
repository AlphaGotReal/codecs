#include <stdio.h>
#include <stdlib.h>
#include <libavformat/avformat.h>

/*
AVFormatContext
*/

int main(int argc, char **argv) {

  if (argc < 3) {
    printf("./copyv {input} {output}\n");
    return 1;
  }

  AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
  AVPacket *pkt = av_packet_alloc();

  avformat_open_input(&ifmt_ctx, argv[1], NULL, NULL);
  avformat_find_stream_info(ifmt_ctx, NULL);

  avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, "output.mp4");
  AVStream *in_stream = ifmt_ctx->streams[0];
  AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
  avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
  out_stream->codecpar->codec_tag = 0;

  avio_open(&ofmt_ctx->pb, argv[2], AVIO_FLAG_WRITE);
  avformat_write_header(ofmt_ctx, NULL);
  while (av_read_frame(ifmt_ctx, pkt) >= 0) {
    if (pkt->stream_index == 0) {
      av_packet_rescale_ts(pkt, in_stream->time_base, out_stream->time_base);
      pkt->stream_index = 0; 
      av_interleaved_write_frame(ofmt_ctx, pkt);
    }
    av_packet_unref(pkt); 
  }

  av_write_trailer(ofmt_ctx);
  av_packet_free(&pkt);
  avformat_close_input(&ifmt_ctx);
  avio_closep(&ofmt_ctx->pb);
  avformat_free_context(ofmt_ctx);

  return 0;
}
