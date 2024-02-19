# RAWcooked Case Study  
  
**BFI National Archive**  
**By Joanna White, Knowledge & Collections Developer**  
  
At the [BFI National Archive](https://www.bfi.org.uk/bfi-national-archive) we have been encoding DPX sequences to FFV1 Matroska since late 2019. In that time our RAWcooked workflow has evolved with the development of RAWcooked, DPX resolutions and flavours and changes in our encoding project priorities.  Today we have a fairly hands-off automated workflow which handles 2K, 4K, RGB, Luma, DPX and Tiff image sequences.  This workflow is built on some of the flags developed by the Media Area and written in a mix of BASH shell scripts and Python3 scripts and is available to view from the [BFI Data & Digital Preservation GitHub](https://github.com/bfidatadigipres/dpx_encoding). In addition to our RAWcooked use I will also consider how we use other Media Area tools alongside RAWcooked to complete necessary stages of this workflow.  Our encoding processes do not include any alpha channels or audio file processing, but RAWcooked is capable of muxing both into the completed FFV1 Matroska dependent upon your licence.
  
This case study is broken into the following sections:  
* [Server configuration](#server_config)
* [One year study of throughput](#findings)
* [Workflow: Image sequence assessment](#assessment)  
* [Workflow: Muxing the image sequence](#muxing)  
* [Workflow: Muxing log assessments](#log_assessment)  
* [Workflow: FFV1 Matroska validation](#ffv1_valid)  
* [Workflow: FFV1 Matroska demux to image sequence](#ffv1_demux)
* [Conclusion & helpful test approaches](#conclusion)  
* [Additional resources](#links)  

---  
### <a name="server_config">Server configurations</a>
  
To encode our DPX and TIFF sequences we have a single server that completes this work for all our different NAS storage paths in parallel.  
  
Our current configuration:  
- Intel(R) Xeon(R) Gold 5218 CPU @ 2.30GHz  
- 252 GB RAM  
- 32-core with 64 CPU threads  
- Ubuntu 20.04 LTS  
- 40Gbps Network card  
- NAS storage on 40GB network  
  
The more CPU threads you have the better your FFmpeg encoding to FFV1 will perform. To calculate the CPU threads for your server you can multiply the Threads x Cores x Sockets. So for our congiguration this would be 2 (threads) x 16 (sockets) x 2 (cores) = 64. To retrieve these figures we would use Linux's ```lscpu```.
  
Our previous 2K film encoding configuration:  
- Virtual Machine of a NAS storage device  
- AMD Opteron 22xx (Gen 2 Class Opteron)  
- 12GB RAM  
- 8-core @ 3 GHz (estimated)  
- 8 threads  
- Ubuntu 18.04 LTS  
  
When encoding 2K RGB we generally reach between 3 and 10 frames per second (fps) from FFmpeg encoding, 4K scans it's generally 1 fps or less. These figures can be impacted by the quantity of parellel processes running at any one time.

---  
### <a name="findings">One year study of throughput</a>
  
Between Febraury 2023 and February 2024 the BFI encoded **1020 DPX sequences** to FFV1 Matroska. A Python script was written to capture certain data about these muxed files, including sequence pixel size, colourspace, bits, and byte size of the image sequence and completed FFV1 Matroska. We captured the following data:

From 1020 total DPX sequences successfully muxed to FFV1 Matroska:  
* 140 were 2K or smaller / 880 were 4K
* 222 were Luma Y / 798 were RGB
* 143 were 10-bit / 279 12-bit / 598 16-bit
* The largest reduction in size of any FFV1 from the DPX was 88%
* The smallest reduction was just .3%
* The largest reductions were from sequences both 10/12-bit, with RGB colorspace that had black and white filters applied
* The smallest reductions were from RGB and Y-Luma 16-bit image sequences scanned full frame
* Across all 1020 muxed sequences the average size reduction was 71%  
  
A small group of sequences had their total RAWcooked muxing time recorded, revealing an average of 24 hours per sequence. The sequences all had finished MKV durations were between 5 and 10 minutes. The fastest encodings took just 7 hours with some taking 46 hours. There appears to be no cause for these variations in the files themselves and so we must assume that general network activity and/or amount of parallel processes running have influenced these variations.

---  
# Workflow
### <a name="assessment">Image sequence assessment</a>  
  
For each image sequence processed the metadata of the first DPX or TIFF is collected and saved into the sequence folder, along with total folder size in bytes and a list of all contents of the sequence. We collect this information using Media Area's MediaInfo software and capture the output into script variables.  
  
Next the first file within the image sequence is checked against a Media Area MediaConch policy for the file ([BFI's DPX policy](https://github.com/bfidatadigipres/dpx_encoding/blob/main/rawcooked_dpx_policy.xml)). If it passes then we know it can be encoded by RAWcooked and by our current licence. Any that fail are assessed for possible RAWcooked licence expansion, or possible anomalies in the DPX resulting.
  
The pixel size and colourspace of the sequence are used to calculate the potential reduction rate of the mux based on previous muxing experience. We make an assumption that 2K RGB will always be atleast one third smaller, so calculate a 1.3TB sequences to make a 1TB FFV1 Matroska.  For 2K Luma and all 4K we must assume that very small size reductions will take place. This step is necessary to control file ingest sizes to our Digital Preservation Infrastructure where we currently have a maximum verifiable ingest file size of 1TB.
  
| RAWcooked 2K RGB     | RAWcooked Luma & RAWcooked 4K |
| -------------------- | ----------------------------- |
| 1.3TB reduces to 1TB | 1.0TB may only reduce to 1TB  |
  

---
### <a name="muxing">Muxing the image sequence</a>  

To encode our image sequences we use the ```--all``` flag released in RAWcooked v21. This flag was a sponsorship development by [NYPL](https://www.nypl.org/), and sees several preservation essential flags merged into this one simple flag. Most imporantly it includes the creation of checksum hashes for every image file in the sequence, with this data being saved into the RAWcooked reversibility file embedded into the Matroska wrapper. This ensures that when demuxed the retrieved sequence can be varified as bit-identical to the original source sequence.

Our encoding command:
```
rawcooked -y --all --no-accept-gaps -s 5281680 <path/sequence_name/> -o <path/sequence_name.mkv> >> <path/sequence_name.mkv.txt> 2>&1
```
| Command                | Description                          |
| ---------------------- | ------------------------------------ |
| ```rawcooked```        | Calls the software                   |
| ```-y```               | Answers 'yes' to software questions  |
| ```-all```             | Preservation command with checksums  |
| ```--no-accept-gaps``` | Exit with warning if sequence gaps   |
| ```-s 5281680```       | Set max attachment size to 5MB       |
| ```-o```               | Use output path for FFV1 MKV         |
| ```>>```               | Capture console output to text file  |
| ```2>&1```             | stderr and stdout messages captured  |

This command is generally launched from within a Bash script, and is passed to [GNU Parallel](https://www.gnu.org/software/parallel/) to run multiple encodings in parallel. This software makes it very simple to run multiple encodings, just by writing all the image sequence paths to one text file you can launch a parallel command like this:
```
cat ${sequence_list.txt} | parallel --jobs 10 "rawcooked -y --all --no-accept-gaps -s 5281680 {} -o {}.mkv >> {}.mkv.txt 2>&1"
```

We always capture our console logs for every encoding. The ```2>&1``` ensures any error messages are output alongside the usual standard messages. These are essential for us to review if a problem is found with an encoding. Over time they also provide a very clear record of changes encountered in FFmpeg and RAWcooked software, and data of our own image sequence files. In recent years these logs have been critical in identifying unanticipated edge cases with some DPX encodings, allowing for impact assessment generation. We definitely encourage all RAWcooked users to capture and retain this information as part of their long-term preservation metadata collection.
  
  
### <a name="log_assessment">Muxing log assessment</a>

### <a name="ffv1_valid">FFV1 Matroska validation</a>

### <a name="ffv1_demux">FFV1 Matroska demux to image sequence</a>

# Conclusion
### <a name="conclusion">Conclusion & some helpful test approaches</a>
  
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


## <a name="links">Additional resources</a>  

* ['No Time To Wait! 5' presentation about the BFI's evolving RAWcooked use](https://www.youtube.com/@MediaAreaNet/streams). Link to follow.  
* [RAWcooked cheat sheet](https://github.com/bfidatadigipres/dpx_encoding/blob/main/RAWcooked_Cheat_Sheet.pdf)  
