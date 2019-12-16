# Structure of RAWcooked reversibility data 

To guarantee the reversibility to the original files you will require some sidecar data. RAWcooked’s reversibility data is built for each specific file, with specific options. In most cases RAWcooked reversibility data is still usable after a remux (for example from Matroska .mkv to QuickTime .mov with RAWcooked reversibility data in a sidecar file) or after a transcode (such as FFV1 version 1 to FFV1 version 3, keeping the same tracks in the same order). This is not guaranteed for all cases.

Reversibility data can be stored in a sidecar file, a container attachment or a chunk/atom/tag.

## Base design

The RAWcooked reversibility data file is an EBML file, with EBML Doctype "rawcooked", EBMLReadVersion 1.  
It contains a sequence of RawCookedSegment, RawCookedTrack or RawCookedBlock elements.  

### Specific types

#### Compressed

Elements with the type "Compressed" have the following structure:  
- 4 big-endian encoded bytes indicating the size of the uncompressed content. Is 0 if content is not compressed.  
- Size of element minus 4 bytes of compressed content if size of the uncompressed content is not 0, or raw content if size of the uncompressed content is 0.  

### Generic reconstructed file related elements

Several elements may be found in "RawCookedTrack" or "RawCookedBlock".  

#### FileName

File name of the resulting file.  

This element is a marker indicating that any element after this element, and before the next FileName element, are related to this file. 

#### DataSize

Indicates the size of the data in the file. Total file size includes BeforeData content size plus DataSize plus AfterData content size.  If DataSize is not present total file size is considered as unknown.  

DataSize must be present for all corresponding FileName elements in case there are several FileName elements. This is useful to know when the tool needs to switch to the next file during demux.

Type: Unsigned integer.

#### BeforeData

Metadata of the resulting file before the actual content.  

If not present, BeforeData content size is considered as 0.  

Type: Compressed.

#### AfterData

Metadata of the resulting file after the actual content.

If not present, AfterData content size is considered as 0.  

Type: Compressed.

#### FileMD5

#### FileSHA1

#### FileSHA256

### RawCookedSegment

A "RawCookedSegment" element contains information about the whole resulting file.  
It may contain information about the RAWcooked reversibility data writing library, such as name and version.  

#### LibraryName

#### LibraryVersion

#### PathSeparator

Indicate the path separator of file names.  
If not present, considered as "/".  

### RawCookedTrack

A "RawCookedTrack" element contains information about a track.  
Information in this element can be used by all "RawCookedBlock" elements found after this track.  

#### MaskBaseFileName

Mask used by any RawCookedBlock after this element and before the next RawCookedTrack element.  

Any byte not stored is considered as 0.  

Type: Compressed.

#### MaskBaseBeforeData

Mask used by any RawCookedBlock after this element and before the next RawCookedTrack element.  

Any byte not stored is considered as 0.  

Type: Compressed.

#### MaskBaseAfterData

Mask used by any RawCookedBlock after this element and before the next RawCookedTrack element.  

Any byte not stored is considered as 0.  

Type: Compressed.

### RawCookedBlock

A RawCookedBlock element contains information about a block (also known as a frame).  

#### MaskAdditionBeforeData

This element has the same meaning as BeforeData element, before its content is added byte per byte to the MaskBaseAfterData element content.

Type: Compressed.

#### MaskAdditionAfterData

This element has the same meaning as BeforeData element, after its content is added byte per byte to the MaskBaseAfterData element content.

Type: Compressed.

## Storage of sidecar data

As a general rule, we try to populate sidecar data in a location suited to receive a description.  
The description should contain the text "rawcooked_reversibility_data" or a variation of this (letters can be uppercase or include a space instead of underscore). The preferred form, when there is no constraints from legacy systems, is "RAWcooked reversibility data".

### In a standalone file

Using the name "FileName.Ext" as an example, RAWcooked reversibility data should be in a sidecar file named "FileName.Ext.rawcooked_reversibility_data".  
The file name doesn't influence the automatic check for the presence of RAWcooked reversibility data, and the RAWcooked reversibility data file name can be provided to the demux tool if automatic detection does not work.  

### As container attachment

RAWcooked reversibility data can be in a container attachment. If this is the case the attachment description should be "RAWcooked reversibility data".  
If a file name is required for the attachment it should be "rawcooked_reversibility_data".  

### As atom/chunk/element defined by a UUID

RAWcooked reversibility data can be in a container atom/chunk/element (or whatever the name of the sub-element is inside another element) defined by a UUID. If this is the case the UUID should be "(TODO)".  

### As atom/chunk/element defined by a 4CC

RAWcooked reversibility data can be in a container atom/chunk/element (or whatever the name of the sub-element is inside another element) defined by a 4CC. If this is the case the UUID should be "RAWc".  
