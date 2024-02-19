# RAWcooked Case Study  
  
**BFI National Archive**  
**Joanna White, Knowledge & Collections Developer**  
  
At the [BFI National Archive](https://www.bfi.org.uk/bfi-national-archive) we have been encoding DPX sequences to FFV1 Matroska since late 2019. In that time our RAWcooked workflow has evolved with the development of RAWcooked, DPX resolutions and flavours and changes in our encoding project priorities.  Today we have a fairly hands-off automated workflow which handles 2K, 4K, RGB, Luma, DPX and Tiff image sequences.  This workflow is built on some of the flags developed by the Media Area and written in a mix of BASH shell scripts and Python3 scripts and is available to view from the [BFI Data & Digital Preservation GitHub](https://github.com/bfidatadigipres/dpx_encoding). In addition to our RAWcooked use I will also consider how we use other Media Area tools alongside RAWcooked to complete necessary stages of this workflow.  Our encoding processes do not include any alpha channels or audio file processing, but RAWcooked is capable of encoding both into the completed FFV1 Matroska dependent upon your licence.
  
This case study is broken into the following sections:  
* [Server configuration](#server_config)
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
  
To encode our DPX and TIFF sequences we have a single server that completes this work for all our different NAS storage paths in parallel.  
  
Our current configuration:  
- Intel(R) Xeon(R) Gold 5218 CPU @ 2.30GHz  
- 252 GB RAM  
- 32-core with 64 CPU threads  
- Ubuntu 20.04 LTS  
- 40Gbps Network card  
- NAS storage on 40GB network  
  
The more CPU threads you have the better your FFmpeg encode to FFV1 will perform. To calculate the CPU threads for your server you can multiply the Threads x Cores x Sockets. So for our congiguration this would be 2 (threads) x 16 (sockets) x 2 (cores) = 64. To retrieve these figures we would use Linux's ```lscpu```.
  
Our previous server configuration:  
- Virtual Machine of a NAS storage device  
- AMD Opteron 22xx (Gen 2 Class Opteron)  
- 12GB RAM  
- 8-core @ 3 GHz (estimated)  
- 8 threads  
- Ubuntu 18.04 LTS  
  
When encoding 2K RGB we generally reach between 3 and 10 frames per second (fps) from FFmpeg, 4K scans is generally 1 fps or less. These figures can be impacted by the quantity of parellel processes running at any one time.

---  
# Workflow
### <a name="assessment">Image sequence assessment</a>  
  
For each image sequence processed the metadata of the first DPX or TIFF is collected and saved into the sequence folder, along with total folder size in bytes and a list of all contents of the sequence. We collect this information using [Media Area's MediaInfo software](https://mediaarea.net/MediaInfo) and capture the output into script variables.  
  
Next the first file within the image sequence is checked against a [Media Area's MediaConch software](https://mediaarea.net/MediaConch) policy for the file ([BFI's DPX policy](https://github.com/bfidatadigipres/dpx_encoding/blob/main/rawcooked_dpx_policy.xml)). If it passes then we know it can be encoded by RAWcooked and by our current licence. Any that fail are assessed for possible RAWcooked licence expansion, or possible anomalies in the DPX resulting.
  
The pixel size and colourspace of the sequence are used to calculate the potential reduction rate of the encode based on previous encoding experience. We make an assumption that 2K RGB will always be atleast one third smaller, so calculate a 1.3TB sequences to make a 1TB FFV1 Matroska.  For 2K Luma and all 4K we must assume that very small size reductions will take place. This step is necessary to control file ingest sizes to our Digital Preservation Infrastructure where we currently have a maximum verifiable ingest file size of 1TB.
  
| RAWcooked 2K RGB     | RAWcooked Luma & RAWcooked 4K |
| -------------------- | ----------------------------- |
| 1.3TB reduces to 1TB | 1.0TB may only reduce to 1TB  |
  

### <a name="muxing">Muxing the image sequence</a>  

To encode our image sequences we use the ```--all``` flag released in RAWcooked v21. This flag was a sponsorship development by [NYPL](https://www.nypl.org/), and sees several preservation essential flags merged into this one simple flag. Most imporantly it includes the creation of checksum hashes for every image file in the sequence, with this data being saved into the RAWcooked reversibility file embedded into the Matroska wrapper. This ensures that when decoded the retrieved sequence can be varified as bit-identical to the original source sequence.

Our RAWcooked encode command:
```
rawcooked -y --all --no-accept-gaps -s 5281680 path/sequence_name/ -o path/sequence_name.mkv >> path/sequence_name.mkv.txt 2>&1
```
  
| Command                | Description                                |
| ---------------------- | ------------------------------------------ |
| ```rawcooked```        | Calls the software                         |
| ```-y```               | Answers 'yes' to software questions        |
| ```-all```             | Preservation command with checksums        |
| ```--no-accept-gaps``` | Exit with warning if sequence gaps found   |
| ```-s 5281680```       | Set max attachment size to 5MB             |
| ```-o```               | Use output path for FFV1 MKV               |
| ```>>```               | Capture console output to text file        |
| ```2>&1```             | stderr and stdout messages captured in log |

This command is generally launched from within a Bash script, and is passed to [GNU Parallel](https://www.gnu.org/software/parallel/) to run multiple encodes in parallel. This software makes it very simple to run a number of encodes specified by the ```--job``` flag. Parallelisation is the act of processing jobs in parallel, dividing up the work to save time. If not run in parallel a computer will usually process jobs one after another. As well as parallelisation, FFmpeg usinges multi-threading to create the FFV1 file. The FFV1 codec has slices through each frame (from 64 slices per RAWcooked frame) which allows for granular checksum verification, but also allows FFmpeg multi-threading. Each slice block is split into different processing tasks and run across your CPU threads, so four our server that 64 separate tasks per thread, one slice per frame of the FFV1 file.

By listing all the image sequence paths in one text file you can launch a parallel command like this to run 5 parallel encodes:
```
cat ${sequence_list.txt} | parallel --jobs 5 "rawcooked -y --all --no-accept-gaps -s 5281680 {} -o {}.mkv >> {}.mkv.txt 2>&1"
```
  
We always capture our console logs for every encode. The ```2>&1``` ensures any error messages are output alongside the usual standard console messages for the software. These are essential for us to review if a problem is found with a encode. Over time they also provide a very clear record of changes encountered in FFmpeg and RAWcooked software, and data of our own image sequence files. These logs have been critical in identifying unanticipated edge cases with some DPX encodes, allowing for impact assessment by Media Area. We definitely encourage all RAWcooked users to capture and retain this information as part of their long-term preservation metadata collection for an image sequence.

  
### <a name="log_assessment">Encoding log assessment</a>

The encoding logs are critical for the automated assessment of the encoding process. Each log consists of four blocks of data:
* The RAWcooked assessment of the sequence
* The FFmpeg encoding data
* The post-encoding RAWcooked assessment of the FFV1 Matroska
* Text review of the success of the encoded sequence

The RAWcooked assessments themselves are lines of repeated data, counting from 0% to 100%. The FFmpeg encoding data contains sequence and FFV1 metadata, along with choices made by the software for the encoding process and logs of the fps for the encoding of the sequence. All this information is really important when there's an issue with the encoding. The final text review is generated by the RAWcooked assessment of the image sequence and the FFV1 Matroska. In this last section you can be given different types of human readable message including:
* Warnings about the image sequence files
* Errors experienced during encoding
* Information about the RAWcooked encode (RAWcooked version, if checksum hashes included)
* Completion success or failure statement

The automation scripts used a the the BFI National Archive largely ignore the warning messages, but look for any messages that have 'Error' in them. If any are found the FFV1 Matroska is deleted and the sequence is queued for a repeated encode attempt.  Likewise, if the completion statement suggests a failure then the FFV1 is deleted and the sequence is queued for a repeat encode. A successful completion statement should always read:
```Reversibility was checked, no issues detected.```
  
There is one error message that triggers a specific type of re-encode:
```Error: the reversibility file is becoming big | Error: undecodable file is becoming too big```

For this error we know that we need to re-encode our image sequence with the additional flag ```--output-version 2``` which writes the large reversibility data to the FFV1 Matroska once encode is completed. FFmpeg has an upper size limit of 1GB for attachments. If there is lots of additional data stored in your DPX file headers then this flag will ensure that your FFV1 Matroska completes fine and the data remains verifiably reversible. FFV1 Matroskas that are encoded using the ```--output-version 2``` flag are not backward compatible with RAWcooked version before V 21.09.
  
  
### <a name="ffv1_valid">FFV1 Matroska validation</a>
  
When the logs have been assessed and the message ```Reversibility was checked, no issue detected``` was found, then the FFV1 Matroska has metadata validation using the [BFI's MediaConch policy](https://github.com/bfidatadigipres/dpx_encoding/blob/main/rawcooked_mkv_policy.xml). This policy ensures that the FFV1 Matroska is whole by looking for duration field entries, checks for reversibility data, and that the correct FFV1 and Matroska formats are being used. It also ensures that all the FFV1 error detection features are present, that slices are included, bit rate is over 300 and pixel aspect ratio is 1.000.

If the policy passes then the FFV1 Matroska is moved onto the final stage, where the RAWcooked flag ```--check``` is used to ensure that the FFV1 Matroska is correctly formed.
```rawcooked --check path/sequence_name.mkv >> path/sequence_name.mkv.txt 2>&1```

Again the stderr and strout messages are captured to a log, and this log is checked for the message ```Reversibility was checked, no issues detected.``` When this check completes the FFV1 Matroska is moved to our Digital Preservation Infrastructure and the original image sequence is deleted under automation.
  
  
### <a name="ffv1_demux">FFV1 Matroska decode to image sequence</a>

We have automation scripts that return an FFV1 Matroska back to the original image sequence. These are essential for our film preseration colleagues who may need to perform grading or enhancement work on preserved films. For this we use the ```--all``` command again which can select decode when an FFV1 Matroska is supplied.  

This simple script runs this command:  
```
rawcooked -y --all path/sequence_name.dpx -o path/decode_sequence >> path/sequence_name.txt 2>&1
```
It decodes the FFV1 Matroska back to image sequence, checks the logs for ```Reversibility was checked, no issue detected``` and reports the outcome to a script log.  
  
---
# Conclusion

We began using RAWcooked to convert 3PB of 2K image sequence data to FFV1 Matroska for our *Unlocking Film Heritage* project. This lossless compression to FFV1 has saved us an estimated 1600TB of storage space. Our workflows run 24/7 performing automated encoding of business as usual DPX sequences with relatively little overview.  There is a need for manual intervention when repeated errors are encountered, usually indicated when an image sequences doesn't make it to Digital Preservation Infrastructure.  Most often this signals a different image sequence 'flavour' that we do not have in our licence, but sometimes it can indicate a problem with either RAWcooked or FFmpeg file encoding. Where errors are found by our automations these are reported to an error log named after the image seqeuence, a build up will indicate repeated problems. 
  
In recent years we have been encoding a larger variety of DPX sequences, a mix of 2K and 4K of various bit depths. Between Febraury 2023 and February 2024 the BFI collected data about its business as usual encoding capturing details of 1020 DPX encodings to CSV. A Python script was written to capture data about these encoded files, including sequence pixel size, colourspace, bits, total byte size of the image sequence and completed FFV1 Matroska.
  
From 1020 total DPX sequences successfully encoded to FFV1 Matroska:  
* 140 were 2K or smaller / 880 were 4K
* 222 were Luma Y / 798 were RGB
* 143 were 10-bit / 279 12-bit / 598 16-bit
* The largest reduction in size of any FFV1 from the DPX was 88%
* The smallest reduction was just 0.3%
* The largest reductions were from 10/12-bit sequences, with RGB colorspace that had black and white filters applied
* The smallest reductions were from RGB and Y-Luma 16-bit image sequences scanned full frame
* Across all 1020 encoded sequences the average size of the finished FFV1 was 71% of the original image sequence  
  
A small group of sequences had their total RAWcooked encoding time recorded, revealing an average of 24 hours per sequence. The sequences all had finished MKV durations were between 5 and 10 minutes. The fastest encodes took just 7 hours with some taking 46 hours. There appears to be no cause for these variations in the files themselves and so we must assume that general network activity and/or amount of parallel processes running have influenced these variations.  
   
### <a name="tests">Some useful test approaches</a>
  
When any system upgrades occur we like to run reversibility test to ensure RAWcooked is still operating as we would expect. This is usually in response to RAWcooked software updates, FFmpeg updates, but also for updates to our operating system. To perform a reversibility test, a cross-section of image sequences are encoded using our usual ```--all``` command, and then decoded again fully. The image sequences of both the original and decoded version then have whole file MD5 checksums generating and saving to a manifest. These manifests are then ```diff``` checked to ensure that every single image file is identical.
  
When we encounter an error there are a few commands I use that make reporting the issue a little easier at the [Media Area RAWcooked GitHub issue tracker](https://github.com/MediaArea/RAWcooked/issues).  
```
rawcooked -d -y -all --accept-gaps <path/sequence_name>  
```
Adding the ```-d``` flag doesn't run the encoding, but returns the command sent to FFmpeg. This flag also leaves the reversibility data available to view as a text file and this is useful for finding errors.  
```
head -c 1048576 sequence_name.mkv > dump_file.mkv  
```
This command uses UNIX ```head``` command to cut the first 1MB of data from a supplied file, copying it to a new file which is easier to forward to Media Area for review.  This contains the file's header data, often requested when a problem has occurred.  
```
echo $?
```
This command should be run directly after a failed RAWcooked encode, and it will tell you the exit code returned from that terminated run.  
  
The results of these three enquiries is always a brilliant way to open an Issue enquiry for Media Area and will help ensure swift diagnose for your problem. It may also be necessary to supply a DPX sequence, and your ```head``` command can be used again to extract the header data.


## <a name="links">Additional resources</a>  

* [RAWcooked GitHub page](https://github.com/MediaArea/RAWcooked)
* ['No Time To Wait! 5' presentation about the BFI's evolving RAWcooked use](https://www.youtube.com/@MediaAreaNet/streams). Link to follow.  
* [RAWcooked cheat sheet for optimization](https://github.com/bfidatadigipres/dpx_encoding/blob/main/RAWcooked_Cheat_Sheet.pdf)  
* [Further conference presentations about BFI National Archive use of RAWcooked](https://github.com/MediaArea/RAWcooked/issues)
* [DPX Preservation Workflows](https://digitensions.home.blog/2019/11/08/dpx-preservation-workflow/)
* [Introduction to FFV1 and Matroska for film scans by Kieran O’Leary](https://kieranjol.wordpress.com/2016/10/07/introduction-to-ffv1-and-matroska-for-film-scans/)
* [RAWCooking With Gas: A Film Digitization and QC Workflow-in-progress by Genevieve Havemeyer-King](https://youtu.be/-cJxq7Vr3Nk?si=BjPWzsZ7LRKMVZNF)
