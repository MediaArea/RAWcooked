# RAWcooked User Manual

## Encode

### Folder

```
rawcooked <folder>
```

The `RAWcooked` tool

- encodes with the FFV1 video codec all single-image files in the folder
- encodes with the FLAC audio codec all audio files in the folder
- muxes these into a Matroska container (.mkv)

The filenames of the single-image files must end with a numbered sequence. `RAWcooked` will generate the regular expression (regex) to parse in the correct order all the frames in the folder. 

The number sequence within the filename can be formed with leading zero padding - eg 000001.dpx, 000002.dpx... 500000.dpx - or no leading zero padding - eg 1.dpx, 2.dpx... 500000.dpx.

`RAWcooked` has no strict expectations of a complete, continuous number sequence, so breaks in the sequence  - eg 47.dpx, 48.dpx, 65.dpx, 66.dpx - will cause no error or failure in `RAWcooked`.

`RAWcooked` has expectations about the folder and subfolder structures containing the image files, and the Matroska that is created will manage subfolders in this way: 

- a single folder of image files, or a folder with a single subfolder of image files, will result in a Matroska with one video track
- a folder with multiple subfolders of image files, will result in a Matroska with multiple video tracks, one track per subfolder

This behaviour could help to manage different use cases, according to local preference. For example: 
- multiple reels in a single Matroska, one track per reel
- multiple film scan attempts (rescanning to address a technical issue in the first scan) in a single Matroska, one track per scan attempt
- multiple overscan approaches (eg no perfs, full perfs) in a single Matroska, one track per overscan approach

Note that maximum permitted video tracks is encoded in the `RAWcooked` licence, so users may have to request extended track allowance as required.

### File

```
rawcooked <file>
```

The file contains RAW data (e.g. it is a .dpx or .wav file). The `RAWcooked` tool

- encodes with the FFV1 video codec all single-image video files in the folder containing the given file
- encodes with the FLAC audio codec all audio files in the folder containing the given file
- muxes these into a Matroska container (.mkv).

The filenames usually end with a numbered sequence. Enter one frame and the tool will generate the regex to parse all the frames in the folder.

## Decode

```
rawcooked <file>
```

The file is a Matroska container (.mkv). The `RAWcooked` tool decodes back the video and the audio of file to its original formats.  All metadata accompanying the original data are preserved **bit-by-bit**.
