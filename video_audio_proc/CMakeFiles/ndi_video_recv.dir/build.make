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
CMAKE_SOURCE_DIR = /home/rui/avatar_ws/src/avatar_interaction/video_audio_proc

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/rui/avatar_ws/src/avatar_interaction/video_audio_proc

# Include any dependencies generated for this target.
include CMakeFiles/ndi_video_recv.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/ndi_video_recv.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/ndi_video_recv.dir/flags.make

CMakeFiles/ndi_video_recv.dir/src/ndi_video_recv.cpp.o: CMakeFiles/ndi_video_recv.dir/flags.make
CMakeFiles/ndi_video_recv.dir/src/ndi_video_recv.cpp.o: src/ndi_video_recv.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/rui/avatar_ws/src/avatar_interaction/video_audio_proc/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/ndi_video_recv.dir/src/ndi_video_recv.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/ndi_video_recv.dir/src/ndi_video_recv.cpp.o -c /home/rui/avatar_ws/src/avatar_interaction/video_audio_proc/src/ndi_video_recv.cpp

CMakeFiles/ndi_video_recv.dir/src/ndi_video_recv.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/ndi_video_recv.dir/src/ndi_video_recv.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/rui/avatar_ws/src/avatar_interaction/video_audio_proc/src/ndi_video_recv.cpp > CMakeFiles/ndi_video_recv.dir/src/ndi_video_recv.cpp.i

CMakeFiles/ndi_video_recv.dir/src/ndi_video_recv.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/ndi_video_recv.dir/src/ndi_video_recv.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/rui/avatar_ws/src/avatar_interaction/video_audio_proc/src/ndi_video_recv.cpp -o CMakeFiles/ndi_video_recv.dir/src/ndi_video_recv.cpp.s

# Object files for target ndi_video_recv
ndi_video_recv_OBJECTS = \
"CMakeFiles/ndi_video_recv.dir/src/ndi_video_recv.cpp.o"

# External object files for target ndi_video_recv
ndi_video_recv_EXTERNAL_OBJECTS =

ndi_video_recv: CMakeFiles/ndi_video_recv.dir/src/ndi_video_recv.cpp.o
ndi_video_recv: CMakeFiles/ndi_video_recv.dir/build.make
ndi_video_recv: /usr/lib/x86_64-linux-gnu/libSDL2_image.so
ndi_video_recv: 3rdparty/lib/x86_64-linux-gnu/libndi.so
ndi_video_recv: CMakeFiles/ndi_video_recv.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/rui/avatar_ws/src/avatar_interaction/video_audio_proc/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable ndi_video_recv"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/ndi_video_recv.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/ndi_video_recv.dir/build: ndi_video_recv

.PHONY : CMakeFiles/ndi_video_recv.dir/build

CMakeFiles/ndi_video_recv.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/ndi_video_recv.dir/cmake_clean.cmake
.PHONY : CMakeFiles/ndi_video_recv.dir/clean

CMakeFiles/ndi_video_recv.dir/depend:
	cd /home/rui/avatar_ws/src/avatar_interaction/video_audio_proc && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/rui/avatar_ws/src/avatar_interaction/video_audio_proc /home/rui/avatar_ws/src/avatar_interaction/video_audio_proc /home/rui/avatar_ws/src/avatar_interaction/video_audio_proc /home/rui/avatar_ws/src/avatar_interaction/video_audio_proc /home/rui/avatar_ws/src/avatar_interaction/video_audio_proc/CMakeFiles/ndi_video_recv.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/ndi_video_recv.dir/depend

