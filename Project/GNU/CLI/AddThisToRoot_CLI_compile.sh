#! /bin/sh

#############################################################################
# Configure
Home=`pwd`

#############################################################################
# Setup for parallel builds
Zen_Make()
{
 if test -e /proc/stat; then
  numprocs=`egrep -c ^cpu[0-9]+ /proc/stat || :`
  if [ "$numprocs" = "0" ]; then
   numprocs=1
  fi
  make -s -j$numprocs
 else
  make
 fi
}

#############################################################################
# RAWCooked
if test -e Project/GNU/CLI/configure; then
 cd Project/GNU/CLI/
 test -e Makefile && rm Makefile
 chmod u+x configure
 ./configure --enable-staticlibs $*
 if test -e Makefile; then
  make clean
  Zen_Make
  if test -e rawcooked; then
   echo RAWCooked compiled
  else
   echo Problem while compiling RAWCooked
   exit
  fi
 else
  echo Problem while configuring RAWCooked
  exit
 fi
else
 echo RAWCooked directory is not found
 exit
fi
cd $Home

#############################################################################
# Going home
cd $Home
echo "RAWCooked (CLI) executable is in Project/GNU/CLI"
echo "For installing, cd Project/GNU/CLI && make install"
