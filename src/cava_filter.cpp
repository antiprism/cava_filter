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

extern "C" {
#include "cavacore/cavacore.h"
}

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

class CavaFilter : public ProgramOpts {
private:
  const size_t input_len_per_channel = 4096;

  // input audio format must be pcm_s16le, convert with, e.g.
  // ffmpeg -i file.wav -f s16le -ar 44100 -acodec pcm_s16le -ac 2
  int channels = 2; // input audio number of channels (stereo)
  int rate = 44100; // input audio sample rate

  int bars_per_channel = 10;
  int channels_out = 1;
  double framerate = 25;
  int autosens = 0;
  double noise_reduction = 0.1; // 0.0: noisy 1.0: smooth
  int print_freq_bands = false;
  std::vector<int> cutoffs = {50, 10000}; // cava low_cutoff and high_cutoff
  FILE *in_file = stdin;
  FILE *out_file = stdout;

  void print_freq_bands_line(float *freqs) const;
  void print_freq_vals_line(const std::vector<double> &frame_bars) const;

public:
  CavaFilter() : ProgramOpts("cava_filter") {}
  ~CavaFilter();
  void process_command_line(int argc, char **argv);
  void usage();
  Status generate_spectrum_file();
};

CavaFilter::~CavaFilter()
{
  if (in_file != stdin) {
    fclose(in_file);
    in_file = stdin;
  }
  if (out_file != stdout) {
    fclose(out_file);
    out_file = stdin;
  }
}

void CavaFilter::print_freq_bands_line(float *freqs) const
{
  if (print_freq_bands) {
    for (int ch = 0; ch < channels_out; ch++)
      for (int i = 0; i < bars_per_channel; i++)
        fprintf(out_file, "%4d ", (int)freqs[i]);
    fprintf(out_file, "\n");
  }
}

void CavaFilter::print_freq_vals_line(
    const std::vector<double> &frame_bars) const
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

namespace {
bool is_odd(int num) { return num % 2; }
};


Status CavaFilter::generate_spectrum_file()
{
  Status stat;
  auto *plan = cava_init(bars_per_channel, rate, channels, autosens,
                         noise_reduction, cutoffs[0], cutoffs[1]);

  if (print_freq_bands)
    print_freq_bands_line(plan->cut_off_frequency);

  size_t input_len = input_len_per_channel * channels; // samples buffier len
  int bars_total = bars_per_channel * channels; // total bar vals in cava_out

  std::vector<int16_t> cava_in_int16(input_len); // raw sample buffer
  std::vector<double> cava_in(input_len);        // double sample buffer
  std::vector<double> cava_out(bars_total);      // cava exec bar values
  std::vector<double> frame_bars(bars_total);    // frame bar values

  const double samples_per_frame = (double)(rate * channels) / framerate;
  // + channels sample to ensure being able to hold fractional part of sample
  int execs_per_frame = ceil((samples_per_frame + channels) / input_len);
  int samples_per_exec = samples_per_frame / execs_per_frame;
  int samples_remainder =
      samples_per_frame - execs_per_frame * samples_per_exec;

  // Find the fractional part of the sample that would be lost each frame
  double intpart;
  double sample_fraction_per_frame = modf(samples_per_frame, &intpart);

  double current_accumulated_sample_fractions = 0.0;
  int finished = 0; // has all the data been read (or error)
  while (!finished) {
    for (int bar_idx = 0; bar_idx < bars_total; bar_idx++)
      frame_bars[bar_idx] = 0;

    for (int read_idx = 0; read_idx < execs_per_frame; read_idx++) {
      // Add 1 to each of the first execs to include the samples remainder
      size_t read_len = samples_per_exec + (read_idx < samples_remainder);

      // ensure that buffer is filled with even number of samples
      if (channels == 2 && is_odd(read_len)) {
        // adjust by one sample, add or subtract on alternate iterations
        const int offset = is_odd(read_idx) ? -1 : 1;
        read_len += offset;
        current_accumulated_sample_fractions -= offset; // balance accounts
      }

      // Add an extra samples if needed to the last exec. There should always
      // be room for this from the calculation of execs_per_frame
      if (read_idx == execs_per_frame - 1) {
        current_accumulated_sample_fractions += sample_fraction_per_frame;
        if (current_accumulated_sample_fractions >= channels) {
          read_len += channels;
          current_accumulated_sample_fractions -= channels;
        }
      }

      size_t actual_num_read =
          fread(cava_in_int16.data(), sizeof(int16_t), read_len, in_file);
      if (actual_num_read < read_len) { // end of stream or error
        finished = 1;
        if (ferror(in_file))
          stat.set_error(std::string("reading input: ") + strerror(errno));
        break;
      }

      // convert samples to doubles for cava
      for (size_t i = 0; i < read_len; i++)
        cava_in[i] = (int)cava_in_int16[i];

      cava_execute(cava_in.data(), read_len, cava_out.data(), plan);

      // add weighted bar values
      for (int bar_idx = 0; bar_idx < bars_total; bar_idx++)
        frame_bars[bar_idx] += cava_out[bar_idx] / execs_per_frame;
    }

    if (finished) // end of stream or error
      break;

    print_freq_vals_line(frame_bars);
  }

  cava_destroy(plan);
  free(plan);

  return stat;
}

void CavaFilter::usage()
{
  fprintf(stdout, R"(
Usage: %s [options] [input_file]

Convert raw pcm_s16le format to frequency spectrum data using the cavacore
library https://github.com/karlstav/cava. If input_file is not given the
program reads from standard input.

  Options
%s
  -b <num>   number of bars to print (default: 10)
  -f <hz>    framerate in Hz (default: 25) Note, the final frame will usually
             be a partial frame and no bars value line will be printed for it
  -S         stereo output, print the right channel bars followed on the line
             by the left channel bars
  -n <fact>  noise reduction, a number between 0.0 noisy, and 1.0 smooth
             (default: 0.1)
  -a <auto>  value for the cava autosens setting (default: 0 no autosens)
  -c <frqs>  low and high cutoff frequencies for cava, two integers
             separated by a comma (default: 50,10000)
  -F         the first line printed is the frequencies of the bands
  -R <hz>    input audio sample rate (default: 44100)
  -C <cnls>  input audio channels 1-mono, 2-stereo (default: 2)
  -o <file>  write output to file (default: write to standard output)

  )",
          get_program_name().c_str(), help_ver_text);
}

void CavaFilter::process_command_line(int argc, char **argv)
{
  opterr = 0;
  int c;
  std::string file_name;

  handle_long_opts(argc, argv);

  while ((c = getopt(argc, argv, ":ho:b:f:Sn:a:c:FR:C:")) != -1) {
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
      if (framerate <= 0)
        error("framerate must be greater than 0", c);
      break;

    case 'S':
      channels_out = 2;
      break;

    case 'n':
      print_status_or_exit(read_double(optarg, &noise_reduction), c);
      if (noise_reduction < 0.0 || noise_reduction > 1.0)
        error("noise reduction must be between 0.0 and 1.0", c);
      break;

    case 'a':
      print_status_or_exit(read_int(optarg, &autosens), c);
      if (autosens < 0)
        error("autosens cannot be negative (0 to disable)", c);
      break;

    case 'c':
      // read two positive integers
      print_status_or_exit(read_int_list(optarg, cutoffs, false, 2), c);
      if (cutoffs.size() < 2)
        error("must specify two cutoffs (low and high)", c);
      if (cutoffs[0] < 1)
        error("first cutoff (low) must be greater than 0", c);
      if (cutoffs[1] <= cutoffs[0])
        error("second cutoff (high) must be greater than first cutoff (low)",
              c);
      break;

    case 'F':
      print_freq_bands = true;
      break;

    case 'R':
      print_status_or_exit(read_int(optarg, &rate), c);
      if (rate < 1)
        error("rate must be positive", c);
      break;

    case 'C':
      if(strcmp(optarg, "1") == 0)
        channels = 1;
      else if(strcmp(optarg, "2") == 0)
        channels = 2;
      else
        error("invalid number of channels, should be 1 or 2", c);
      break;

    case 'o':
      file_name = optarg;
      if (file_name == "-") {
        out_file = stdout;
      }
      else {
        out_file = fopen(file_name.c_str(), "w");
        if (!out_file)
          error("could not open file for writing '" + file_name +
                "': " + strerror(errno));
      }
      break;

    default:
      error("unknown command line error");
    }
  }

  if (argc - optind > 1)
    error("too many arguments");

  file_name = (argc - optind == 0) ? "-" : argv[optind];
  if (file_name == "-")
    in_file = stdin;
  else {
    in_file = fopen(file_name.c_str(), "r");
    if (!in_file)
      error("could not open file for reading '" + file_name +
            "': " + strerror(errno));
  }
}

int main(int argc, char *argv[])
{
  CavaFilter cava;
  cava.process_command_line(argc, argv);
  cava.print_status_or_exit(cava.generate_spectrum_file());

  return 0;
}
