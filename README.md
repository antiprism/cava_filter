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

Convert raw pcm_s16le stereo 44100 format to frequency spectrum data using
the cavacore library https://github.com/karlstav/cava. If input_file is not
given the program reads from standard input.

  Options
  -h,--help this help message
  --version version information

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
  -o <file>  write output to file (default: write to standard output)
```
