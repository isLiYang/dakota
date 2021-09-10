#!/bin/bash
set -eu


# Constants
#
DAK_ROOT=${ISSM_DIR}/externalpackages/dakota
VER="6.2"

## Environment
#

# Find libgfortran so that we do not have to hardcode it.
#
# Should retrieve a copy of gfortran that is compiled from source before 
# returning one that is installed via package manager.
#
LIBGFORTRAN=$(mdfind -onlyin /usr -name libgfortran | grep -n libgfortran.a | grep -v i386 | sed "s/[0-9]*://g" | head -1)
LIBGFORTRAN_ROOT=${LIBGFORTRAN%/*}

export BLAS_LIBS="-L${ISSM_DIR}/externalpackages/petsc/install/lib -lfblas -L${LIBGFORTRAN_ROOT} -lgfortran" # Need to export BLAS_LIBS *and* pass it as an option to CMake to ensure that external packages also find it
export BOOST_ROOT=${ISSM_DIR}/externalpackages/boost/install
export DAK_BUILD=${DAK_ROOT}/build
export DAK_INSTALL=${DAK_ROOT}/install
export DAK_SRC=${DAK_ROOT}/src
export GSL_HOME=${ISSM_DIR}/externalpackages/gsl/install
export LAPACK_LIBS="-L${ISSM_DIR}/externalpackages/petsc/install/lib -lflapack -L${LIBGFORTRAN_ROOT} -lgfortran" # Need to export LAPACK_LIBS *and* pass it as an option to CMake to ensure that external packages also find it

# Cleanup
rm -rf build install src
mkdir build install src

# Download source
${ISSM_DIR}/scripts/DownloadExternalPackage.sh "https://issm.ess.uci.edu/files/externalpackages/dakota-${VER}-public.src.tar.gz" "dakota-${VER}-public-src.tar.gz"

# Unpack source
tar -zxvf dakota-${VER}-public-src.tar.gz

# Move source to 'src' directory
mv dakota-${VER}.0.src/* src
rm -rf dakota-${VER}.0.src

# Copy customized source and configuration files to 'src' directory
cp configs/${VER}/packages/DDACE/src/Analyzer/MainEffectsExcelOutput.cpp ${DAK_SRC}/packages/DDACE/src/Analyzer
cp configs/${VER}/packages/surfpack/src/surfaces/nkm/NKM_KrigingModel.cpp ${DAK_SRC}/packages/surfpack/src/surfaces/nkm
cp configs/${VER}/src/DakotaInterface.cpp ${DAK_SRC}/src
cp configs/${VER}/src/NonDLocalReliability.cpp ${DAK_SRC}/src
cp configs/${VER}/src/NonDSampling.cpp ${DAK_SRC}/src

# Copy customized source and configuration files specific to Mac to 'src' directory
cp configs/${VER}/mac/cmake/BuildDakotaCustom.cmake ${DAK_SRC}/cmake
cp configs/${VER}/mac/cmake/DakotaDev.cmake ${DAK_SRC}/cmake
cp configs/${VER}/mac/cmake/InstallDarwinDylibs.cmake ${DAK_SRC}/cmake
cp configs/${VER}/mac/packages/VPISparseGrid/src/sandia_rules.cpp ${DAK_SRC}/packages/VPISparseGrid/src

# Configure
cd ${DAK_BUILD}
cmake \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_STATIC_LIBS=OFF \
	-DCMAKE_C_COMPILER=${MPI_HOME}/bin/mpicc \
	-DCMAKE_CXX_COMPILER=${MPI_HOME}/bin/mpicxx \
	-DCMAKE_Fortran_COMPILER=${MPI_HOME}/bin/mpif77 \
	-DCMAKE_CXX_FLAGS="-fdelayed-template-parsing" \
	-DBoost_NO_BOOST_CMAKE=TRUE \
	-DHAVE_ACRO=OFF \
	-DHAVE_JEGA=OFF \
	-DHAVE_QUESO=ON \
	-DDAKOTA_HAVE_GSL=ON \
	-C${DAK_SRC}/cmake/BuildDakotaCustom.cmake \
	-C${DAK_SRC}/cmake/DakotaDev.cmake \
	${DAK_SRC}

# Compile and install
if [ $# -eq 0 ]; then
	make
	make install
else
	make -j $1
	make -j $1 install
fi

cd ${DAK_INSTALL}

# Comment out definition of HAVE_MPI in Teuchos config header file in order to
# avoid conflict with our definition
sed -i -e "s/#define HAVE_MPI/\/* #define HAVE_MPI *\//g" include/Teuchos_config.h

# Set install_name for all shared libraries
cd ${DAK_INSTALL}/lib
for name in *.dylib; do
	install_name_tool -id ${DAK_INSTALL}/lib/${name} ${name}
done

## Patch install names for certain libraries
#
# TODO: Figure out how to reconfigure source to apply these install names at compile time
#
install_name_tool -change libdakota_src_fortran.dylib ${DAK_INSTALL}/lib/libdakota_src_fortran.dylib libdakota_src.dylib
install_name_tool -change liblhs_mod.dylib ${DAK_INSTALL}/lib/liblhs_mod.dylib liblhs.dylib
install_name_tool -change liblhs_mods.dylib ${DAK_INSTALL}/lib/liblhs_mods.dylib liblhs.dylib
install_name_tool -change liblhs_mod.dylib ${DAK_INSTALL}/lib/liblhs_mod.dylib liblhs_mods.dylib
install_name_tool -change libteuchos.dylib ${DAK_INSTALL}/lib/libteuchos.dylib liboptpp.dylib
install_name_tool -change libdfftpack.dylib ${DAK_INSTALL}/lib/libdfftpack.dylib libpecos.dylib
install_name_tool -change liblhs.dylib ${DAK_INSTALL}/lib/liblhs.dylib libpecos.dylib
install_name_tool -change liblhs_mod.dylib ${DAK_INSTALL}/lib/liblhs_mod.dylib libpecos.dylib
install_name_tool -change liblhs_mods.dylib ${DAK_INSTALL}/lib/liblhs_mods.dylib libpecos.dylib
install_name_tool -change libpecos_src.dylib ${DAK_INSTALL}/lib/libpecos_src.dylib libpecos.dylib
install_name_tool -change libteuchos.dylib ${DAK_INSTALL}/lib/libteuchos.dylib libpecos.dylib
install_name_tool -change libdfftpack.dylib ${DAK_INSTALL}/lib/libdfftpack.dylib libpecos_src.dylib
install_name_tool -change liblhs.dylib ${DAK_INSTALL}/lib/liblhs.dylib libpecos_src.dylib
install_name_tool -change liblhs_mod.dylib ${DAK_INSTALL}/lib/liblhs_mod.dylib libpecos_src.dylib
install_name_tool -change liblhs_mods.dylib ${DAK_INSTALL}/lib/liblhs_mods.dylib libpecos_src.dylib
install_name_tool -change libteuchos.dylib ${DAK_INSTALL}/lib/libteuchos.dylib libpecos_src.dylib
install_name_tool -change libsurfpack_fortran.dylib ${DAK_INSTALL}/lib/libsurfpack_fortran.dylib libsurfpack.dylib
