# Structure of RAWcooked reversibility data 

Beside video and audio content, losslessly compressed with adapted lossless format, some sidecar data is needed for guaranteeing the reversibility to the original files.  
RAWcooked reversibility data is built for a specific file, with specific options. I most cases, RAWcooked reversibility data is still usable after a remux (e.g. from MKV to MOV with RAWcooked reversibility data in a sidecar file) or transcode (e.g. FFV1v1 to FFV1v3, keeping the same tracks in the same order) but it is not guaranteed.

Reversibility data can be stored in e.g. a side car file, a container attachment or a chunk/atom/tag.  

## Base design

RAWcooked reversibility data file is a EBML file, with EBML Doctype "rawcooked", EBMLReadVersion 1.  
It contains a sequence of RawCookedSegment, RawCookedTrack or RawCookedBlock elements.  

### Specific types

#### Compressed

Elements with the type "Compressed" have the following structure:  
- 4 Big Endiand encoded bytes indicating the size of the uncompressed content. Is 0 if content is not compressed.  
- Size of element minus 4 bytes of compressed content (if size of the uncompressed content is not 0) or raw content (if size of the uncompressed content is 0).  

### Generic reconstructed file related elements

Several elements may be found in "RawCookedTrack" or "RawCookedBlock".  

#### FileName

File name of the resulting file.  

This element is a marker, indicating that any element after this element and before the next FileName element are related to this file.  

#### DataSize

Size of the data in the file. The total file size is BeforeData content size + DataSize + AfterData content size.  

If not present, total file size is considered as unknown.  
DataSize must be present for all corresponding FileName element in case there are several FileName elements. This is useful for knowing when the tool need to switch to the next file during demux.

Type: unsigned integer

#### BeforeData

Metadata of the the resulting file before the actual content.  

If not present, BeforeData content size is considered as 0.  

Type: Compressed

#### AfterData

Metadata of the the resulting file after the actual content.

If not present, AfterData content size is considered as 0.  

Type: Compressed

#### FileMD5

#### FileSHA1

#### FileSHA256

### RawCookedSegment

A "RawCookedSegment" element contains information about the whole resulting file.  
It may contain information about the RAWcooked reversibility data writing library (name and version).  

#### LibraryName

#### LibraryVersion

### RawCookedTrack

A "RawCookedTrack" element contains information about a track.  
Information in this element can be used by all "RawCookedBlock" element found after this track.  

#### MaskBaseFileName

Mask used by any RawCookedBlock after this element and before the next RawCookedTrack element.  

Any byte not stored is considered as 0.  

Type: Compressed

#### MaskBaseBeforeData

Mask used by any RawCookedBlock after this element and before the next RawCookedTrack element.  

Any byte not stored is considered as 0.  

Type: Compressed

#### MaskBaseAfterData

Mask used by any RawCookedBlock after this element and before the next RawCookedTrack element.  

Any byte not stored is considered as 0.  

Type: Compressed

### RawCookedBlock

A "RawCookedBlock" element contains information about a block (a.k.a. a frame).  

#### MaskAdditionBeforeData

This element has the same meaning as "BeforeData" element before its content is summed byte per byte to the "MaskBaseAfterData" element content.

Type: Compressed

#### MaskAdditionAfterData

This element has the same meaning as "BeforeData" element after its content is summed byte per byte to the "MaskBaseAfterData" element content.

Type: Compressed

## Storage of sidecar data

As a general rule, we try to fill some information at a place able to receive some description.  
The description should contain the text "rawcooked_reversibility_data" or any variant (any letter as uppercase, a space instead of an underscore). The preferred form, when there is no constraint about any legacy systems, is "RAWcooked reversibility data".  

### In a standalone file

For a file name named "FileName.Ext", RAWcooked reversibility data should be in a sidecar file named "FileName.Ext.rawcooked_reversibility_data".  
The file name does not matter except for automatic check of the presence of RAWcooked reversibility data, and the RAWcooked reversibility data file name can be provided to the demux tool if automatic detection does not work.  

### As container attachment

RAWcooked reversibility data can be in a container attachment. In that case, attachment description should be "RAWcooked reversibility data".  
In the case a file name is required for the attachment, it should be "rawcooked_reversibility_data".  

### As atom/chunk/element defined by a UUID

RAWcooked reversibility data can be in a container atom/chunk/element (or whatever is the name of a sub-element inside another element) defined by a UUID. In that case, UUID should be "(TODO)".  

### As atom/chunk/element defined by a 4CC

RAWcooked reversibility data can be in a container atom/chunk/element (or whatever is the name of a sub-element inside another element) defined by a 4CC. In that case, UUID should be "RAWc".  
