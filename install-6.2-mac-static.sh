#!/bin/bash
set -eu


# Constants
#
DAK_ROOT=${ISSM_DIR}/externalpackages/dakota
VER="6.2"

## Environment
#

# Find libgfortran and libgcc so we do not have to hardcode them.
#
# Should retrieve a copy of gfortran that is compiled from source before 
# returning one that is installed via package manager.
#
# TODO:
# - Test if -static-libgfortran flag will avoid all of this.
# - Otherwise, refactor this to work with other gfortran installations.
#
LIBGFORTRAN=$(mdfind -onlyin /usr -name libgfortran | grep -n libgfortran.a | grep -v i386 | sed "s/[0-9]*://g" | head -1)
LIBGFORTRAN_ROOT=${LIBGFORTRAN%/*}
LIBGCC=$(mdfind -onlyin ${LIBGFORTRAN_ROOT} -name libgcc | grep -n libgcc.a | grep -v i386 | sed "s/[0-9]*://g" | head -1)

export BLAS_LIBS="-L${ISSM_DIR}/externalpackages/petsc/install/lib -lfblas ${LIBGFORTRAN_ROOT}/libgfortran.a ${LIBGFORTRAN_ROOT}/libquadmath.a ${LIBGCC}" # Need to export BLAS_LIBS *and* pass it as an option to CMake to ensure that external packages also find it
export BOOST_ROOT=${ISSM_DIR}/externalpackages/boost/install
export DAK_BUILD=${DAK_ROOT}/build
export DAK_INSTALL=${DAK_ROOT}/install
export DAK_SRC=${DAK_ROOT}/src
export GSL_HOME=${ISSM_DIR}/externalpackages/gsl/install
export LAPACK_LIBS="-L${ISSM_DIR}/externalpackages/petsc/install/lib -lflapack ${LIBGFORTRAN_ROOT}/libgfortran.a ${LIBGFORTRAN_ROOT}/libquadmath.a ${LIBGCC}" # Need to export LAPACK_LIBS *and* pass it as an option to CMake to ensure that external packages also find it

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

# Uncomment to copy the following customized source files if using C99 or later 
# standard. If uncommented, adding -Wno-error=implicit-function-declaration 
# option to CFLAGS is not needed.
# cp configs/${VER}/mac/static/packages/DDACE/include/xtndispatch.h ${DAK_SRC}/packages/DDACE/include
# cp configs/${VER}/mac/static/packages/DDACE/src/Bose/Boselink.c ${DAK_SRC}/packages/DDACE/src/Bose
# cp configs/${VER}/mac/static/packages/DDACE/src/Bose/construct.c ${DAK_SRC}/packages/DDACE/src/Bose
# cp configs/${VER}/mac/static/packages/DDACE/src/Bose/galois.c ${DAK_SRC}/packages/DDACE/src/Bose
# cp configs/${VER}/mac/static/packages/DDACE/src/Bose/gfields.c ${DAK_SRC}/packages/DDACE/src/Bose
# cp configs/${VER}/mac/static/packages/DDACE/src/Bose/oa.c ${DAK_SRC}/packages/DDACE/src/Bose
# cp configs/${VER}/mac/static/packages/nidr/nidr.c ${DAK_SRC}/packages/nidr
# cp configs/${VER}/mac/static/packages/nidr/nidr-scanner.c ${DAK_SRC}/packages/nidr

# Configure
cd ${DAK_BUILD}
cmake \
	-DBUILD_SHARED_LIBS=OFF \
	-DBUILD_STATIC_LIBS=ON \
	-DCMAKE_C_COMPILER=${MPI_HOME}/bin/mpicc \
	-DCMAKE_CXX_COMPILER=${MPI_HOME}/bin/mpicxx \
	-DCMAKE_Fortran_COMPILER=${MPI_HOME}/bin/mpif77 \
	-DCMAKE_C_FLAGS="-fPIC -Wno-error=implicit-function-declaration" \
	-DCMAKE_CXX_FLAGS="-fPIC -fdelayed-template-parsing" \
	-DCMAKE_Fortran_FLAGS="-fPIC" \
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
