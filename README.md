# RAWcooked

RAWcooked encodes RAW audio-visual data into the Matroska container (MKV), using the video codec FFV1 for the image and audio codec FLAC for the sound.

## Status

RAWcooked is ready for production. As with any other tool, keep doing basic checks e.g. testing reversibility with your own files.

## Website

Information on this project and builds for different platforms can be found at <https://mediaarea.net/RAWcooked>.

## FFmpeg

RAWcooked needs FFmpeg for encoding. The FFmpeg libraries are also used for decoding with the GPU (Vulkan), which is faster.  
We recommend the FFmpeg builds of MediaArea, they have GPU support and RAWcooked finds them automatically.
All the builds are in <https://mediaarea.net/download/snapshots/binary/ffmpeg/latest/>.

Use `--show-stats` for seeing if the GPU is used.

### Windows

Download <https://mediaarea.net/download/snapshots/binary/ffmpeg/latest/FFmpeg_Bin_Latest_Windows_Shared_x64.zip>.

Put rawcooked.exe in a directory "YourDir", create a subdirectory "YourDir\ffmpeg" and put the content of FFmpeg_Bin_Latest_Windows_Shared_x64.zip in it.  
ffmpeg.exe should be in "YourDir\ffmpeg\bin":

```
YourDir\rawcooked.exe
YourDir\ffmpeg\bin\ffmpeg.exe
YourDir\ffmpeg\bin\avcodec-63.dll
YourDir\ffmpeg\bin\avutil-61.dll
...
```

Use the Shared package, the Static package has only ffmpeg.exe, without the libraries used by RAWcooked for decoding with the GPU.  
The GPU support uses the Vulkan driver installed with the driver of the graphics card (NVIDIA, AMD or Intel).

### macOS

Download <https://mediaarea.net/download/snapshots/binary/ffmpeg/latest/FFmpeg_Bin_Latest_Mac_Shared_universal.zip>.

Put rawcooked in a directory "YourDir", create a subdirectory "YourDir/ffmpeg" and put the content of FFmpeg_Bin_Latest_Mac_Shared_universal.zip in it.  
ffmpeg should be in "YourDir/ffmpeg/bin":

```
YourDir/rawcooked
YourDir/ffmpeg/bin/ffmpeg
YourDir/ffmpeg/lib/libavcodec.63.dylib
YourDir/ffmpeg/lib/libavutil.61.dylib
...
```

Alternatively, download <https://mediaarea.net/download/snapshots/binary/ffmpeg/latest/ffmpeg-ma_Latest_Mac.dmg>, open it and run the installer in it.
FFmpeg is installed as ffmpeg-ma in /usr/local and RAWcooked finds it, wherever rawcooked is.

The GPU support needs a Mac with Apple silicon and macOS 26 or later, the Vulkan driver (KosmicKrisp) is included in both packages.

### Linux

Download the ffmpeg-ma package of your distribution and of your CPU (amd64 or x86_64 for Intel and AMD CPUs, arm64 or aarch64 for ARM CPUs) from <https://mediaarea.net/download/snapshots/binary/ffmpeg/latest/> and install it with the package manager of your distribution, e.g.:

```sh
# Debian, Ubuntu, Raspberry Pi OS
sudo apt install ./ffmpeg-ma_Latest-1_amd64.Ubuntu_24.04.deb
# Fedora, AlmaLinux
sudo dnf install --nogpgcheck ./ffmpeg-ma-Latest.x86_64.Fedora_44.rpm
# openSUSE, SUSE Linux Enterprise
sudo zypper install --allow-unsigned-rpm ./ffmpeg-ma-Latest.x86_64.openSUSE_Tumbleweed.rpm
# Mageia
sudo urpmi --no-verify-rpm ./ffmpeg-ma-Latest.x86_64.Mageia_10.rpm
# Arch Linux
sudo pacman -U ./ffmpeg-ma-Latest-x86_64.Arch_Extra.pkg.tar.zst
```

FFmpeg is installed as ffmpeg-ma, it does not replace the FFmpeg of your distribution, and RAWcooked finds it, wherever rawcooked is.

The GPU support uses the Vulkan driver of the graphics card: the driver of NVIDIA, or the Vulkan drivers of Mesa for AMD and Intel (e.g. package mesa-vulkan-drivers).

### Other FFmpeg builds

Any FFmpeg with the FFV1 and FLAC encoders may be used for encoding (e.g. the FFmpeg of your distribution or of Homebrew), if ffmpeg is in PATH.  
The FFmpeg libraries are used by RAWcooked only if they are from FFmpeg 9 (libavcodec 63 and libavutil 61) and, for the GPU support, built with Vulkan.  
The `-b` option selects another ffmpeg binary or libavcodec library, see `rawcooked --help`.

## License

[BSD 2-Clause License](LICENSE).  
Some [alternate open source licenses](License.html) are also available.
