# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.16

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /usr/share/cmake-3.16/Modules/FortranCInterface

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/liyang/software/issm/trunk/externalpackages/dakota/build/CMakeFiles/FortranCInterface

# Include any dependencies generated for this target.
include CMakeFiles/myfort.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/myfort.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/myfort.dir/flags.make

CMakeFiles/myfort.dir/mysub.f.o: CMakeFiles/myfort.dir/flags.make
CMakeFiles/myfort.dir/mysub.f.o: /usr/share/cmake-3.16/Modules/FortranCInterface/mysub.f
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --progress-dir=/home/liyang/software/issm/trunk/externalpackages/dakota/build/CMakeFiles/FortranCInterface/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building Fortran object CMakeFiles/myfort.dir/mysub.f.o"
	/home/liyang/software/issm/trunk/externalpackages/petsc/install/bin/mpif77 $(Fortran_DEFINES) $(Fortran_INCLUDES) $(Fortran_FLAGS) -c /usr/share/cmake-3.16/Modules/FortranCInterface/mysub.f -o CMakeFiles/myfort.dir/mysub.f.o

CMakeFiles/myfort.dir/mysub.f.i: cmake_force
	@echo "Preprocessing Fortran source to CMakeFiles/myfort.dir/mysub.f.i"
	/home/liyang/software/issm/trunk/externalpackages/petsc/install/bin/mpif77 $(Fortran_DEFINES) $(Fortran_INCLUDES) $(Fortran_FLAGS) -E /usr/share/cmake-3.16/Modules/FortranCInterface/mysub.f > CMakeFiles/myfort.dir/mysub.f.i

CMakeFiles/myfort.dir/mysub.f.s: cmake_force
	@echo "Compiling Fortran source to assembly CMakeFiles/myfort.dir/mysub.f.s"
	/home/liyang/software/issm/trunk/externalpackages/petsc/install/bin/mpif77 $(Fortran_DEFINES) $(Fortran_INCLUDES) $(Fortran_FLAGS) -S /usr/share/cmake-3.16/Modules/FortranCInterface/mysub.f -o CMakeFiles/myfort.dir/mysub.f.s

CMakeFiles/myfort.dir/my_sub.f.o: CMakeFiles/myfort.dir/flags.make
CMakeFiles/myfort.dir/my_sub.f.o: /usr/share/cmake-3.16/Modules/FortranCInterface/my_sub.f
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --progress-dir=/home/liyang/software/issm/trunk/externalpackages/dakota/build/CMakeFiles/FortranCInterface/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building Fortran object CMakeFiles/myfort.dir/my_sub.f.o"
	/home/liyang/software/issm/trunk/externalpackages/petsc/install/bin/mpif77 $(Fortran_DEFINES) $(Fortran_INCLUDES) $(Fortran_FLAGS) -c /usr/share/cmake-3.16/Modules/FortranCInterface/my_sub.f -o CMakeFiles/myfort.dir/my_sub.f.o

CMakeFiles/myfort.dir/my_sub.f.i: cmake_force
	@echo "Preprocessing Fortran source to CMakeFiles/myfort.dir/my_sub.f.i"
	/home/liyang/software/issm/trunk/externalpackages/petsc/install/bin/mpif77 $(Fortran_DEFINES) $(Fortran_INCLUDES) $(Fortran_FLAGS) -E /usr/share/cmake-3.16/Modules/FortranCInterface/my_sub.f > CMakeFiles/myfort.dir/my_sub.f.i

CMakeFiles/myfort.dir/my_sub.f.s: cmake_force
	@echo "Compiling Fortran source to assembly CMakeFiles/myfort.dir/my_sub.f.s"
	/home/liyang/software/issm/trunk/externalpackages/petsc/install/bin/mpif77 $(Fortran_DEFINES) $(Fortran_INCLUDES) $(Fortran_FLAGS) -S /usr/share/cmake-3.16/Modules/FortranCInterface/my_sub.f -o CMakeFiles/myfort.dir/my_sub.f.s

CMakeFiles/myfort.dir/mymodule.f90.o: CMakeFiles/myfort.dir/flags.make
CMakeFiles/myfort.dir/mymodule.f90.o: /usr/share/cmake-3.16/Modules/FortranCInterface/mymodule.f90
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --progress-dir=/home/liyang/software/issm/trunk/externalpackages/dakota/build/CMakeFiles/FortranCInterface/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building Fortran object CMakeFiles/myfort.dir/mymodule.f90.o"
	/home/liyang/software/issm/trunk/externalpackages/petsc/install/bin/mpif77 $(Fortran_DEFINES) $(Fortran_INCLUDES) $(Fortran_FLAGS) -c /usr/share/cmake-3.16/Modules/FortranCInterface/mymodule.f90 -o CMakeFiles/myfort.dir/mymodule.f90.o

CMakeFiles/myfort.dir/mymodule.f90.i: cmake_force
	@echo "Preprocessing Fortran source to CMakeFiles/myfort.dir/mymodule.f90.i"
	/home/liyang/software/issm/trunk/externalpackages/petsc/install/bin/mpif77 $(Fortran_DEFINES) $(Fortran_INCLUDES) $(Fortran_FLAGS) -E /usr/share/cmake-3.16/Modules/FortranCInterface/mymodule.f90 > CMakeFiles/myfort.dir/mymodule.f90.i

CMakeFiles/myfort.dir/mymodule.f90.s: cmake_force
	@echo "Compiling Fortran source to assembly CMakeFiles/myfort.dir/mymodule.f90.s"
	/home/liyang/software/issm/trunk/externalpackages/petsc/install/bin/mpif77 $(Fortran_DEFINES) $(Fortran_INCLUDES) $(Fortran_FLAGS) -S /usr/share/cmake-3.16/Modules/FortranCInterface/mymodule.f90 -o CMakeFiles/myfort.dir/mymodule.f90.s

CMakeFiles/myfort.dir/my_module.f90.o: CMakeFiles/myfort.dir/flags.make
CMakeFiles/myfort.dir/my_module.f90.o: /usr/share/cmake-3.16/Modules/FortranCInterface/my_module.f90
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --progress-dir=/home/liyang/software/issm/trunk/externalpackages/dakota/build/CMakeFiles/FortranCInterface/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Building Fortran object CMakeFiles/myfort.dir/my_module.f90.o"
	/home/liyang/software/issm/trunk/externalpackages/petsc/install/bin/mpif77 $(Fortran_DEFINES) $(Fortran_INCLUDES) $(Fortran_FLAGS) -c /usr/share/cmake-3.16/Modules/FortranCInterface/my_module.f90 -o CMakeFiles/myfort.dir/my_module.f90.o

CMakeFiles/myfort.dir/my_module.f90.i: cmake_force
	@echo "Preprocessing Fortran source to CMakeFiles/myfort.dir/my_module.f90.i"
	/home/liyang/software/issm/trunk/externalpackages/petsc/install/bin/mpif77 $(Fortran_DEFINES) $(Fortran_INCLUDES) $(Fortran_FLAGS) -E /usr/share/cmake-3.16/Modules/FortranCInterface/my_module.f90 > CMakeFiles/myfort.dir/my_module.f90.i

CMakeFiles/myfort.dir/my_module.f90.s: cmake_force
	@echo "Compiling Fortran source to assembly CMakeFiles/myfort.dir/my_module.f90.s"
	/home/liyang/software/issm/trunk/externalpackages/petsc/install/bin/mpif77 $(Fortran_DEFINES) $(Fortran_INCLUDES) $(Fortran_FLAGS) -S /usr/share/cmake-3.16/Modules/FortranCInterface/my_module.f90 -o CMakeFiles/myfort.dir/my_module.f90.s

# Object files for target myfort
myfort_OBJECTS = \
"CMakeFiles/myfort.dir/mysub.f.o" \
"CMakeFiles/myfort.dir/my_sub.f.o" \
"CMakeFiles/myfort.dir/mymodule.f90.o" \
"CMakeFiles/myfort.dir/my_module.f90.o"

# External object files for target myfort
myfort_EXTERNAL_OBJECTS =

libmyfort.a: CMakeFiles/myfort.dir/mysub.f.o
libmyfort.a: CMakeFiles/myfort.dir/my_sub.f.o
libmyfort.a: CMakeFiles/myfort.dir/mymodule.f90.o
libmyfort.a: CMakeFiles/myfort.dir/my_module.f90.o
libmyfort.a: CMakeFiles/myfort.dir/build.make
libmyfort.a: CMakeFiles/myfort.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --progress-dir=/home/liyang/software/issm/trunk/externalpackages/dakota/build/CMakeFiles/FortranCInterface/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Linking Fortran static library libmyfort.a"
	$(CMAKE_COMMAND) -P CMakeFiles/myfort.dir/cmake_clean_target.cmake
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/myfort.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/myfort.dir/build: libmyfort.a

.PHONY : CMakeFiles/myfort.dir/build

CMakeFiles/myfort.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/myfort.dir/cmake_clean.cmake
.PHONY : CMakeFiles/myfort.dir/clean

CMakeFiles/myfort.dir/depend:
	cd /home/liyang/software/issm/trunk/externalpackages/dakota/build/CMakeFiles/FortranCInterface && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /usr/share/cmake-3.16/Modules/FortranCInterface /usr/share/cmake-3.16/Modules/FortranCInterface /home/liyang/software/issm/trunk/externalpackages/dakota/build/CMakeFiles/FortranCInterface /home/liyang/software/issm/trunk/externalpackages/dakota/build/CMakeFiles/FortranCInterface /home/liyang/software/issm/trunk/externalpackages/dakota/build/CMakeFiles/FortranCInterface/CMakeFiles/myfort.dir/DependInfo.cmake
.PHONY : CMakeFiles/myfort.dir/depend

