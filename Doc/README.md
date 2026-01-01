# RAWcooked Documentation

RAWcooked encodes so-called raw audio-visual data, like DPX or TIFF and WAVE or BWF, into the Matroska container (.mkv), using the video codec FFV1 for the image and audio codec FLAC for the sound. The metadata accompanying the raw data are preserved, and sidecar files, like MD5, LUT or XML, can be added into the Matroska container as attachments. This allows to manage these audio-visual file formats in an effective and transparent way (e.g. native playback in VLC), while saving typically between one and two thirds of the needed storage, and speeding up file writing and reading (e.g. to/from harddisk, over network or for backup on LTO).

When needed, the uncompressed source is retrieved **bit-by-bit**, in a manner faster than uncompressed sources directly stored on LTO cartridges.

---

## Table of Contents

- [RAWcooked User Manual](User_Manual.md)
- [BFI National Archive RAWcooked Case Study](Case_study.md)
- [Structure of RAWcooked reversibility data](File_Structure.md)
- [RAWcooked Compilation Instructions](Compilation.md)
