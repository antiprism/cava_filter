# cava_filter

`cava_filter` uses the new [Cava](https://github.com/karlstav/cava)
cavacore library to process an audio file and print out the frequency
spectrum data.

## Build instructions

```
./bootstrap
./configure
make
sudo make install-strip
```

## Usage

`cava_filter` converts raw pcm_s16le format to frequency spectrum data.

Convert file.wav to file.raw and process by cava_filter with the following
command
```
ffmpeg -i file.wav -f s16le -ar 44100 -acodec pcm_s16le -ac 2 file.raw
cava_filter file.raw > file_freq_spectrum.txt
```
or
```
ffmpeg -i file.wav -f s16le -ar 44100 -acodec pcm_s16le -ac 2 - | cava_filter > file_freq_spectrum.txt
```

To see all options run `cava_filter -h`
```
Usage: cava_filter [options] [input_file]

convert raw pcm_s16le format to frequency spectrum data using the cavacore
library https://github.com/karlstav/cava. If input_file is not given the
program reads from standard input.

  Options
    -h,--help this help message
  --version version information

  -b <num>   number of bars to print (default: 10)
  -f <hz>    framerate in Hz (default: 25)
  -S         stereo output, print the right channel bars followed on the line
             by the left channel bars
  -s <fact>  smooth factor, a number greater than 0.1 (not smooth) to set
             spectrum smoothness (default: 1)
  -a <auto>  value for the cava autosens setting (default: 0 no autosens)
  -F         the first line printed is the frequencies of the bands
  -o <file>  write output to file (default: write to standard output)

```
