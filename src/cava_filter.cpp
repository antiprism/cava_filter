/*
  Copyright (c) 2022, Adrian Rossiter

  Antiprism - http://www.antiprism.com

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

      The above copyright notice and this permission notice shall be included
      in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.
*/

#include "programopts.hpp"
#include "utils.hpp"

#include "cavacore.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

class CavaFilter : public ProgramOpts {
  public:
    int bars_per_channel = 10;
    const int channels = 2;
    int channels_out = 1;
    const int height = 100;
    double framerate = 25;
    const size_t input_len_per_channel = 512;
    const int rate = 44100;
    int autosens = 0;
    double smooth_factor = 1; // 1: smooth, >1 more smooth, <1 less smooth
    int print_freq_bands = false;
    std::string ifile = "-";  // read from stdin
    std::string ofile = "-";  // write to stdout

    CavaFilter() : ProgramOpts("cava_filter") {}
    void process_command_line(int argc, char **argv);
    void usage();

    size_t input_len() const { return input_len_per_channel * channels; }
    size_t output_len() const { return bars_per_channel * channels; }
    double cava_smoothing() const
    {
      return smooth_factor * rate / input_len();
    }
    struct cava_plan *get_cava_plan()
    {
      return cava_init(bars_per_channel, rate, channels, height,
                       cava_smoothing(), autosens);
    }
    void print_freq_bands_line(struct cava_plan *plan) const
    {
      if (print_freq_bands) {
        for (int i = 0; i < bars_per_channel; i++) {
          printf("%4d ", (int)plan->cut_off_frequency[i]);
        }
        printf("\n");
      }
    }
    void print_freq_vals_line(double *frame_bars) const {
      int num_bars_out = bars_per_channel * channels_out;
      for (int i = 0; i < num_bars_out; i++) {
        double bar_ht =
            (channels_out == 2)
                ? frame_bars[i]
                : (frame_bars[i] + frame_bars[i + bars_per_channel]) / 2;
        printf("%4d ", (int)bar_ht);
      }
      printf("\n");
    }

    // discarding the fractional sample could lead to worst case
    // desynchronization of approx .1 seconds per hour
    int samples_per_frame() const { return (double)rate / framerate; }
};

void CavaFilter::usage()
{
  fprintf(stdout, R"(
Usage: %s -o [options] [input_file] [output_file]

cava_filter converts raw pcm_s16le format to frequency spectrum data. If
output_file is not given, write to standard output, if input file is also
not given, read from standard input.
  
  Options
  %s
  -b <num>   number of bars to print (default: 10)
  -f <hz>    framerate in Hz (default: 25)
  -S         stereo output, print the right channel bars followed on the line
             by the left channel bars
  -s <fact>  smooth factor <1.0 less smooth, >1.0 mode smooth (default: 1.0)
  -a <auto>  value for cava autosens setting
  -F         the first line printed is the frequencies of the bands
  )",
          get_program_name().c_str(), help_ver_text);
}

void CavaFilter::process_command_line(int argc, char **argv)
{
  opterr = 0;
  int c;

  handle_long_opts(argc, argv);

  while ((c = getopt(argc, argv, ":hb:f:Ss:a:F")) != -1) {
    if (common_opts(c, optopt))
      continue;

    switch (c) {
    case 'b':
      print_status_or_exit(read_int(optarg, &bars_per_channel), c);
      if (bars_per_channel < 2 || bars_per_channel > 200)
        error("select between 2 and 200 bars", c);
      break;

    case 'f':
      print_status_or_exit(read_double(optarg, &framerate), c);
      if (framerate < 1)
        error("framerate must be greater than 1", c);
      break;

    case 'S':
      channels_out = 2;
      break;

    case 's':
      print_status_or_exit(read_double(optarg, &smooth_factor), c);
      if (smooth_factor <= 0)
        error("smooth_factor must be a positive number", c);
      break;

    case 'a':
      print_status_or_exit(read_int(optarg, &autosens), c);
      if (autosens < 0)
        error("autosens cannot be negative (0 to disable)", c);
      break;

    case 'F':
      print_freq_bands = true;
      break;

    default:
      error("unknown command line error");
    }
  }

  if (argc - optind > 2)
    error("too many arguments");

  if (argc - optind > 1)
    ofile = argv[optind + 1];

  if (argc - optind > 0)
    ifile = argv[optind];
}

int main(int argc, char *argv[])
{
  CavaFilter cava;
  cava.process_command_line(argc, argv);

  auto *plan = cava.get_cava_plan();

  if (cava.print_freq_bands)
    cava.print_freq_bands_line(plan);

  size_t input_len = cava.input_len();   // samples per execute
  size_t output_len = cava.output_len();

  int16_t *cava_in_int16 = (int16_t *)malloc(input_len * sizeof(int16_t));
  double *cava_in = (double *)malloc(input_len * sizeof(double));
  double *cava_out = (double *)malloc(output_len * sizeof(double));
  double *frame_bars = (double *)malloc(output_len * sizeof(double));

  for (size_t i = 0; i < output_len; i++) {
    cava_out[i] = 0;
  }

  // number of full buffer reads per frame
  int reads_per_frame = cava.samples_per_frame() / input_len;
  // length of final buffer read
  int final_input_len = cava.samples_per_frame() - reads_per_frame * input_len;
  // exact number of buffer reads in floating point
  double reads_per_frame_exact = (double)cava.samples_per_frame() / input_len;
  // convert to total buffer reads per frame
  reads_per_frame = reads_per_frame + (final_input_len > 1);
  // total number of bar values in cava_out
  int bars_total = cava.output_len();

  int finished = 0; // has all the data been read (or error)
  while (!finished) {
    for (int bar_idx = 0; bar_idx < bars_total; bar_idx++)
      frame_bars[bar_idx] = 0;

    for (int read_idx = 0; read_idx < reads_per_frame; read_idx++) {
      // read full buffer, or partial buffer if it is a final partial read
      size_t read_len = input_len;
      if (read_idx == reads_per_frame - 1 && final_input_len)
        read_len = final_input_len;

      // input format pcm_s16le, converted with, e.g.
      // ffmpeg -i file.wav -f s16le -ar 44100 -acodec pcm_s16le -ac 2 file.raw
      size_t actual_num_read =
          fread(cava_in_int16, sizeof(int16_t), read_len, stdin);
      if (actual_num_read < read_len) { // end of stream or error
        finished = 1;
        break;
      }

      // convert samples to doubles for cava
      for (size_t i = 0; i < read_len; i++)
        cava_in[i] = (int)cava_in_int16[i];

      cava_execute(cava_in, read_len, cava_out, plan);

      // add weighted bar values
      double weight = (double)(read_len) / input_len;
      for (int bar_idx = 0; bar_idx < bars_total; bar_idx++)
        frame_bars[bar_idx] += weight * cava_out[bar_idx];
    }

    if (finished) // end of stream or error
      break;

    // weighted average of bars
    for (int i = 0; i < bars_total; i++)
      frame_bars[i] /= reads_per_frame_exact;

    // print average of L and R channels
    cava.print_freq_vals_line(frame_bars);
  }

  cava_destroy(plan);
  free(plan);
  free(cava_in);
  free(cava_out);
  free(frame_bars);

  return 0;
}
