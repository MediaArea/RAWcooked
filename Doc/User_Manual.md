# RAWcooked User Manual

## Encoding

```
rawcooked --all <folder> / <file>
```
  
When using the `RAWcooked` tool:  
- encodes an image sequence by supplying the folder path to the sequence, or by supplying the path to a media file within your sequence folder
- encodes with the FFV1 video codec all single-image files or video files in the folder path/folder containing the file
- encodes with the FLAC audio codec all audio files in the folder  
- muxes these into a Matroska container (.mkv)

To encode your sequences using the best preservation flags within RAWcooked then you can use the ```--all``` flag which concatenates several important flags into one:  
  
| Commands with --all       | Description                                |
| ------------------------- | ------------------------------------------ |
| ```--info```              | Supplies useful file information           |
| ```--conch```             | Conformance checks file format where       |
|                           | supported (partially implemented for DPX)  |
| ```--encode / --decode``` | Selected based on supplied file type       |
| ```--hash```              | Important flag which computes hashes and   |
|                           | embeds them in reversibility data stored   |
|                           | in MKV wrapper allowing reversibility test |
|                           | assurance when original sequences absent.  |
| ```--coherency```         | Ensures package and content are coherent   |
|                           | eg, sequence gap checks and audio duration |
|                           | matches image sequence duration            |
| ```--check```             | Checks that an encoded file can be decoded |
|                           | correctly. Requires hashes to be present   |
|                           | for checking compressed content.           |
| ```--check_padding```     | Runs padding checks for DPX files that do  |
|                           | not have zero padding. Ensures additional  |
|                           | padding data is stored in reversibility    |
|                           | file for perfect restoration of the DPX.   |
|                           | Can be time consuming.                     |
| ```--accept-gaps```       | Where gaps in a sequence are found this    |
|                           | flag ensures the encoding completes        |
|                           | successfully. If you require that gaps     |
|                           | are not encoded then follow the ```--all```|
|                           | command with ```--no-accept-gaps```        |
  
If you do not require all of these flags you can build your own command with just the flags you prefer, for exmaple:
```
rawcooked --info --conch --encode --hash --check --no-accept-gap <folder> / <file>
```

For more information about all the available flags in RAWcooked please visit the help page:
```
rawcooked --help / -h
```


The filenames of the single-image files must end with a numbered sequence. `RAWcooked` will generate the regular expression (regex) to parse in the correct order all the frames in the folder. 

The number sequence within the filename can be formed with leading zero padding - e.g. 000001.dpx, 000002.dpx... 500000.dpx - or no leading zero padding - e.g. 1.dpx, 2.dpx... 500000.dpx.

`RAWcooked` has no strict expectations of a complete, continuous number sequence, so breaks in the sequence  - e.g. 47.dpx, 48.dpx, 65.dpx, 66.dpx - will cause no error or failure in `RAWcooked`.

`RAWcooked` has expectations about the folder and subfolder structures containing the image files, and the Matroska that is created will manage subfolders in this way: 

- a single folder of image files, or a folder with a single subfolder of image files, will result in a Matroska with one video track
- a folder with multiple subfolders of image files, will result in a Matroska with multiple video tracks, one track per subfolder

This behaviour could help to manage different use cases, according to local preference. For example: 
- multiple reels in a single Matroska, one track per reel
- multiple film scan attempts (rescanning to address a technical issue in the first scan) in a single Matroska, one track per scan attempt
- multiple overscan approaches (e.g. no perfs, full perfs) in a single Matroska, one track per overscan approach

Note that maximum permitted video tracks is encoded in the `RAWcooked` licence, so users may have to request extended track allowance as required.




## Decode

```
rawcooked <file>
```

The file is a Matroska container (.mkv). The `RAWcooked` tool decodes back the video and the audio of file to its original formats.  All metadata accompanying the original data are preserved **bit-by-bit**.
