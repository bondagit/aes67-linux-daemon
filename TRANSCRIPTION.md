# Transcription Feature for AES67 Linux Daemon

## Introduction

The AES67 Linux Daemon now supports real-time transcription of audio streams  using [OpenAI's Whisper](https://github.com/openai/whisper), integrated through [Whisper.cpp](https://github.com/ggml-org/whisper.cpp), a high-performance C/C++ inference of Whisper. The transcription feature enables speech-to-text conversion of daemon's configured Sinks with good robustness and accuracy, making it a valuable addition for multimedia and broadcast applications.

Audio transcription feature has been integrated while maintaining robust performance in multi-sink setups by leveraging a multithreaded architecture.

This document explains the architecture of the transcription feature, how to build and configure the daemon with transcription support, and how to test the feature.


## Architecture

The transcription feature in the AES67 Linux Daemon builds upon a multithreaded design to achieve high performance and minimize the risk of audio capture overruns.

1. **Threads**:
   - **Capture Thread**: Captures the audio streams from the configured channels using the ALSA interface. It writes the captured audio data into per Sink **rotating audio buffers** to decouple audio capture from transcription computation. Such decoupling is required as the ALSA buffer size depends on the specifc audio device and we want to cope with spikes in transcription processing that could cause capture overruns.
   The capture thread also perfoms audio resampling to 16KHz, downmixing, audio format conversion from PCM signed to float and audio silence detection to filter out silence buffers.
   - **Transcription Thread**: For each Sink reads data from the current audio buffer and executes transcriptions via Whisper that uses the available CPU cores and GPUs for processing. Transcription results are stored in text buffers that are accessible through the daemon's REST API and the web interface.

2. **Rotating Audio Buffers**:
   - The capture thread writes audio data into rotating audio buffers of a specifc duration. These buffers ensure that audio capture is independent from the transcriptions and they can run in parallel. The transcription can start when the first audio buffer with no silence is filled, so it runs with a latency of a single buffer.
   - The number of rotating buffers and their durations are user-configurable, see the daemon's transcription parameters below.

3. **REST Interface & Web Dialog**:
   - Real-time transcription results are presented via the web interface, where each configured Sink has a dedicated transcription dialog.
   - Additionally, transcription logs and warnings (e.g., slow transcription processing) are emitted into the daemon's log for monitoring purposes.

4. **Configuration Parameters**:
   - Some new parameters have been added to the daemon's configuration file to allow customization of the transcription feature. These parameters are described in the **How to Build and Run a Test** section below and the the daemon's README.

This architecture ensures a robust audio buffer handling, independence from the specific ALSA audio driver in use, real-time transcription, and seamless integration with existing daemon functionality.

See the diagram below:

![transcription schema](https://github.com/user-attachments/assets/f5ebf0b7-0dcc-456a-9aea-8a54e83f2e7b)


## How to Build and Run a Test

To enable the transcription feature in the AES67 Linux Daemon, follow these steps:

### 1. Build the Daemon with Transcription Support

Use the build.sh script to compile the daemon with the transcription feature. Make sure that the _WITH_TRANSCRIBER_ flag to ON. The build.sh script will also build and download the Whisper base model.

### 2. Configure the daemon

Check the daemon configuration parameters for fine tuning:


    {
      "transcriber_enabled": true,
      "transcriber_channels": 4,
	  "transcriber_files_num": 4,
      "transcriber_file_duration": 5,
      "transcriber_model": "../3rdparty/whisper.cpp/models/ggml-base.en.bin",
      "transcriber_language": "en",
      "transcriber_openvino_device": "CPU"
    }

where:

> **transcriber\_enabled**
> JSON boolean Enables or disables transcription for all Sinks. Set to true to enable.

> **transcriber\_channels**
> JSON number specifying the number of audio channels captured by the ALSA capture thread.

> **transcriber\_files\_num**
> JSON number specifying the number of buffers in the rotating audio buffers pool.

> **transcriber\_file\_duration**: 
> JSON number specifying the duration (in seconds) of each audio buffer.

> **transcriber\_model**: 
> JSON string specifying the path to the Whisper model file. The default is the base English model.

> **transcriber\_language**: 
> JSON string specifying the language setting for Whisper. Default is English "en".

> **transcriber\_openvino\_device**: 
> JSON string specifying the OpenVINO device for transcription inference, if supported by the current model. Default is "CPU".

The fiest 5 parameters can be modfied also via the Web interface in the _Config_ tab.

### 3. Test the Transcription

- Using the WebUI create a Sink in the daemon by selecting the remote source.
- Navigate to the Sinks page.
- Click on the Transcription Icon next to the Sink entry to monitor the transcription of that Sink. The transcription dialog displays the extracted audio in real time and updates it every 5 seconds.
- The transcription results are also logged in the daemon's log file, along with performance warnings in case the transcription engine is running slowly due to hardware limitations.

### 4. Notes

- Integration with Whisper: the daemon implements a basic integration with Whisper. Real-time transcription poses some challenges: 
  - audio chunking should be done at word boundaries to avoid cutting words.
  - adoption of LocalAgreement-2 policy can improve the transcirption accuracy: incremental audio chunks can be presented to Whisper and a confirmed transcription is returned when 2 runs agree on a text prefix.
- Performance Considerations: Whisper transcription is computationally demanding. On a system with a _Intel Core i9-11900H_ processor:
  - the _base_ model supports up to 4 or 5 simultaneous real-time transcriptions
  - the _small_ model supports up to 1 or 2 real-time transcriptions due to higher computational requirements
   - larger models require dedicated GPUs for real-time performance.

- System Monitoring: monitor warnings in the daemon's log. Check for messages like the following and in case reduce the load:

      transcriber:: requesting current capture file, probably running to slow, skipping file     
      transcriber:: id X whisper processing took longer than the audio buffer duration


