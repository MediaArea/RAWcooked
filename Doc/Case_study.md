# RAWcooked Case Study  
  
## BFI National Archive  
### Joanna White, Knowledge & Collections Developer
  
At the [BFI National Archive](https://www.bfi.org.uk/bfi-national-archive) we have been encoding DPX sequences to FFV1 Matroska since late 2019. In that time our RAWcooked workflow has evolved with DPX resolutions and flavours and changes in project priorities - Ev.  Today we have a fairly hands-off automated workflow which handles 2K, 4K, RGB, Luma, DPX and Tiff DPX.  This workflow is built on some of the flags developed by the Media Area. This includes the '--all' flag, originally an idea suggested by [NYPL](https://www.nypl.org/), more details in the [Encoding]() section below.  
  
This case study is broken into the stages our encoding workflow follows and the Media Area tools we use to achieve our goals.  We have four distinct stages:  
* Server configuration  
* Image sequence assessment  
* Encoding the image sequence  
* Encoding log assessments  
* FFV1 MKV validation  
  
### Server configuration
To encode our DPX and TIFF sequences we have a single server that completes this work for all our different NAS storage paths.

Intel Xeon Gold 5218 @ 2.30GHz  
It's 32-core with 64 threads so can manage a LOT of processes concurrently.  We have read/write to NAS storage devices that are pretty fast across a 100GB network.  

Previously we were running speedy RAWcooking (only RAWcooking) on a Virtual Machine of a NAS storage device and we read from/wrote to it at times:  
AMD Opteron 22xx (Gen 2 Class Opteron)  
8-core @ 3 GHz (estimated)  
8 threads  

When encoding 2K RGB we generally reach between 4 and 5 FPS from FFmpeg encoding on the lower machine. Now running 4K scans it's often 1 FPS or less on the top machine.  
  
## Image sequence assessment  
  





## Encoding the image sequence  

```
rawcooked -y --all --no-accept-gaps -s 5281680 <path/sequence_name/> -o <path/sequence_name.mkv> &>> <path/sequence_name.mkv.txt>
```
