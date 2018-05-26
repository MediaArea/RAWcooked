# RAWcooked User Manual

## Encode Files

### Folder

```sh
rawcooked folder
```

The `RAWcooked` tool

- encodes with the FFV1 video codec all single-image files in the folder
- encodes with the FLAC audio codec all audio files in the folder
- muxes these into a Matroska container (.mkv)

The filenames of the single-image files must end with a numbered sequence. `RAWcooked` will generate the regular expression (regex) to parse in the correct order all the frames in the folder.

### File

```sh
rawcooked file
```

The file contains RAW data (e.g. it is a .dpx or .wav file). The `RAWcooked` tool

- encodes with the FFV1 video codec all single-image video files in the folder containing the given file
- encodes with the FLAC audio codec all audio files in the folder containing the given file
- muxes these into a Matroska container (.mkv).

The filenames usually end with a numbered sequence. Enter one frame and the tool will generate the regex to parse all the frames in the folder.

## Decode Files

The file is a Matroska container (.mkv). The `RAWcooked` tool decodes back the video and the audio of file to its original formats.  All metadata accompanying the original data are preserved **bit-by-bit**.
