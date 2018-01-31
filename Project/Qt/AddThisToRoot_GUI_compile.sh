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
# RAWcooked
if test -e Project/Qt/rawcooked-gui.pro; then
 pushd Project/Qt
 test -e Makefile && rm Makefile
 ./prepare
 if test -e Makefile; then
  make clean
  Zen_Make
  if test -e rawcooked-gui || test -e RAWcooked.app || test -e "RAWcooked.app"; then
   echo RAWcooked compiled
  else
   echo Problem while compiling RAWcooked
   exit
  fi
 else
  echo Problem while configuring RAWcooked
  exit
 fi
else
 echo RAWcooked directory is not found
 exit
fi
cd $Home

#############################################################################
# Going home
cd $Home
echo "RAWcooked (GUI) executable is in Project/Qt"
