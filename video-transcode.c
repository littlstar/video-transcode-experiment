
#include <stdio.h>
#include <stdlib.h>

#include <libavcodec/avcodec.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>

#define FRAMES_PER_SECOND 24

#define debug(fmt, ...) fprintf(stderr, "debug: " fmt "\n", ##__VA_ARGS__)
#define error(fmt, ...) fprintf(stderr, "error: " fmt "\n", ##__VA_ARGS__);

static void
transcode (const char *filename) {
  debug("transcoding `%s'", filename);

  FILE *file = NULL;

  AVCodec *codec = NULL;
  AVFrame *frame = NULL;
  AVCodecContext *ctx = NULL;

  int i = 0;
  int x = 0;
  int y = 0;

  size_t size = 0;
  size_t out_size = 0;
  size_t buf_size = 0;

  int *outbuf = NULL;
  uint8_t *framebuf = NULL;

  // fetch encoder
  debug("Fetching encoder");
  codec = avcodec_find_encoder(CODEC_ID_H264);
  if (NULL == codec) {
    error("Failed to find codec `CODEC_ID_MPEG1VIDEO'");
    exit(1);
  }

  // allocate codec context
  debug("Allocating context");
  ctx = avcodec_alloc_context3(codec);
  if (NULL == ctx) {
    error("Failed to allocate codec context");
    exit(1);
  }

  // allocate frame
  debug("Allocating frame");
  frame = av_frame_alloc();
  if (NULL == frame) {
    error("Failed to allocate frame");
    exit(1);
  }

  // initialize context
  debug("Initializing context");
  /*ctx->width = 352; // arbitrary
  ctx->height = 288; // arbitrary
  ctx->bit_rate = 4e5;
  ctx->gop_size = 10; // group of frame count with a single intra frame emission
  ctx->time_base = (AVRational) {1, 25}; // fps
  // see http://en.wikipedia.org/wiki/Video_compression_picture_types#Pictures.2FFrames
  ctx->max_b_frames = 1;
  ctx->pix_fmt = PIX_FMT_YUV420P; // pixel format
  */

  ctx->bit_rate = 500*1000;
  ctx->bit_rate_tolerance = 0;
  ctx->rc_max_rate = 0;
  ctx->rc_buffer_size = 0;
  ctx->gop_size = 40;
  ctx->max_b_frames = 3;
  ctx->b_frame_strategy = 1;
  ctx->coder_type = 1;
  ctx->me_cmp = 1;
  ctx->me_range = 16;
  ctx->qmin = 10;
  ctx->qmax = 51;
  ctx->scenechange_threshold = 40;
  ctx->flags |= CODEC_FLAG_LOOP_FILTER;
  ctx->me_method = ME_HEX;
  ctx->me_subpel_quality = 5;
  ctx->i_quant_factor = 0.71;
  ctx->qcompress = 0.6;
  ctx->max_qdiff = 4;
  //ctx->directpred = 1;
  //ctx->flags2 |= CODEC_FLAG2_FASTPSKIP;

  // initialize codec
  // see http://stackoverflow.com/questions/25171778/error-avcodec-open-was-not-declared-in-this-scope-on-attempting-to-compile
  debug("Initializing codec");
  if (avcodec_open2(ctx, codec, NULL) < 0) {
    error("Failed to open codec in context");
    exit(1);
  }

  // open source video
  debug("Opening source");
  file = fopen(filename, "wb");
  if (NULL == file) {
    error("Failed to open file `%s'", filename);
    exit(1);
  }

  // allocate output buffer
  debug("Allocating output buffer");
  out_size = 10e4;
  outbuf = (int *) malloc(out_size);

  // allocate frame buffer
  debug("Allocating frame buffer");
  size = ctx->width * ctx->height;
  // see http://en.wikipedia.org/wiki/YUV#Y.27UV420p_.28and_Y.27V12_or_YV12.29_to_RGB888_conversion
  framebuf = malloc((size * 3) / 2); // size of YUV420

  // initialize frame
  debug("Initializing frame");
  frame->data[0] = framebuf;
  frame->data[1] = frame->data[0] + size; // advance framebuf + size
  frame->data[2] = frame->data[1] + size / 4;
  frame->linesize[0] = ctx->width;
  frame->linesize[1] = ctx->width / 2;
  frame->linesize[2] = ctx->width / 2;

  // encode 1 second
  debug("Begin encoding");
  for (i = 0; i < FRAMES_PER_SECOND + 1; ++i) {
    debug("Encoding 1 second frame");
    // flush output
    fflush(stdout);

    // see http://en.wikipedia.org/wiki/YCbCr#CbCr_Planes_at_different_Y_values

    // draw Y
    debug("Drawing Y");
    for (y = 0; y < ctx->height; ++y) {
      // dray X for Y
      for (x = 0; x < ctx->width; ++x) {
        frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;
      }
    }

    // draw YCbCr
    debug("Drawing YCbCr");
    for (y = 0; y < ctx->height / 2; ++y) {
      for (x = 0; x < ctx->width / 2; ++x) {
        frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
        frame->data[2][y * frame->linesize[2] + x] = 64 + x + i * 5;
      }
    }

    debug("Initializing packet");
    // frame packet
    AVPacket packet;
    av_init_packet(&packet);
    packet.data = NULL;
    packet.size = 0;

    // encode frame image
    debug("Preparing frame image encode");
    out_size = avcodec_encode_video2(ctx, &packet, frame, outbuf);
    debug("Encoding frame %3d (size = %zu)", i, out_size);
    fwrite(outbuf, 1, out_size, file);
  }

  // write frames
  for (; out_size; ++i) {
    fflush(stdout);
    AVPacket packet;
    av_init_packet(&packet);
    packet.data = NULL;
    packet.size = 0;
    out_size = avcodec_encode_video2(ctx, &packet, NULL, outbuf);
    debug("Writing frame %3d (size = %zu)", i, out_size);
    fwrite(outbuf, 1, out_size, file);
  }

  debug("Writing end sequence header");
  // see http://dvd.sourceforge.net/dvdinfo/mpeghdrs.html
  outbuf[0] = 0x00;
  outbuf[1] = 0x00;
  outbuf[2] = 0x01;
  outbuf[3] = 0xb7; // end code

  // write end header
  fwrite(outbuf, 1, 4, file);

  // close file
  fclose(file);

  debug("Cleaning up");
  // free buffers
  free(framebuf);
  free(outbuf);

  // free context
  avcodec_close(ctx);
  av_free(ctx);

  // frame frame
  av_free(frame);
  printf("\n");
}

int
main (int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: video-transcode <file>\n");
    return 1;
  }

  // register all the codecs
  avcodec_register_all();

  // transcode video
  transcode(argv[1]);
  return 0;
}
