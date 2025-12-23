# BFI National Archive RAWcooked Case Study  
**by Joanna White, Knowledge Learning & Collections Developer**  
  
At the [BFI National Archive](https://www.bfi.org.uk/bfi-national-archive) we have been encoding DPX sequences to FFV1 Matroska since late 2019. In that time our RAWcooked workflow has evolved with the development of RAWcooked, DPX resolutions, flavours and changes in our encoding project priorities.  Today we have a fairly hands-off automated workflow which handles 2K and 4K image sequences.  This workflow is built on some of the flags developed in RAWcooked by Media Area and written in a mix of Bash shell and Python3 scripts ([BFI Data & Digital Preservation GitHub](https://github.com/bfidatadigipres/dpx_encoding)). In addition to RAWcooked we use other Media Area tools to complete necessary stages of this workflow.  Our encoding processes do not include any alpha channel or audio file processing, but RAWcooked is capable of encoding both into the completed FFV1 Matroska.

A note on our RAWcooked performance in context of the operational network: This case study covers DPX sequence processing within the operational network where we run dozens of Windows and Mac workstations, and a similar number of Linux servers, addressing 20 nodes of NAS storage - all these devices connected via a mixture of 10Gbps, 25Gbps and 40Gbps. Data flows to the network storage from automated off-air television recording (over 20 channels), born-digital acquisition of cinema and streaming platform content, videotape and film digitisation workflows, as well as media restored from our data tape libraries. And data flows from the network storage to the data tape libraries as we ingest at high volume. We are in the process of upgrading the network to use only 100Gbps switches with higher-than-10Gbps cards on all critical devices; but meanwhile the throughput we achieve is constrained by the very heavy concurrent use of the network for many high-bitrate audiovisual data workflows; and this network contention impacts on the speed of our RAWcooked workflows.
  
This case study is broken into the following sections:  
* [Server configurations](#server_config)
* [Workflow: Image sequence assessment](#assessment)  
* [Workflow: Encoding the image sequence](#muxing)  
* [Workflow: Encoding log assessments](#log_assessment)  
* [Workflow: FFV1 Matroska validation](#ffv1_valid)  
* [Workflow: FFV1 Matroska decode to image sequence](#ffv1_demux)
* [Conclusion](#conclusion)
* [Useful test approaches](#tests)  
* [Sublicensing to suppliers](#sublicense)  
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
  
Following recent benchmark tests using FFmpeg version 6.0 against internal server HDD storage using 200 frame sequence of 4K 10-bit DPX we received a maximum of 11 fps performance. We are aware this is much slower than anticipated FFmpeg performace speeds and will investigate potential causes in time. This does imply all supplied durations for encoding processes following can be improved with higher fps results. When encoding 2K RGB we generally reach between 3 and 10 frames per second (fps), and 4K scans can be up to 5.5 fps. These figures can be impacted by the quantity of parallel processes running at any one time.  

[ Jérôme values here ]

---  
## Workflow
### <a name="assessment">Image sequence assessment</a>  
  
For each image sequence processed the metadata of the first DPX is collected and saved into the sequence folder, along with total folder size in bytes and a list of all contents of the sequence. We collect this information using [Media Area's MediaInfo software](https://mediaarea.net/MediaInfo) and capture the output into script variables.  
  
Next the first file within the image sequence is checked against a DPX policy created using [Media Area's MediaConch software](https://mediaarea.net/MediaConch) - ([BFI's DPX policy](https://github.com/bfidatadigipres/dpx_encoding/blob/main/policies/rawcooked_dpx_policy.xml)). If it passes then we know it can be encoded by RAWcooked and by our current licence. Any that fail are assessed for possible RAWcooked licence expansion or possible anomalies in the DPX.
  
The frame pixel size and colourspace of the sequence are used to calculate the potential reduction rate of the RAWcooked encode based on previous reduction experience. We make an assumption that 2K RGB will always be at least one third smaller, so calculate a 1.3TB sequence will make a 1TB FFV1 Matroska.  For 2K Luma and all 4K we must assume that very small size reductions could occur so map 1TB to 1TB. This step is necessary to control file ingest sizes to our Digital Preservation Infrastructure where we currently have a maximum verifiable ingest file size of 1TB. Where a sequence is over 1TB we have Python scripts to split that DPX sequence across additional folders depending on total size.
  
| RAWcooked 2K RGB     | RAWcooked Luma & RAWcooked 4K |
| -------------------- | ----------------------------- |
| 1.3TB reduces to 1TB | 1.0TB may only reduce to 1TB  |
  

### <a name="muxing">Encoding the image sequence</a>  

To encode our image sequences we use the ```--all``` flag released in RAWcooked v21. This flag was a sponsorship development by [NYPL](https://www.nypl.org/), and sees several preservation essential flags merged into one. Most importantly it includes the creation of checksum hashes for every image file in the sequence, with this data being saved into the RAWcooked reversibility file and embedded into the Matroska wrapper. This ensures that when decoded the retrieved sequence can be verified as bit-identical to the original source sequence.

Our RAWcooked encode command:
```
rawcooked -y --all --no-accept-gaps -s 5281680 path/sequence_name/ -o path/sequence_name.mkv >> path/sequence_name.mkv.txt 2>&1
```
  
| Command                | Description                                |
| ---------------------- | ------------------------------------------ |
| ```rawcooked```        | Calls the software                         |
| ```-y```               | Answers 'yes' to software questions        |
| ```--all```            | Preservation command with CRC-32 hashes    |
| ```--no-accept-gaps``` | Exit with warning if sequence gaps found   |
|                        | --all command defaults to accepting gaps   |
| ```-s 5281680```       | Set max attachment size to 5MB             |
| ```-o```               | Use output path for FFV1 MKV               |
| ```>>```               | Capture console output to text file        |
| ```2>&1```             | stderr and stdout messages captured in log |

This command is generally launched from within a Bash script, and is passed to [GNU Parallel](https://www.gnu.org/software/parallel/) to run concurrent encodings. This software makes it very simple to fix a specific number of encodes using the ```--jobs``` flag. Parallelisation is the act of processing jobs in parallel, dividing up the work across threads to maximise efficiency. If not run in parallel a computer will usually process jobs serially, one after another. As well as parallelisation, FFmpeg uses multi-threading to create the FFV1 file. The FFV1 codec has slices through each frame (usually between 64 and 576 slices) which allows for granular checksum verification, but also allows FFmpeg multi-threading. Each slice block is split into different processing tasks and run across your CPU threads, so for our server that works as 64 separate tasks per thread, one slice per frame of the FFV1 file.

By listing all the image sequence paths in one text file you can launch a parallel command like this to run 5 parallel encodes:
```
cat ${sequence_list.txt} | parallel --jobs 5 "rawcooked -y --all --no-accept-gaps -s 5281680 {} -o {}.mkv >> {}.mkv.txt 2>&1"
```
  
We always capture our console logs for every encode job. The ```2>&1``` ensures any error messages are output alongside the usual standard console messages for the software. These are essential for us to review if a problem is found with an FFV1 Matroska. Over time they also provide a very clear record of changes encountered in FFmpeg and RAWcooked software, and valuable metadata of our own image sequence files. These logs have been critical in identifying unanticipated edge cases with some DPX encodings, allowing for impact assessment by Media Area. We would definitely encourage all RAWcooked users to capture and retain this information as part of the long-term preservation of RAWcooked encoded sequences.

  
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
* The encoding outputs which give current frame, frames per second, size of data processed, timecode for FFV1, bitrate and speed data
```
frame=  0 fps=0.0 q=-0.0 size=       0kB time=00:00:00.00 bitrate=N/A speed=N/A  
frame=  1 fps=0.7 q=-0.0 size=    4864kB time=00:00:00.04 bitrate=948711.6kbits/s speed=0.028x
frame=  3 fps=1.3 q=-0.0 size=   52736kB time=00:00:00.12 bitrate=3456106.5kbits/s speed=0.0531x
frame=  5 fps=1.6 q=-0.0 size=  153344kB time=00:00:00.20 bitrate=6039394.5kbits/s speed=0.0665x
frame=  7 fps=1.8 q=-0.0 size=  254464kB time=00:00:00.29 bitrate=7138935.2kbits/s speed=0.0749x
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
* Text review of the success/failures of the encoded sequence provided by the -info flag
```
Info: Reversibility data created by RAWcooked 24.11.
Info: Uncompressed file hashes (used by reversibility check) present.
Info: Reversibility was checked, no issue detected.
```
  
  
If an encoding has completed then in this last section you might see different types of messages that include:
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
  
The automation scripts used at the BFI National Archive look for any messages that have 'Error' in them. If any are found the FFV1 Matroska is deleted and the sequence is queued for a repeated encoding attempt.  Likewise, if the completion statement suggests a failure then the FFV1 is deleted and the sequence is queued for a repeat encode. A successful completion statement should always read:  
```Reversibility was checked, no issues detected.```  
  
There is one error message that triggers a specific type of re-encode:  
```Error: the reversibility file is becoming big | Error: undecodable file is becoming too big```  
  
For this error we know that we need to re-encode our image sequence with the additional flag ```--output-version 2``` which writes the large reversibility data to the FFV1 Matroska once encoding has completed. FFmpeg has an upper size limit of 1GB for attachments. If there is additional data stored in your DPX file headers (not zero padding) then this flag will ensure that this data is stored safely into the reversibility data and that the FFV1 Matroska remains verifiably reversible. FFV1 Matroskas that are encoded using the ```--output-version 2``` flag are not backward compatible with RAWcooked version before V21.09.  
  
  
### <a name="ffv1_valid">FFV1 Matroska validation</a>
  
When the logs have been assessed and the message ```Reversibility was checked, no issue detected``` is found, then the FFV1 Matroska is compared against the [BFI's MediaConch policy](https://github.com/bfidatadigipres/dpx_encoding/blob/main/policies/rawcooked_mkv_policy.xml). This policy ensures that the FFV1 Matroska is whole by looking for duration field entries, checks for reversibility data, and that the correct FFV1 and Matroska formats are being used. It also ensures that all the FFV1 error detection features are present, that slices are included, bit rate is over 300 and pixel aspect ratio is 1.000.

If the policy passes then the FFV1 Matroska is moved onto the final stage, where the RAWcooked flag ```--check``` is used to ensure that the FFV1 Matroska is correctly formed.
```rawcooked --check path/sequence_name.mkv >> path/sequence_name.mkv.txt 2>&1```

Again the stderr and stdout messages are captured to a log, and this log is checked for the same confirmation message ```Reversibility was checked, no issues detected.``` When this check completes the FFV1 Matroska is moved to our Digital Preservation Infrastructure and the original DPX sequence is deleted under automation.  
  
  
### <a name="ffv1_demux">FFV1 Matroska decode to image sequence</a>

We have automation scripts that return an FFV1 Matroska back to the original image sequence. These are essential for our film preservation colleagues who may need to perform grading or enhancement work on preserved films. For this we use the ```--all``` command again which automatically selects decode when an FFV1 Matroska is supplied.  

This simple script runs this command:  
```
rawcooked -y --all path/sequence_name.dpx -o path/decode_sequence >> path/sequence_name.txt 2>&1
```
It decodes the FFV1 Matroska back to it's original form as a DPX image sequence, checks the logs for ```Reversibility was checked, no issue detected``` and reports the outcome to a script log.  
  
---
## Conclusion

We began using RAWcooked to convert 3 petabytes of 2K DPX sequence data to FFV1 Matroska for our *Unlocking Film Heritage* project. This lossless compression to FFV1 has saved us an estimated 1600TB of storage space, which has saved thousands of pounds of additional magnetic storage tape purchases. Undoubtedly this software offers amazing financial incentives with all the benefits of open standards and open-source tools. It also creates a viewable video file of an otherwise invisible DPX scan, so useful for viewing the unseen technology of film.    
  
Today, our workflow runs 24/7 performing automated encoding of business-as-usual DPX sequences with relatively little overview.  There is a need for manual intervention when repeated errors are encountered. This is usually indicated in error logs or when an image sequences doesn't make it to our Digital Preservation Infrastructure.  Most often this is caused by a new image sequence 'flavour' that we do not have covered by our RAWcooked licence, or sometimes it can indicate a problem with either RAWcooked or FFmpeg while encoding a specific DPX scan. There can be many differences found in DPX metadata depending on the scanning technology used. Where errors are found by our automations these are reported to an error log named after the image sequence.
  
Our 2K workflows could run multiple parallel processes with good efficiency, seeing as many as 32 concurrent encodings running at once against a single storage device. This was before we implemented the ```--all``` command which calculates checksums adding them to the reversibility data and runs a checksum comparison of the Matroska after encoding has completed which expands the encoding process. When introducing this command we reduced our concurrency, particularly as our workflow introduced a final ```--check``` pass against the Matroska file that automated the deletion of the DPX sequence, when successful. We also expanded our storage devices for RAWcooking and currently have 8 storage devices (a mix of Isilon, QNAPs and G-Rack NAS) generally set for between 2 and 8 concurrent encodings with the aim of not exceeding 32.  
  
In recent years we have seen a shift from majority 2K DPX to majority 4K DPX with mostly 12- or 16-bit depths. To maintain speed of specific DPX throughout it is better to limit our parallel encodings to two DPX per storage at any given time. Below are some recent 4K DPX encoding times using RAWcooked's ```--all``` command with a maximum of just two parallel encodings per server targeting a single QNAP storage, and where we can assume a single ```--check``` run is underway also:  
  
* Parallel 4K RGB 16-bit DPX (699.4 GB) - MKV duration 5:10 (639.8 GB) - encoding time 5:17:00 - MKV 8.5% smaller than DPX  
* Parallel 4K RGB 16-bit DPX (723.1 GB) - MKV duration 5:20 (648.9 GB) - encoding time 5:40:07 - MKV 10.25% smaller than DPX  
* Parallel 4K RGB 16-bit DPX (1078.7 GB) - MKV duration 9:56 (954.9 GB) - encoding time 7:49:20 - MKV 11.5% smaller than DPX  
* Parallel 4K RGB 12-bit DPX (796.3 GB) - MKV duration 9:47 (194.1 GB) - encoding time 5:13:22 - MKV 75.6% smaller than DPX **  
* Parallel 4K RGB 12-bit DPX (118.1 GB) - MKV duration 1:27 (87.1 GB) - encoding time 1:06:02 - MKV 26.3% smaller than DPX  
* Parallel 4K RGB 12-bit DPX (121.6 GB) - MKV duration 1:29 (87.3 GB) - encoding time 0:54:00 - MKV 28.2% smaller than DPX  
* Parallel 4K RGB 12-bit DPX (887.3 GB) - MKV duration 10:54 (208.7 GB) - encoding time 5:02:00 - MKV 76.5% smaller than DPX **  
** Where the MKV is significantly smaller than the DPX then a black and while filter will have been applied to an RGB scan, as in these cases.  
  
A separate 2K solo and parallel encoding test revealed much quicker encoding times for >10 minute sequences, again using the ```--all``` command against a single QNAP storage, and where we can assume another single ```--check``` run is also working in parallel:  
  
* Solo 2K RGB 12-bit DPX (341 GB) - MKV duration 16:16 - encoding time 1:20:00 - MKV 22.5% smaller than DPX
* Solo 2K RGB 16-bit DPX (126 GB) - MKV duration 11:42 - encoding time 1:02:00 - MKV was 30.6% smaller than the DPX  
* Parallel 2K RGB 16-bit DPX (367 GB) - MKV duration 11:34 - encoding time 2:40:00 - MKV was 27.6% smaller than the DPX
* Parallel 2K RGB 16-bit DPX (325 GB) - MKV duration 10:15 - encoding time 2:21:00 - MKV was 24.4% smaller than the DPX
  
It provides us with great reassurance to implement the ```--all``` command and we remain highly satisfied with RAWcooked encoding of DPX sequences despite the reduction in our concurrent encodings. The embedded DPX hashes which ```--all``` includes are critical for long-term preservation of the digitised film. In addition there are checksums embedded in the slices of every video frame (up to 576 checksums *per* video frame) allowing granular analysis of any problems found with digital FFV1 preservation files, should they arise. This is thanks to the FFV1 codec, and it allows us to pinpoint exactly where digital damage may have occurred. This means we can easily replace the impacted DPX files using our duplicate preservation copies. Open-source RAWcooked, FFV1 and Matroska allow open access to their source code which means reduced likelihood of obsolescence long into the future. Finally, we plan to begin testing RAWcooked encoding of TIFF image sequences with the intention of encoding DCDM image sequences to FFV1 also.  
   
### <a name="tests">Useful test approaches</a>
  
When any system upgrades occur we like to run reversibility test to ensure RAWcooked is still operating as we would expect. This is usually in response to RAWcooked software updates, FFmpeg updates, but also for updates to our operating system. To perform a reversibility test, a cross-section of image sequences are encoded using our usual ```--all``` command, and then decoded again fully. The image sequences of both the original and decoded version then have whole file MD5 checksums generated for every DPX which are written into a manifest for the source DPX and a manifest for the decoded DPX. These manifests are then ```diff``` checked to ensure that every single image file has identical checksums.  
  
To have confidence in the ```--check``` feature, which confirms for us a DPX sequence can be deleted, we ran several ```--check``` command tests that included editing test FFV1 Matroska metadata using hex editor software, and altering test DPX files in the same way during the encoding run. The encoding/check features always identified these data breakages correctly which helped build our confidence in the ```--all``` and ```--check``` flags.  
  
When we encounter an error there are a few commands used that make reporting the issue a little easier at the [Media Area RAWcooked GitHub issue tracker](https://github.com/MediaArea/RAWcooked/issues).  
```
rawcooked -d -y -all --no-accept-gaps <path/sequence_name>  
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
  
The results of these three enquiries is always a great help when opening an Issue enquiry for Media Area aiding diagnosis of your problem. It may also be necessary to supply a DPX image, and your ```head``` command can be used again to extract the header data.
  

### <a name="sublicense">Sublicensing to suppliers</a>

We use our RAWcook license internally for encoding BFI scans, but we also supply sublicenses to partner companies using RAWcooked's sublicensing option. To do this we can create sublicenses via the CLI:  
  
```  
rawcooked --sublicense <license number 1-126> 
rawcooked --sublicense-dur <integer for duration>  
```  
So to create a sublicense I would use the ```--sublicense``` flag with a license number between 1 and 126, then use ```--sublicense-dur``` flag and supply an integer. The integer would represent the month starting from '0', which represents the remainder of this calendar month. To create a six month license including this month (assuming it's the first), you would supply '5'.  
  
```  
rawcooked --sublicense 1 --sublicense-dur 0
```  
The first command would then create a long 'Sub-licensee license key', which we would supply to our partner company who could use our version of RAWcooked for the rest of this calendar month.  
  
  
## <a name="links">Additional resources</a>  
  
* [Media Area's RAWcooked GitHub page](https://github.com/MediaArea/RAWcooked)
* ['No Time To Wait! 5' presentation by Joanna White about the BFI's evolving RAWcooked use](https://www.youtube.com/watch?v=Mgo_DKHJEfI) 
* [BFI National Archive RAWcooked cheat sheet for optimization](https://github.com/bfidatadigipres/dpx_encoding/blob/main/RAWcooked_Cheat_Sheet.pdf)  
* [BFI National Archive DPX Preservation Workflows](https://digitensions.home.blog/2019/11/08/dpx-preservation-workflow/)
* [Further conference presentations about BFI National Archive use of RAWcooked, by Joanna White](https://youtu.be/4cG5RL_CZqg?si=w-iEICSfXqBco5NB)
* [RAWCooking With Gas: A Film Digitization and QC Workflow-in-progress by Genevieve Havemeyer-King](https://youtu.be/-cJxq7Vr3Nk?si=BjPWzsZ7LRKMVZNF)
* [Introduction to FFV1 and Matroska for film scans by Kieran O’Leary](https://kieranjol.wordpress.com/2016/10/07/introduction-to-ffv1-and-matroska-for-film-scans/)
