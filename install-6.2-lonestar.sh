#!/bin/bash
set -eu

#Some cleanup
rm -rf Dakota
rm -rf src 
rm -rf build 
rm -rf install 
mkdir src build install 

#Download from ISSM server
$ISSM_DIR/scripts/DownloadExternalPackage.sh 'https://issm.ess.uci.edu/files/externalpackages/dakota-6.2-public.src.tar.gz' 'dakota-6.2-public-src.tar.gz'

#Untar 
tar -zxvf dakota-6.2-public-src.tar.gz

#Move Dakota to src directory
mv dakota-6.2.0.src/* src
rm -rf dakota-6.2.0.src

#Set up Dakota cmake variables and config
export DAK_SRC=$ISSM_DIR/externalpackages/dakota/src
export DAK_BUILD=$ISSM_DIR/externalpackages/dakota/build
export MPIHOME=/opt/cray/pe/mpt/7.7.3/gni/mpich-intel/16.0/

cp $DAK_SRC/cmake/BuildDakotaTemplate.cmake $DAK_SRC/cmake/BuildDakotaCustom.cmake
patch $DAK_SRC/cmake/BuildDakotaCustom.cmake configs/6.2/BuildDakotaCustom.cmake.patch
patch $DAK_SRC/cmake/DakotaDev.cmake configs/6.2/DakotaDev.cmake.patch
patch $DAK_SRC/CMakeLists.txt configs/6.2/CMakeLists.txt.lonestar.patch

#Apply patches
patch src/src/NonDSampling.cpp configs/6.2/NonDSampling.cpp.patch
patch src/src/NonDLocalReliability.cpp configs/6.2/NonDLocalReliability.cpp.patch
patch src/packages/pecos/src/pecos_global_defs.hpp configs/6.2/pecos_global_defs.hpp.patch
patch src/packages/surfpack/src/surfaces/nkm/NKM_KrigingModel.cpp configs/6.2/NKM_KrigingModel.patch
patch src/packages/DDACE/src/Analyzer/MainEffectsExcelOutput.cpp configs/6.2/MainEffectsExcelOutput.patch
patch src/src/DakotaInterface.cpp configs/6.2/DakotaInterface.patch

#Configure dakota
cd $DAK_BUILD

cmake -D CMAKE_C_COMPILER=mpicc \
	   -D CMAKE_CXX_COMPILER=mpicxx \
	   -D CMAKE_Fortran_COMPILER=mpif77 \
		-D MPIEXEC_EXECUTABLE=/opt/apps/tacc/bin/ibrun \
		-DHAVE_ACRO=off \
		-DHAVE_JEGA=off \
		-C $DAK_SRC/cmake/BuildDakotaCustom.cmake \
		-C $DAK_SRC/cmake/DakotaDev.cmake \
		$DAK_SRC
cd ..

#Compile and install dakota
cd $DAK_BUILD
if [ $# -eq 0 ];
then
	make
	make install
else
	make -j $1
	make -j $1 install
fi
cd ..
