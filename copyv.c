#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <libavformat/avformat.h>

double get_wall_time() {
  struct timespec time;
  clock_gettime(CLOCK_MONOTONIC, &time);
  return (double)time.tv_sec + (double)time.tv_nsec * 1e-9;
}

long get_file_size(const char *filename) {
  FILE *file = fopen(filename, "rb");
  if (file == NULL) {
    return -1;
  }

  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fclose(file);

  return size;
}

int main(int argc, char **argv) {

  if (argc < 3) {
    printf("Usage: ./copyv {input_file} {output_file}\n");
    return 1;
  }

  const char *in_filename = argv[1];
  const char *out_filename = argv[2];

  double rs = get_wall_time();

  AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
  AVPacket *pkt = av_packet_alloc();

  if (avformat_open_input(&ifmt_ctx, in_filename, NULL, NULL) < 0) {
    fprintf(stderr, "Error: Could not open input file '%s'\n", in_filename);
    return 1;
  }

  avformat_find_stream_info(ifmt_ctx, NULL);

  int video_stream_idx = -1;
  for (unsigned int i = 0; i < ifmt_ctx->nb_streams; i++) {
    if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      video_stream_idx = i;
      break;
    }
  }

  if (video_stream_idx == -1) {
    fprintf(stderr, "Error: Could not find video stream in input file\n");
    return 1;
  }

  AVStream *in_stream = ifmt_ctx->streams[video_stream_idx];
  AVCodecParameters *in_codecpar = in_stream->codecpar;

  int W = in_codecpar->width;
  int H = in_codecpar->height;

  AVRational fps = av_guess_frame_rate(ifmt_ctx, in_stream, NULL);
  double FPS = av_q2d(fps);
  if (FPS <= 0 || FPS > 1000) {
    FPS = 30.0;
  }

  printf("Input: %s (%dx%d, %.2f fps)\n", in_filename, W, H, FPS);

  avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);

  AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
  avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
  out_stream->codecpar->codec_tag = 0;

  avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
  avformat_write_header(ofmt_ctx, NULL);

  long frame_count = 0;
  while (av_read_frame(ifmt_ctx, pkt) >= 0) {
    if (pkt->stream_index == video_stream_idx) {
      av_packet_rescale_ts(pkt, in_stream->time_base, out_stream->time_base);
      pkt->stream_index = out_stream->index;
      av_interleaved_write_frame(ofmt_ctx, pkt);
      frame_count++;
    }
    av_packet_unref(pkt);
  }

  av_write_trailer(ofmt_ctx);

  double rdt = get_wall_time() - rs;

  av_packet_free(&pkt);
  avformat_close_input(&ifmt_ctx);
  avio_closep(&ofmt_ctx->pb);
  avformat_free_context(ofmt_ctx);

  long original_size = (long)(W * H * 1.5 * frame_count);
  long compressed_size = get_file_size(out_filename);

  printf("real time       : %.2f\n", rdt);
  printf("frames          : %ld\n", frame_count);
  printf("original size   : %ldB\n", original_size);
  printf("compressed size : %ldB\n", compressed_size);
  printf("compression ratio: %.2f\n", (double)original_size / compressed_size);

  return 0;
}