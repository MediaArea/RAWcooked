dnl #########################################################################
dnl ### Init
dnl #########################################################################

dnl -------------------------------------------------------------------------
dnl Name and version
dnl
AC_INIT([rawcooked], [0])

dnl -------------------------------------------------------------------------
dnl Test if we are at the good place
dnl
AC_CONFIG_SRCDIR([../../../Source/CLI/Main.cpp])

dnl -------------------------------------------------------------------------
dnl sets build, host, target variables and the same with _alias
dnl
AC_CANONICAL_BUILD

dnl -------------------------------------------------------------------------
dnl Automake
dnl
AM_INIT_AUTOMAKE

dnl -------------------------------------------------------------------------
dnl Autotools init
dnl
CFLAGS="$CFLAGS"
CXXFLAGS="$CXXFLAGS"
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL

dnl #########################################################################
dnl ### Options
dnl #########################################################################

dnl -------------------------------------------------------------------------
dnl Arguments - Enable
dnl
AC_ARG_ENABLE(debug,            AS_HELP_STRING([--enable-debug],            [Enable full debug]),                         , enable_debug=no)
AC_ARG_ENABLE(gprof,            AS_HELP_STRING([--enable-gprof],            [Enable gprof log]),                          , enable_gprof=no)
AC_ARG_ENABLE(universal_binary, AS_HELP_STRING([--enable-universal_binary], [Create Mac x86_64/arm64 Universal binary]),  , enable_universal_binary=no)
AC_ARG_ENABLE(arch-ppc,         AS_HELP_STRING([--with-arch-ppc],           [Create Mac PPC Universal binary]),           , enable_arch_ppc=no)
AC_ARG_ENABLE(arch-i386,        AS_HELP_STRING([--with-arch-i386],          [Create Mac i386 Universal binary]),          , enable_arch_i386=no)
AC_ARG_ENABLE(arch-x86_64,      AS_HELP_STRING([--enable-arch-x86_64],      [Create Mac x86_64 architecture]),            , enable_arch_x86_64=no)
AC_ARG_ENABLE(arch-arm64,       AS_HELP_STRING([--enable-arch-arm64],       [Create Mac arm64 architecture]),             , enable_arch_arm64=no)

dnl -------------------------------------------------------------------------
dnl Arguments - With
dnl
AC_ARG_WITH(macosx-sdk,         AS_HELP_STRING([--with-macosx-sdk],         [Force the Mac SDK]),                         , with_macosx_sdk=no)
AC_ARG_WITH(macosx-version-min, AS_HELP_STRING([--with-macosx-version-min], [Force the Mac Version]),                     , with_macosx_version_min=no)
AC_ARG_WITH(ffmpeg,             AS_HELP_STRING([--with-ffmpeg],             [Enable FFmpeg libavcodec FFV1 decoder (--decoder ffmpeg), =dynamic for loading FFmpeg libraries at run time (default on macOS), =DIR for loading at run time with the headers of an FFmpeg package in DIR/include]), , with_ffmpeg=no)

dnl #########################################################################
dnl ### Specific flags
dnl #########################################################################

dnl -------------------------------------------------------------------------
dnl Pthreads
dnl
CXXFLAGS="$CXXFLAGS -pthread"
LDFLAGS="$LDFLAGS -lpthread -pthread"

dnl #########################################################################
dnl ### C/C++ compiler options used to compile
dnl #########################################################################

dnl -------------------------------------------------------------------------
dnl GNU compiler
dnl
if test "$GXX" = yes ; then
	if test "$enable_debug" = "yes"; then
		CFLAGS="$CFLAGS -O0 -g -Wall -Wundef"
		CXXFLAGS="$CXXFLAGS -O0 -g -Wall -W -Wundef -Wcast-qual -Wno-ctor-dtor-privacy"
	else
		CFLAGS="$CFLAGS -O2"
		CXXFLAGS="$CXXFLAGS -O2"
	fi
	if test "$enable_gprof" = "yes"; then
		CFLAGS="$CFLAGS -pg -g"
		CXXFLAGS="$CXXFLAGS -pg -g"
	fi
fi

dnl #########################################################################
dnl ### Compiler specific
dnl #########################################################################

dnl -------------------------------------------------------------------------
case "${build}" in
  powerpc-montavista* )
    CFLAGS="$CFLAGS -D__MONTAVISTA__"
    CXXFLAGS="$CXXFLAGS -D__MONTAVISTA__"
  ;;
esac

dnl -------------------------------------------------------------------------
dnl Mac Architecture and SDK
dnl
if test "$enable_universal_binary" = "yes"; then
	enable_arch_x86_64=yes
	enable_arch_arm64=yes
fi
if test "$enable_arch_ppc" = "yes"; then
	CXXFLAGS="$CXXFLAGS -arch ppc"
	CFLAGS="$CFLAGS -arch ppc"
	LDFLAGS="$LDFLAGS -arch ppc"
fi
if test "$enable_arch_i386" = "yes"; then
	CXXFLAGS="$CXXFLAGS -arch i386"
	CFLAGS="$CFLAGS -arch i386"
	LDFLAGS="$LDFLAGS -arch i386"
fi
if test "$enable_arch_x86_64" = "yes"; then
	CXXFLAGS="$CXXFLAGS -arch x86_64"
	CFLAGS="$CFLAGS -arch x86_64"
	LDFLAGS="$LDFLAGS -arch x86_64"
fi
if test "$enable_arch_arm64" = "yes"; then
	CXXFLAGS="$CXXFLAGS -arch arm64"
	CFLAGS="$CFLAGS -arch arm64"
	LDFLAGS="$LDFLAGS -arch arm64"
fi
if test "x$with_macosx_sdk" != xno ; then
	CXXFLAGS="-isysroot $with_macosx_sdk $CXXFLAGS"
	CFLAGS="-isysroot $with_macosx_sdk $CFLAGS"
	LDFLAGS="-isysroot $with_macosx_sdk $LDFLAGS"
fi
if test "x$with_macosx_version_min" != xno ; then
	CXXFLAGS="-mmacosx-version-min=$with_macosx_version_min $CXXFLAGS"
	CFLAGS="-mmacosx-version-min=$with_macosx_version_min $CFLAGS"
	LDFLAGS="-mmacosx-version-min=$with_macosx_version_min $LDFLAGS"
fi

dnl -------------------------------------------------------------------------
dnl Intel JCC erratum (Skylake and derived CPUs): jumps are aligned so they do not cross or end on 32-byte boundaries,
dnl else the decoding is slower (measured on i7-7800X: 7%), if the assembler supports it (x86)
dnl
SAVE_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS -Wa,-mbranches-within-32B-boundaries"
AC_LANG_PUSH([C++])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [])], [CFLAGS="$CFLAGS -Wa,-mbranches-within-32B-boundaries"], [CXXFLAGS="$SAVE_CXXFLAGS"])
AC_LANG_POP([C++])

dnl #########################################################################
dnl ### libFLAC
dnl #########################################################################

AC_SEARCH_LIBS([lround],[m], [AC_DEFINE(HAVE_LROUND,1,lround support)])
AC_FUNC_FSEEKO
AC_CHECK_HEADERS([stdint.h inttypes.h byteswap.h sys/param.h sys/ioctl.h termios.h x86intrin.h cpuid.h])
CFLAGS=" -DFLAC__NO_DLL -DFLAC__HAS_OGG=0 -DFLAC__NO_ASM $CFLAGS"

dnl #########################################################################
dnl ### FFmpeg (optional)
dnl #########################################################################

if test "$with_ffmpeg" != "no"; then
	if test "$with_ffmpeg" != "yes" && test "$with_ffmpeg" != "dynamic"; then
		dnl Directory of an FFmpeg package (include and lib directories), libraries are loaded at run time
		if ! test -f "$with_ffmpeg/include/libavcodec/avcodec.h"; then
			AC_MSG_ERROR([$with_ffmpeg/include/libavcodec/avcodec.h not found])
		fi
		CXXFLAGS="$CXXFLAGS -DRAWCOOKED_FFMPEG -DRAWCOOKED_FFMPEG_DYNAMIC -I$with_ffmpeg/include"
		AC_SEARCH_LIBS([dlopen], [dl])
	elif pkg-config --exists libavcodec-ma libavutil-ma || pkg-config --exists libavcodec libavutil; then
		dnl ffmpeg-ma package (FFmpeg build of MediaArea, with -ma suffix) first, as at run time
		if pkg-config --exists libavcodec-ma libavutil-ma; then
			ffmpeg_pkgs="libavcodec-ma libavutil-ma"
		else
			ffmpeg_pkgs="libavcodec libavutil"
		fi
		CXXFLAGS="$CXXFLAGS -DRAWCOOKED_FFMPEG `pkg-config --cflags $ffmpeg_pkgs`"
		case "${build}" in
		  *darwin* )
			with_ffmpeg=dynamic
		  ;;
		esac
		if test "$with_ffmpeg" = "dynamic"; then
			dnl Libraries are loaded at run time, so they are optional
			CXXFLAGS="$CXXFLAGS -DRAWCOOKED_FFMPEG_DYNAMIC"
			AC_SEARCH_LIBS([dlopen], [dl])
		else
			LIBS="$LIBS `pkg-config --libs $ffmpeg_pkgs`"
		fi
	else
		AC_MSG_ERROR([libavcodec/libavutil (or libavcodec-ma/libavutil-ma) not found by pkg-config])
	fi
fi

dnl #########################################################################
dnl ### Output
dnl #########################################################################

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

dnl #########################################################################
dnl ### Report how we have been configured
dnl #########################################################################

echo ""
echo "Configured RAWcooked for '$build'"
echo ""
echo "  CXXFLAGS: $CXXFLAGS"
echo "  LIBS: $LIBS"
