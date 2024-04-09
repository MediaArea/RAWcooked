# BFI National Archive RAWcooked Case Study  
**by Joanna White, Knowledge & Collections Developer**  
  
At the [BFI National Archive](https://www.bfi.org.uk/bfi-national-archive) we have been encoding DPX sequences to FFV1 Matroska since late 2019. In that time our RAWcooked workflow has evolved with the development of RAWcooked, DPX resolutions and flavours and changes in our encoding project priorities.  Today we have a fairly hands-off automated workflow which handles 2K and 4K image sequences.  This workflow is built on some of the flags developed in RAWcooked by Media Area and written in a mix of Bash shell and Python3 scripts ([BFI Data & Digital Preservation GitHub](https://github.com/bfidatadigipres/dpx_encoding)). In addition to RAWcooked we use other Media Area tools to complete necessary stages of this workflow.  Our encoding processes do not include any alpha channel or audio file processing, but RAWcooked is capable of encoding both into the completed FFV1 Matroska.
  
This case study is broken into the following sections:  
* [Server configurations](#server_config)
* [Workflow: Image sequence assessment](#assessment)  
* [Workflow: Encoding the image sequence](#muxing)  
* [Workflow: Encoding log assessments](#log_assessment)  
* [Workflow: FFV1 Matroska validation](#ffv1_valid)  
* [Workflow: FFV1 Matroska decode to image sequence](#ffv1_demux)
* [Conclusion](#conclusion)
* [Useful test approaches](#tests)
* [Additional resources](#links)  

---  
### <a name="server_config">Server configurations</a>
  
To encode our DPX sequences we have a single server that completes this work against 6 different Network Attached Storage (NAS) devices in parallel.  
  
Our current server configuration:  
- Intel(R) Xeon(R) Gold 5218 CPU @ 2.30GHz  
- 252GB RAM  
- 32-core with 64 CPU threads  
- Ubuntu 20.04 LTS  
- 40Gbps Network card  
- NAS storage with 40Gbps network card  
  
The more CPU threads you have the better your FFmpeg encode to FFV1 will perform. To calculate the CPU threads for your server you can multiply the Threads x Cores x Sockets. So for our configuration this would be 2 (threads) x 16 (sockets) x 2 (cores) = 64. To retrieve these figures we would use Linux's ```lscpu```.
  
Our previous server configuration:  
- Virtual Machine of a NAS storage device  
- AMD Opteron 22xx (Gen 2 Class Opteron)  
- 12GB RAM  
- 8-core @ 3 GHz (estimated)  
- 8 threads  
- Ubuntu 18.04 LTS  
  
When encoding 2K RGB we generally reach between 3 and 10 frames per second (fps), but 4K scans are generally 1 fps or less. These figures can be impacted by the quantity of parallel processes running at any one time.

---  
## Workflow
### <a name="assessment">Image sequence assessment</a>  
  
For each image sequence processed the metadata of the first DPX is collected and saved into the sequence folder, along with total folder size in bytes and a list of all contents of the sequence. We collect this information using [Media Area's MediaInfo software](https://mediaarea.net/MediaInfo) and capture the output into script variables.  
  
Next the first file within the image sequence is checked against a [Media Area's MediaConch software](https://mediaarea.net/MediaConch) policy for the file ([BFI's DPX policy](https://github.com/bfidatadigipres/dpx_encoding/blob/main/rawcooked_dpx_policy.xml)). If it passes then we know it can be encoded by RAWcooked and by our current licence. Any that fail are assessed for possible RAWcooked licence expansion or possible anomalies in the DPX.
  
The frame pixel size and colourspace of the sequence are used to calculate the potential reduction rate of the RAWcooked encode based on previous reduction experience. We make an assumption that 2K RGB will always be atleast one third smaller, so calculate a 1.3TB sequences to make a 1TB FFV1 Matroska.  For 2K Luma and all 4K we must assume that very small size reductions could occur so map 1TB to 1TB. This step is necessary to control file ingest sizes to our Digital Preservation Infrastructure where we currently have a maximum verifiable ingest file size of 1TB. Where a sequence is over 1TB we have Python scripts to split that DPX sequence across additional folders depending on total size.
  
| RAWcooked 2K RGB     | RAWcooked Luma & RAWcooked 4K |
| -------------------- | ----------------------------- |
| 1.3TB reduces to 1TB | 1.0TB may only reduce to 1TB  |
  

### <a name="muxing">Encoding the image sequence</a>  

To encode our image sequences we use the ```--all``` flag released in RAWcooked v21. This flag was a sponsorship development by [NYPL](https://www.nypl.org/), and sees several preservation essential flags merged into this one simple flag. Most imporantly it includes the creation of checksum hashes for every image file in the sequence, with this data being saved into the RAWcooked reversibility file and embedded into the Matroska wrapper. This ensures that when decoded the retrieved sequence can be verified as bit-identical to the original source sequence.

Our RAWcooked encode command:
```
rawcooked -y --all --no-accept-gaps -s 5281680 path/sequence_name/ -o path/sequence_name.mkv >> path/sequence_name.mkv.txt 2>&1
```
  
| Command                | Description                                |
| ---------------------- | ------------------------------------------ |
| ```rawcooked```        | Calls the software                         |
| ```-y```               | Answers 'yes' to software questions        |
| ```-all```             | Preservation command with CRC-32 hashes    |
| ```--no-accept-gaps``` | Exit with warning if sequence gaps found   |
|                        | --all command defaults to accepting gaps   |
| ```-s 5281680```       | Set max attachment size to 5MB             |
| ```-o```               | Use output path for FFV1 MKV               |
| ```>>```               | Capture console output to text file        |
| ```2>&1```             | stderr and stdout messages captured in log |

This command is generally launched from within a Bash script, and is passed to [GNU Parallel](https://www.gnu.org/software/parallel/) to run multiple encodes at the same time. This software makes it very simple to fix a specific number of encodes specified by the ```--jobs``` flag. Parallelisation is the act of processing jobs in parallel, dividing up the work to save time. If not run in parallel a computer will usually process jobs serially, one after another. As well as parallelisation, FFmpeg usinges multi-threading to create the FFV1 file. The FFV1 codec has slices through each frame (64 slice minimum in RAWcooked frame) which allows for granular checksum verification, but also allows FFmpeg multi-threading. Each slice block is split into different processing tasks and run across your CPU threads, so for our server that works as 64 separate tasks per thread, one slice per frame of the FFV1 file.

By listing all the image sequence paths in one text file you can launch a parallel command like this to run 5 parallel encodes:
```
cat ${sequence_list.txt} | parallel --jobs 5 "rawcooked -y --all --no-accept-gaps -s 5281680 {} -o {}.mkv >> {}.mkv.txt 2>&1"
```
  
We always capture our console logs for every encode. The ```2>&1``` ensures any error messages are output alongside the usual standard console messages for the software. These are essential for us to review if a problem is found with an encode. Over time they also provide a very clear record of changes encountered in FFmpeg and RAWcooked software, and valuable metadata of our own image sequence files. These logs have been critical in identifying unanticipated edge cases with some DPX encodings, allowing for impact assessment by Media Area. We would definitely encourage all RAWcooked users to capture and retain this information as part of their long-term preservation of their RAWcooked sequences.

  
### <a name="log_assessment">Encoding log assessment</a>

The encoding logs are critical for the automated assessment of the encoding process. Each log consists of four blocks of data:
* The RAWcooked assessment of the sequence
```
Analyzing files (0%)    
Analyzing files (0.01%), 1 files/s    
Analyzing files (0.02%), 1 files/s    
Analyzing files (0.03%), 1 files/s    
Analyzing files (0.04%), 1 files/s    
Analyzing files (0.05%), 1 files/s    
Analyzing files (0.06%), 1 files/s    
Analyzing files (0.07%), 1 files/s
...
```
* The FFmpeg console output with encoding data
```
Track 1:
  Scan01/2150x1820/%08d.dpx
 (00000000 --> 00033766)
  DPX/Raw/Y/16bit/U/BE

Attachments:
  Scan01/N_9623089_01of04_00000000.dpx_metadata.txt
  Scan01/N_9623089_01of04_directory_contents.txt
  Scan01/N_9623089_01of04_directory_total_byte_size.txt

ffmpeg version 6.0 Copyright (c) 2000-2023 the FFmpeg developers
  built with gcc 11 (Ubuntu 11.4.0-1ubuntu1~22.04)
  configuration: --prefix=/home/linuxbrew/.linuxbrew/Cellar/ffmpeg/6.0_1 --enable-shared --enable-pthreads --enable-version3 --cc=gcc-11 --host-cflags= --host-ldflags= --enable-ffplay --enable-gnutls --enable-gpl --enable-libaom --enable-libaribb24 --enable-libbluray --enable-libdav1d --enable-libmp3lame --enable-libopus --enable-librav1e --enable-librist --enable-librubberband --enable-libsnappy --enable-libsrt --enable-libsvtav1 --enable-libtesseract --enable-libtheora --enable-libvidstab --enable-libvmaf --enable-libvorbis --enable-libvpx --enable-libwebp --enable-libx264 --enable-libx265 --enable-libxml2 --enable-libxvid --enable-lzma --enable-libfontconfig --enable-libfreetype --enable-frei0r --enable-libass --enable-libopencore-amrnb --enable-libopencore-amrwb --enable-libopenjpeg --enable-libspeex --enable-libsoxr --enable-libzmq --enable-libzimg --disable-libjack --disable-indev=jack
  libavutil      58.  2.100 / 58.  2.100
  libavcodec     60.  3.100 / 60.  3.100
  libavformat    60.  3.100 / 60.  3.100
  libavdevice    60.  1.100 / 60.  1.100
  libavfilter     9.  3.100 /  9.  3.100
  libswscale      7.  1.100 /  7.  1.100
  libswresample   4. 10.100 /  4. 10.100
  libpostproc    57.  1.100 / 57.  1.100
Input #0, image2, from 'N_9623089_01of04/Scan01/2150x1820/%08d.dpx':
  Duration: 00:23:26.96, start: 0.000000, bitrate: N/A
  Stream #0:0: Video: dpx, gray16be, 2150x1820 [SAR 1:1 DAR 215:182], 24 fps, 24 tbr, 24 tbn
Stream mapping:
  Stream #0:0 -> #0:0 (dpx (native) -> ffv1 (native))
  File N_9623089_01of04/Scan01/N_9623089_01of04_00000000.dpx_metadata.txt -> Stream #0:1
  File N_9623089_01of04/Scan01/N_9623089_01of04_directory_contents.txt -> Stream #0:2
  File N_9623089_01of04/Scan01/N_9623089_01of04_directory_total_byte_size.txt -> Stream #0:3
  File ../encoded/mkv_cooked/N_9623089_01of04.mkv.rawcooked_reversibility_data -> Stream #0:4
Press [q] to stop, [?] for help
Output #0, matroska, to '../encoded/mkv_cooked/N_9623089_01of04.mkv':
```
* The post-encoding RAWcooked assessment of the FFV1 Matroska
```
...
Time=00:23:22 (99.93%), 3.0 MiB/s, 0.03x realtime    
Time=00:23:22 (99.94%), 1.0 MiB/s, 0.04x realtime    
Time=00:23:23 (99.95%), 1.3 MiB/s, 0.05x realtime    
Time=00:23:24 (99.96%), 1.2 MiB/s, 0.04x realtime    
Time=00:23:25 (99.97%), 1.1 MiB/s, 0.05x realtime    
Time=00:23:25 (99.98%), 1.2 MiB/s, 0.04x realtime    
Time=00:23:26 (99.99%), 1.6 MiB/s, 0.04x realtime    
3.3 MiB/s, 0.02x realtime 
```
* Text review of the success/failures of the encoded sequence
```
Info: Reversibility data created by RAWcooked 23.12.
Info: Uncompressed file hashes (used by reversibility check) present.

Reversibility was checked, no issue detected.
```
  
  
If an encoding has completed then in this last section you might see different types of human readable message including:
* Warnings about the image sequence files
* Errors experienced during encoding
* Information about the RAWcooked encode (shown above)
* Completion success or failure statement (shown above)
  
Error example:
```
Reversibility was checked, issues detected, see below.

Error: undecodable files are not same.
       N_7192293_01of06/Scan01/2150x1582/00014215.dpx
       N_7192293_01of06/Scan01/2150x1582/00014216.dpx
       ...
Error: undecodable files from output are not same as files from source.
       N_7192293_01of06/Scan01/2150x1582/00014215.dpx
       N_7192293_01of06/Scan01/2150x1582/00014216.dpx
       ...
At least 1 file is not conform to specifications.
```
  
Sometimes an encoding will not even start, and a single error message may be found in your log:
```
Error: unsupported DPX (non conforming) alternate end of line non padding
Please contact info@mediaarea.net if you want support of such content.
```
  
The automation scripts used at the BFI National Archive look for any messages that have 'Error' in them. If any are found the FFV1 Matroska is deleted and the sequence is queued for a repeated encode attempt.  Likewise, if the completion statement suggests a failure then the FFV1 is deleted and the sequence is queued for a repeat encode. A successful completion statement should always read:  
```Reversibility was checked, no issues detected.```  
  
There is one error message that triggers a specific type of re-encode:  
```Error: the reversibility file is becoming big | Error: undecodable file is becoming too big```  
  
For this error we know that we need to re-encode our image sequence with the additional flag ```--output-version 2``` which writes the large reversibility data to the FFV1 Matroska once encoding has completed. FFmpeg has an upper size limit of 1GB for attachments. If there is lots of additional data stored in your DPX file headers then this flag will ensure that your FFV1 Matroska completes fine and the data remains verifiably reversible. FFV1 Matroskas that are encoded using the ```--output-version 2``` flag are not backward compatible with RAWcooked version before V 21.09.  
  
  
### <a name="ffv1_valid">FFV1 Matroska validation</a>
  
When the logs have been assessed and the message ```Reversibility was checked, no issue detected``` is found, then the FFV1 Matroska is compared against the [BFI's MediaConch policy](https://github.com/bfidatadigipres/dpx_encoding/blob/main/rawcooked_mkv_policy.xml). This policy ensures that the FFV1 Matroska is whole by looking for duration field entries, checks for reversibility data, and that the correct FFV1 and Matroska formats are being used. It also ensures that all the FFV1 error detection features are present, that slices are included, bit rate is over 300 and pixel aspect ratio is 1.000.

If the policy passes then the FFV1 Matroska is moved onto the final stage, where the RAWcooked flag ```--check``` is used to ensure that the FFV1 Matroska is correctly formed.
```rawcooked --check path/sequence_name.mkv >> path/sequence_name.mkv.txt 2>&1```

Again the stderr and stdout messages are captured to a log, and this log is checked for the same confirmation message ```Reversibility was checked, no issues detected.``` When this check completes the FFV1 Matroska is moved to our Digital Preservation Infrastructure and the original DPX sequence is deleted under automation.  
  
  
### <a name="ffv1_demux">FFV1 Matroska decode to image sequence</a>

We have automation scripts that return an FFV1 Matroska back to the original image sequence. These are essential for our film preseration colleagues who may need to perform grading or enhancement work on preserved films. For this we use the ```--all``` command again which can select decode when an FFV1 Matroska is supplied.  

This simple script runs this command:  
```
rawcooked -y --all path/sequence_name.dpx -o path/decode_sequence >> path/sequence_name.txt 2>&1
```
It decodes the FFV1 Matroska back to image sequence, checks the logs for ```Reversibility was checked, no issue detected``` and reports the outcome to a script log.  
  
---
## Conclusion

We began using RAWcooked to convert 3 petabytes of 2K DPX sequence data to FFV1 Matroska for our *Unlocking Film Heritage* project. This lossless compression to FFV1 has saved us an estimated 1600TB of storage space, which has saved thousands of pounds of additional magnetic storage tape purchases. Undoubtedly this software offers amazing financial incentives with all the benefits of open standards and open-source tools. It also creates a viewable video file of an otherwise invisible DPX scan, so useful for viewing the unseen technology of film.  We plan to begin testing RAWcooked encoding of TIFF image sequences shortly with the intention of moving DCDM image sequences to FFV1. Today, our workflow runs 24/7 performing automated encoding of business-as-usual DPX sequences with relatively little overview.  There is a need for manual intervention when repeated errors are encountered, usually indicated when an image sequences doesn't make it to our Digital Preservation Infrastructure.  Most often this is caused by a new image sequence 'flavour' that we do not have covered by our RAWcooked licence, or sometimes it can indicate a problem with either RAWcooked or FFmpeg file encoding a specific DPX scan - there can be many differences found in DPX metadata depending on the scanning technology. Where errors are found by our automations these are reported to an error log named after the image seqeuence, a build up of reported errors will indicate repeated problems.  
  
In recent years we have been encoding a larger variety of DPX sequences, a mix of 2K and 4K of various bit depths has seen our licence expand. Between February 2023 and February 2024 the BFI collected data about its business-as-usual encoding capturing details of 1020 DPX encodings to CSV. A Python script was written to capture data about these encoded files, including sequence pixel size, colourspace, bits, total byte size of the image sequence and completed FFV1 Matroska.  
  
From 1020 total DPX sequences successfully encoded to FFV1 Matroska:  
* 140 were 2K or smaller / 880 were 4K
* 222 were Luma Y / 798 were RGB
* 143 were 10-bit / 279 12-bit / 598 16-bit
* The largest reduction in size of any FFV1 was 88% smaller than the source DPX (the largest reductions were from 10/12-bit sequences, with RGB colorspace that had black and white filters applied)  
* The smallest reduction saw the FFV1 just 0.3% smaller than the DPX (the smallest reductions were from RGB and Y-Luma 16-bit image sequences scanned full frame)  
* Across all 1020 encoded sequences the average size of the finished FFV1 was 29% smaller than the source image sequence  
  
A small group of sequences had their RAWcooked encoding times recorded, revealing an average of 24 hours per sequence. The sequences all had finished MKV durations between 5 and 10 minutes and were mostly 4K 16-bit sequences. The fastest encodes took just 7 hours with some taking upto 46 hours. There appears to be no cause for these variations in the files themselves and so we must assume that general network activity and/or amount of parallel processes running have influenced these variations.  
  
A separate 2K solo and parallel encoding test revealed much quicker encoding times from our servers:  
* Solo 341GB 2K RGB 12-bit sequence took 80 minutes to complete RAWcooked encoding.  MKV was 22.5% smaller than DPX. The MKV was 16 minutes and 16 seconds.  
* Solo 126GB 2K RGB 16-bit sequence tool 62 minutes to complete.  MKV was 30.6% smaller than the DPX. The duration of the MKV was 11 mins 42 secs.  
* Parallel 367GB/325GB 2K RGB 16-bit sequences took 160 minutes/140 minutes to complete.  MKVs were 27.6% and 24.4% smaller than their DPX respectively. The durations were 11 mins 34 secs, and 10 mins 15 secs.  
   
### <a name="tests">Useful test approaches</a>
  
When any system upgrades occur we like to run reversibility test to ensure RAWcooked is still operating as we would expect. This is usually in response to RAWcooked software updates, FFmpeg updates, but also for updates to our operating system. To perform a reversibility test, a cross-section of image sequences are encoded using our usual ```--all``` command, and then decoded again fully. The image sequences of both the original and decoded version then have whole file MD5 checksums generated for every and saved into one manifest for the source and one for the decoded version. These manifests are then ```diff``` checked to ensure that every single image file is identical.  
  
To have confidence in the --check feature, which confirms for us a DPX sequence can be deleted, we ran several --check command tests that included editing test FFV1 Matroska metadata using hex editor software, and altering test DPX files in the same way while partially encoded. The encoding/check features always identified these data breakages correctly which helped build our confidence in the --all and --check flags.  
  
When we encounter an error there are a few commands used that make reporting the issue a little easier at the [Media Area RAWcooked GitHub issue tracker](https://github.com/MediaArea/RAWcooked/issues).  
```
rawcooked -d -y -all --accept-gaps <path/sequence_name>  
```
Adding the ```-d``` flag doesn't run the encoding but returns the command that would be sent to FFmpeg. This flag also leaves the reversibility data available as a text file and this is useful for sending to Media Area to help with finding errors.  
```
head -c 1048576 sequence_name.mkv > dump_file.mkv  
```
This command uses Linux ```head``` command to cut the first 1MB of data from a supplied file, copying it to a new file which is easier to forward to Media Area for review.  This contains the file's header data, often requested when a problem has occurred.  
```
echo $?
```
This command should be run directly after a failed RAWcooked encode, and it will tell you the exit code returned from that terminated run.  
  
The results of these three enquiries is always a brilliant way to open an Issue enquiry for Media Area and will help ensure swift diagnose for your problem. It may also be necessary to supply a DPX sequence, and your ```head``` command can be used again to extract the header data.
  
  
## <a name="links">Additional resources</a>  
  
* [BFI National Archive DPX Preservation Workflows](https://digitensions.home.blog/2019/11/08/dpx-preservation-workflow/)
* [Media Area's RAWcooked GitHub page](https://github.com/MediaArea/RAWcooked)
* ['No Time To Wait! 5' presentation by Joanna White about the BFI's evolving RAWcooked use](https://www.youtube.com/watch?v=Mgo_DKHJEfI) 
* [BFI National Archive RAWcooked cheat sheet for optimization](https://github.com/bfidatadigipres/dpx_encoding/blob/main/RAWcooked_Cheat_Sheet.pdf)  
* [Further conference presentations about BFI National Archive use of RAWcooked, by Joanna White](https://youtu.be/4cG5RL_CZqg?si=w-iEICSfXqBco5NB)
* [RAWCooking With Gas: A Film Digitization and QC Workflow-in-progress by Genevieve Havemeyer-King](https://youtu.be/-cJxq7Vr3Nk?si=BjPWzsZ7LRKMVZNF)
* [Introduction to FFV1 and Matroska for film scans by Kieran O’Leary](https://kieranjol.wordpress.com/2016/10/07/introduction-to-ffv1-and-matroska-for-film-scans/)
