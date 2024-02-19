# RAWcooked Case Study  

## BFI National Archive  
### Joanna White, Knowledge & Collections Developer

At the [BFI National Archive](https://www.bfi.org.uk/bfi-national-archive) we have been encoding DPX sequences to FFV1 Matroska since late 2019. In that time our RAWcooked workflow has developed and evolved with DPX resolutions and flavours and changes in project priorities.  Today we have a fairly hands-off automated workflow which handles 2K, 4K, RGB, Luma, DPX and Tiff DPX.  This workflow is built on some of the flags developed by the Media Area. This includes the '--all' flag, originally an idea suggested by [NYPL](https://www.nypl.org/), more details in the [Encoding]() section below.  

This case study is broken into the stages our encoding workflow follows and the Media Area tools we use to achieve our goals.  We have four distinct stages:  
* Image sequence assessment
* Encoding the image sequence
* Encoding log assessments
* FFV1 MKV validation

## Image sequence assessment  






## Encoding the image sequence  

```
rawcooked -y --all --no-accept-gaps -s 5281680 <path/sequence_name/> -o <path/sequence_name.mkv> &>> <path/sequence_name.mkv.txt>
```
