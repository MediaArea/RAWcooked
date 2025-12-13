AUTOMAKE_OPTIONS = foreign subdir-objects

bin_PROGRAMS = rawcooked
rawcooked_SOURCES = \
    ../../../Source/CLI/Global.cpp \
    ../../../Source/CLI/Help.cpp \
    ../../../Source/CLI/Input.cpp \
    ../../../Source/CLI/Main.cpp \
    ../../../Source/CLI/Output.cpp \
    ../../../Source/Lib/CoDec/FFV1/Coder/FFV1_Coder_GolombRice.cpp \
    ../../../Source/Lib/CoDec/FFV1/Coder/FFV1_Coder_RangeCoder.cpp \
    ../../../Source/Lib/CoDec/FFV1/FFV1_Frame.cpp \
    ../../../Source/Lib/CoDec/FFV1/FFV1_Parameters.cpp \
    ../../../Source/Lib/CoDec/FFV1/FFV1_RangeCoder.cpp \
    ../../../Source/Lib/CoDec/FFV1/FFV1_Slice.cpp \
    ../../../Source/Lib/CoDec/Wrapper.cpp \
    ../../../Source/Lib/Common/Common.cpp \
    ../../../Source/Lib/Compressed/Matroska/Matroska.cpp \
    ../../../Source/Lib/Compressed/RAWcooked/IntermediateWrite.cpp \
    ../../../Source/Lib/Compressed/RAWcooked/RAWcooked.cpp \
    ../../../Source/Lib/Compressed/RAWcooked/Reversibility.cpp \
    ../../../Source/Lib/Compressed/RAWcooked/Track.cpp \
    ../../../Source/Lib/License/License.cpp \
    ../../../Source/Lib/ThirdParty/flac/src/libFLAC/bitmath.c \
    ../../../Source/Lib/ThirdParty/flac/src/libFLAC/bitreader.c \
    ../../../Source/Lib/ThirdParty/flac/src/libFLAC/cpu.c \
    ../../../Source/Lib/ThirdParty/flac/src/libFLAC/crc.c \
    ../../../Source/Lib/ThirdParty/flac/src/libFLAC/fixed.c \
    ../../../Source/Lib/ThirdParty/flac/src/libFLAC/float.c \
    ../../../Source/Lib/ThirdParty/flac/src/libFLAC/format.c \
    ../../../Source/Lib/ThirdParty/flac/src/libFLAC/lpc.c \
    ../../../Source/Lib/ThirdParty/flac/src/libFLAC/md5.c \
    ../../../Source/Lib/ThirdParty/flac/src/libFLAC/memory.c \
    ../../../Source/Lib/ThirdParty/flac/src/libFLAC/metadata_iterators.c \
    ../../../Source/Lib/ThirdParty/flac/src/libFLAC/metadata_object.c \
    ../../../Source/Lib/ThirdParty/flac/src/libFLAC/stream_decoder.c \
    ../../../Source/Lib/ThirdParty/flac/src/libFLAC/window.c \
    ../../../Source/Lib/ThirdParty/md5/md5.c \
    ../../../Source/Lib/ThirdParty/zlib/adler32.c \
    ../../../Source/Lib/ThirdParty/zlib/compress.c \
    ../../../Source/Lib/ThirdParty/zlib/crc32.c \
    ../../../Source/Lib/ThirdParty/zlib/deflate.c \
    ../../../Source/Lib/ThirdParty/zlib/infback.c \
    ../../../Source/Lib/ThirdParty/zlib/inffast.c \
    ../../../Source/Lib/ThirdParty/zlib/inflate.c \
    ../../../Source/Lib/ThirdParty/zlib/inftrees.c \
    ../../../Source/Lib/ThirdParty/zlib/trees.c \
    ../../../Source/Lib/ThirdParty/zlib/uncompr.c \
    ../../../Source/Lib/ThirdParty/zlib/zutil.c \
    ../../../Source/Lib/Transform/Transform.cpp \
    ../../../Source/Lib/Uncompressed/AIFF/AIFF.cpp \
    ../../../Source/Lib/Uncompressed/AVI/AVI.cpp \
    ../../../Source/Lib/Uncompressed/DPX/DPX.cpp \
    ../../../Source/Lib/Uncompressed/EXR/EXR.cpp \
    ../../../Source/Lib/Uncompressed/HashSum/HashSum.cpp \
    ../../../Source/Lib/Uncompressed/TIFF/TIFF.cpp \
    ../../../Source/Lib/Uncompressed/WAV/WAV.cpp \
    ../../../Source/Lib/Utils/CRC32/ZenCRC32.cpp \
    ../../../Source/Lib/Utils/Errors/Errors.cpp \
    ../../../Source/Lib/Utils/FileIO/FileChecker.cpp \
    ../../../Source/Lib/Utils/FileIO/FileIO.cpp \
    ../../../Source/Lib/Utils/FileIO/FileWriter.cpp \
    ../../../Source/Lib/Utils/FileIO/Input_Base.cpp \
    ../../../Source/Lib/Utils/RawFrame/RawFrame.cpp

AM_CPPFLAGS = -I../../../Source \
              -I../../../Source/Lib/ThirdParty/flac/include \
              -I../../../Source/Lib/ThirdParty/flac/src/libFLAC/include \
              -I../../../Source/Lib/ThirdParty/md5 \
              -I../../../Source/Lib/ThirdParty/thread-pool/include\
              -I../../../Source/Lib/ThirdParty/zlib

AM_CXXFLAGS = -std=c++0x

man1_MANS = ../../../Source/CLI/rawcooked.1

AM_TESTS_FD_REDIRECT = 9>&2

TESTS = test/test1.sh test/test1b.sh test/test2.sh test/test3.sh test/vulkan.sh test/avi.sh test/pcm.sh test/reversibilityfile.sh test/paddingbits.sh test/check.sh test/legacy.sh test/multiple.sh test/valgrind.sh test/overwrite.sh test/increasingdigitcount.sh test/gaps.sh test/slices.sh test/framerate.sh test/hash.sh test/notfound.sh test/version.sh

TESTING_DIR = test/TestingFiles

clone_testing_files: $(TESTING_DIR)

$(TESTING_DIR):
	git clone --depth=1 https://github.com/MediaArea/RAWCooked-RegressionTestingFiles-Light.git test/TestingFiles
