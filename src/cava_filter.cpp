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
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
  std::string in_file_name = "-";  // read from stdin
  std::string out_file_name = "-"; // write to stdout
  FILE *in_file = stdin;
  FILE *out_file = stdout;

  CavaFilter() : ProgramOpts("cava_filter") {}
  void process_command_line(int argc, char **argv);
  void usage();

  size_t input_len() const { return input_len_per_channel * channels; }
  size_t output_len() const { return bars_per_channel * channels; }
  double cava_smoothing() const { return smooth_factor * rate / input_len(); }
  struct cava_plan *get_cava_plan()
  {
    return cava_init(bars_per_channel, rate, channels, height, cava_smoothing(),
                     autosens);
  }
  void print_freq_bands_line(struct cava_plan *plan) const
  {
    if (print_freq_bands) {
      for (int ch = 0; ch < channels_out; ch++)
        for (int i = 0; i < bars_per_channel; i++)
          fprintf(out_file, "%4d ", (int)plan->cut_off_frequency[i]);
      fprintf(out_file, "\n");
    }
  }
  void print_freq_vals_line(double *frame_bars) const
  {
    int num_bars_out = bars_per_channel * channels_out;
    for (int i = 0; i < num_bars_out; i++) {
      double bar_ht =
          (channels_out == 2)
              ? frame_bars[i]
              : (frame_bars[i] + frame_bars[i + bars_per_channel]) / 2;
      fprintf(out_file, "%4d ", (int)bar_ht);
    }
    fprintf(out_file, "\n");
  }

  // discarding the fractional sample could lead to worst case
  // desynchronization of approx .1 seconds per hour
  int samples_per_frame() const
  {
    return (double)(rate * channels) / framerate;
  }

  FILE *get_in_file() const { return in_file; }
  FILE *get_out_file() const { return out_file; }
  void close_files()
  {
    if (in_file != stdin)
      fclose(in_file);
    if (out_file != stdout)
      fclose(out_file);
  }
};

void CavaFilter::usage()
{
  fprintf(stdout, R"(
Usage: %s [options] [input_file]

convert raw pcm_s16le format to frequency spectrum data using the cavacore
library https://github.com/karlstav/cava. If input_file is not given the
program reads from standard input.

  Options
  %s
  -b <num>   number of bars to print (default: 10)
  -f <hz>    framerate in Hz (default: 25)
  -S         stereo output, print the right channel bars followed on the line
             by the left channel bars
  -s <fact>  smooth factor, a number greater than 0.1 (not smooth) to set
             spectrum smoothness (default: 1)
  -a <auto>  value for the cava autosens setting (default: 0 no autosens)
  -F         the first line printed is the frequencies of the bands
  -o <file>  write output to file (default: write to standard output)

  )",
          get_program_name().c_str(), help_ver_text);
}

void CavaFilter::process_command_line(int argc, char **argv)
{
  opterr = 0;
  int c;

  handle_long_opts(argc, argv);

  while ((c = getopt(argc, argv, ":ho:b:f:Ss:a:F")) != -1) {
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

    case 'o':
      out_file_name = optarg;
      break;

    default:
      error("unknown command line error");
    }
  }

  if (argc - optind > 1)
    error("too many arguments");

  if (argc - optind == 1)
    in_file_name = argv[optind];

  if (in_file_name == "-") {
    in_file = stdin;
  }
  else {
    in_file = fopen(in_file_name.c_str(), "r");
    if (!in_file)
      error("could not open file for reading '" + in_file_name +
            "': " + strerror(errno));
  }

  if (out_file_name == "-") {
    out_file = stdout;
  }
  else {
    out_file = fopen(out_file_name.c_str(), "w");
    if (!out_file)
      error("could not open file for writing '" + out_file_name +
            "': " + strerror(errno));
  }
}

int main(int argc, char *argv[])
{
  CavaFilter cava;
  cava.process_command_line(argc, argv);

  auto *plan = cava.get_cava_plan();

  if (cava.print_freq_bands)
    cava.print_freq_bands_line(plan);

  size_t input_len = cava.input_len(); // samples per execute
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
          fread(cava_in_int16, sizeof(int16_t), read_len, cava.get_in_file());
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

  cava.close_files();
  cava_destroy(plan);
  free(plan);
  free(cava_in);
  free(cava_out);
  free(frame_bars);

  return 0;
}
