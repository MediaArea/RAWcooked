AUTOMAKE_OPTIONS = foreign subdir-objects

bin_PROGRAMS = rawcooked
rawcooked_SOURCES = \
    ../../../Source/CLI/Help.cpp \
    ../../../Source/CLI/Main.cpp \
    ../../../Source/Lib/CRC32/CRC32.cpp \
    ../../../Source/Lib/DPX/DPX.cpp \
    ../../../Source/Lib/FFV1/Coder/FFV1_Coder_GolombRice.cpp \
    ../../../Source/Lib/FFV1/Coder/FFV1_Coder_RangeCoder.cpp \
    ../../../Source/Lib/FFV1/FFV1_Frame.cpp \
    ../../../Source/Lib/FFV1/FFV1_Parameters.cpp \
    ../../../Source/Lib/FFV1/FFV1_RangeCoder.cpp \
    ../../../Source/Lib/FFV1/FFV1_Slice.cpp \
    ../../../Source/Lib/FFV1/Transform/FFV1_Transform_JPEG2000RCT.cpp \
    ../../../Source/Lib/FrameHash/FrameHash_MD5.cpp \
    ../../../Source/Lib/Matroska/Matroska_Common.cpp \
    ../../../Source/Lib/RAWCooked/RAWCooked.cpp \
    ../../../Source/Lib/RawFrame/RawFrame.cpp \
    ../../../Source/Lib/ThirdParty/md5/md5.c

AM_CPPFLAGS = -std=c++0x -I../../../Source -I../../../Source/Lib/ThirdParty/thread-pool/include

#AM_TESTS_FD_REDIRECT = 9>&2
#TESTS = test/test.sh
#TESTING_DIR = test/TestingFiles
