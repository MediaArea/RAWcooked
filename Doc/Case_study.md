# RAWcooked Case Study  
  
**BFI National Archive**  
**By Joanna White, Knowledge & Collections Developer**  
  
At the [BFI National Archive](https://www.bfi.org.uk/bfi-national-archive) we have been encoding DPX sequences to FFV1 Matroska since late 2019. In that time our RAWcooked workflow has evolved with the development of RAWcooked, DPX resolutions and flavours and changes in our encoding project priorities.  Today we have a fairly hands-off automated workflow which handles 2K, 4K, RGB, Luma, DPX and Tiff image sequences.  This workflow is built on some of the flags developed by the Media Area and written in a mix of BASH shell scripts and Python3 scripts and is available to view from the [BFI Data & Digital Preservation GitHub](https://github.com/bfidatadigipres/dpx_encoding). In addition to our RAWcooked use I will also consider how we use other Media Area tools alongside RAWcooked to complete necessary stages of this workflow.
  
This case study is broken into the following sections:  
* Server configuration  
* Image sequence assessment  
* Encoding the image sequence  
* Encoding log assessments  
* FFV1 Matroska validation  
* FFV1 Matroska demux to image sequence
* Conclusion & helpful test approaches  
* Additional resources  
  
### Server configurations
  
To encode our DPX and TIFF sequences we have a single server that completes this work for all our different NAS storage paths in parallel.  
  
Our current configuration:  
Intel(R) Xeon(R) Gold 5218 CPU @ 2.30GHz  
252 GB RAM  
32-core with 64 CPU threads  
Ubuntu 20.04 LTS  
40Gbps Network card  
NAS storage on 10GB network  
  
Our previous 2K film encoding configuration:  
Virtual Machine of a NAS storage device  
AMD Opteron 22xx (Gen 2 Class Opteron)  
12GB RAM  
8-core @ 3 GHz (estimated)  
8 threads  
Ubuntu 18.04 LTS  
  
When encoding 2K RGB we generally reach between 3 and 10 frames per second (fps) from FFmpeg encoding on the lower machine. Now running 4K scans it's generally 1 fps or less. 
  
## Image sequence assessment  
  




## Encoding the image sequence  

This includes the '--all' flag, originally an idea suggested by [NYPL](https://www.nypl.org/), more details in the [Encoding]() section below.  
```
rawcooked -y --all --no-accept-gaps -s 5281680 <path/sequence_name/> -o <path/sequence_name.mkv> &>> <path/sequence_name.mkv.txt>
```

## Encoding log assessment

## FFV1 Matroska validation

## FFV1 Matroska demux to image sequence

## Conclusion & some helpful test approaches
  
The workflow covers most of the the areas we think are essential for safe automated encoding of the DPX sequences.  There is a need for manual intervention when repeated errors are encountered and an image sequences never makes it to our Digital Preservation Infrastructure.  Most often this indicates a different image sequence flavour we do not have covered in our licence, but sometimes it can indicate a larger issue with either RAWcooked of FFmpeg encoding. Where errors are found these are reported to an error log named after the image seqeuence, for easier monitoring.  

When any upgrades occur we like to run some select reversibility test to ensure RAWcooked is still operating as we would expect. This can be for RAWcooked software updates, FFmpeg updates, but also for updates to our operating system. To perform a reversibility test, a cross-section of image sequences are muxed using our usual ```--all``` command, and then demuxed again fully. The image sequences of both the original and demuxed version then have whole file MD5 checksums generating and saving to a manifest. These manifests are then ```diff``` checked to ensure that every single image file is identical.
  
When we encounter an error there are a few commands I use that make reporting the issue a little easier at the Media Area RAWcooked GitHub issue tracker.  
```
rawcooked -d -y -all --accept-gaps <path/sequence_name>  
```
The -d flag returns the command sent to FFmpeg instead of launching the command. This flag also leaves the reversibility data available to view as a text file and this is useful for finding errors.  
```
head -c 1048576 sequence_name.mkv > dump_file.mkv  
```
This command uses UNIX ```head``` software to cut the first 120KB of data from a supplied file, copying it to a new file which is easier to forward to Media Area for review.  This contains the file's header data, often requested when a problem has occurred.  
```
echo $?
```
This command should be run directly after a failed RAWcooked encoding, and it will tell you the exit code returned from that terminated run.  
  
The results of these three enquiries is always a brilliant way to open an Issue enquiry for Media Area and will help ensure swift diagnose for your problem. It may also be necessary to supply a DPX sequence, and your ```head``` command can be used again to extract the header data.


## Additional resources  

* ['No Time To Wait! 5' presentation about the BFI's evolving RAWcooked use](https://www.youtube.com/@MediaAreaNet/streams). Link to follow.  
* [RAWcooked cheat sheet](https://github.com/bfidatadigipres/dpx_encoding/blob/main/RAWcooked_Cheat_Sheet.pdf)  
