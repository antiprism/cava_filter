# cava_filter

*cava_filter* uses the new [Cava](https://github.com/karlstav/cava)
cavacore library to produce produce frequency spectrum data for an audio file.

## Build instructions

```
./bootstrap
./configure
make
sudo make install-strip
```

## Usage

*cava_filter* converts raw pcm_s16le format to frequency spectrum data. To
see all options run
```
cava_filter -h
```
Convert file.wav to file.raw and process by cava_filter with the following
command
```
ffmpeg -i file.wav -f s16le -ar 44100 -acodec pcm_s16le -ac 2 file.raw
cava_filter < file.raw > file_freq_spectrum.txt
```
or
```
ffmpeg -i file.wav -f s16le -ar 44100 -acodec pcm_s16le -ac 2 | cava_filter > file_freq_spectrum.txt
```

