# AVSample Projects

Welcome to AVSample Projects! This repository is designed to be a comprehensive resource for anyone looking to understand the fundamentals of audio and video processing. Whether you're a student, a hobbyist, or a professional developer, these building blocks are intended to help you get started with audio and video applications.

## About This Repository

AVSample Projects aims to provide clear examples and thorough documentation to teach the basic concepts in audio and video processing. From simple audio capturing to basic video frame operations, this repository serves as your starting point into the world of audio and video applications.

## Table of Contents

- [Getting Started](#getting-started)
- [Modules](#modules)
  - [Audio Basics (Windows)](#audio-basics-windows)
  - [Video Basics (Linux)](#video-basics-linux)
- [Installation](#installation)

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes.

### Prerequisites
**For Audio Basics (Windows):**
- Windows OS (7/8/10)
- MSVC 2020 or later

**For Video Basics (Linux):**
- Ubuntu 16 or Later

## Audio Basics (Windows)
This module covers the fundamentals of audio processing on Windows platforms, including:

- Audio signal manipulation
- Basic audio filters
- Audio file operations
- Windows-specific audio APIs

#### List of Audio Projects

1. **Raw Audio Capture with PortAudio** - Develop an application to capture raw audio data using the PortAudio library. This project allows users to record audio from input devices in real-time, save raw audio data, and learn about audio stream handling.
2. **Converting Raw Audio to High-Quality AMR Format** - Create a tool that converts raw audio files into the Adaptive Multi-Rate (AMR) format to enhance audio quality and reduce file size, suitable for voice recordings.
3. **Audio Capture And AMR Encoding in Real-time** - Build an application that captures audio in real-time and encodes it into the AMR format directly, optimizing for mobile communications.
4. **Capturing Audio with Port Audio and AAC Encoding using FFmpeg** - Implement a system to capture audio using PortAudio and encode it in real-time using the Advanced Audio Coding (AAC) format through FFmpeg, aimed at achieving high-quality audio streams.
5. **PCM Data Mixing Algorithm** - Develop an algorithm to mix multiple Pulse Code Modulation (PCM) audio streams into a single output stream, useful for audio editing and DJ software.
6. **RTSP Audio Streaming Server using LIVE555** - Set up a Real Time Streaming Protocol (RTSP) server using the LIVE555 multimedia library to stream audio over the internet or local networks.


## Video Basics (Linux)

This module includes basic and advanced video processing techniques using FFMPEG on Linux platforms:

- Frame extraction
- Video overlays
- Basic transitions and effects
- Utilizing FFMPEG for video processing

#### List of Video Projects

1. **B-Frames significance in Video Encoding using FFMPEG** - Explore the role and benefits of B-Frames in video compression and encoding processes using FFMPEG.
2. **Pixel Formats conversion (RGB to YUV and Vice-Versa) using FFMPEG** - Implement a tool to convert video pixel formats between RGB and YUV using FFMPEG, crucial for video editing and broadcasting applications.
3. **RGB Images to MP4 using FFMPEG** - Create a tutorial or application to convert a series of RGB images into a coherent MP4 video file, useful for animation and video production.
4. **FFMPEG Camera Frames to MP4 File** - Develop a system to capture live camera frames and encode them into an MP4 file using FFMPEG, ideal for surveillance or live streaming.
5. **FFMPEG Camera Frames to RGB Images** - Set up a process to convert live camera frames into RGB images using FFMPEG, enabling detailed image processing and analysis.


#### Installation

## Video Basics (Linux)
Clone the repo, go to AVSampleProjects/Video. To build each project, use CMakeLists.txt associated with that folder. Before using CMaleLists.txt, modify the PKG_CONFIG_PATH to link to ffmpge libs and headers properly. Watch this video https://www.youtube.com/watch?v=UGJQo1XG1iM for more details on building FFMPEG on Ubuntu from sources.
Now 
 - mkdir build
 - cd build
 - cmake ..
 - make
   Run the application

## Audio Basics (Windows)
Run the solution files usign MSVC.
