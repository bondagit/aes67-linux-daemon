/************************************************************************************************************
 * WavPlay AM824
 * Copyright (C) 2020, Dolby Laboratories Inc.
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 ************************************************************************************************************/

#define VERSION_STRING "1.0"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include "portaudio.h"
#include "am824_framer.h"

#if defined(__linux__ )
#include "pa_linux_alsa.h"
#endif

#if defined(_WIN32) || defined(_WIN64)
#include "pa_win_wasapi.h"

#include <windows.h>
#include <Ks.h>
#include <Ksmedia.h>
#include <mmreg.h>

#if PA_USE_ASIO
#include "pa_asio.h"
#endif
#else
#include <sndfile.h>
#endif

typedef enum
{
	FF_PCM = 0,
	FF_FLOAT32,
}
FileFormat;


typedef enum
{
	ERR_FILE_NOT_FOUND = -101,
	ERR_BAD_INPUT_FILE,
	ERR_BAD_WAV_FORMAT,
	ERR_INCOMPLETE_INPUT_FILE,
	ERR_NOT_SUPPORTED,
	ERR_BAD_CMD_OPTION,
	ERR_NO_MEMORY,
	ERR_NO_DEVICE,
	ERR_NO_STREAM,
	ERR_PORTAUDIO,
	ERR_OK = 0
}
ErrorCode;


typedef struct
{
    unsigned long	frameIndex;  /* Index into sample array. */
    unsigned long	maxFrameIndex;
    unsigned int 	numChannels;
    unsigned int 	bytesPerSample;
    unsigned char   *audio;
    FileFormat      waveFileFormat;
    unsigned int    bitsPerSample;
    unsigned int    fs;
    unsigned int    blockAlign;
    unsigned long   totalBytes;
#if defined(_WIN32) || defined(_WIN64)
	HMMIO	waveFile;
#else
	SNDFILE         *waveFile;
#endif
   	unsigned int    am824_audio;
   	AM824SamplingFrequency    am824_fs;
   	unsigned int    am824_fs_match_wavfile;
   	unsigned int    am824_professional;
}
UserData;

void throwError(ErrorCode err, const char *format, ...)
{
    va_list args;
    va_start(args, format);

    fprintf(stderr, "***Error: ");
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
	fflush(stderr);
	exit(err);
}

/* This is the main callback functioned called by PortAudio. It is registered when the stream is created */

static int playCallback( const void *inputBuffer, void *outputBuffer,
                         unsigned long framesPerBuffer,
						 const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData )
{
    UserData *data = (UserData*)userData;
    unsigned char *rptr = &data->audio[data->frameIndex * data->numChannels * data->bytesPerSample];
    unsigned char *wptr = (unsigned char *)outputBuffer;
    unsigned int i;
    int finished;
    unsigned int framesLeft = data->maxFrameIndex - data->frameIndex;

    (void) inputBuffer; /* Prevent unused variable warnings. */
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;

    if( framesLeft < framesPerBuffer )
    {
        /* final buffer... */
        for( i=0; i<framesLeft * data->numChannels * data->bytesPerSample; i++)
		{
			*wptr++ = *rptr++;
        }
        for( ; i<framesPerBuffer * data->numChannels * data->bytesPerSample; i++ )
        {
			*wptr++ = 0;
       }
       data->frameIndex += framesLeft;
       finished = paComplete;
    }
    else
    {
        for( i=0; i< framesPerBuffer * data->numChannels * data->bytesPerSample; i++)
		{
			*wptr++ = *rptr++;
        }
        data->frameIndex += framesPerBuffer;
        finished = paContinue;
    }
    return finished;
}

/* Two versions of the following two functions exist, one for Windows and one for Linux. This approach was chosen
   because a platform independent wav file library was not used but rather Windows API and libasound directly */

#if defined(_WIN32) || defined(_WIN64)
int read_wav_file_header(char *playbackWaveFile, /* Input file name string */
						 UserData *outputData)   /* Output options */
{
	MMCKINFO mmckinfoParent;
	MMCKINFO mmckinfoSubchunk;
	WAVEFORMATEXTENSIBLE *format;

	outputData->waveFile = mmioOpenA(playbackWaveFile, 0, MMIO_READ | MMIO_ALLOCBUF);
	if (!outputData->waveFile)
	{
		throwError(ERR_FILE_NOT_FOUND, "Can't Open %s!", playbackWaveFile);
	}
	mmckinfoParent.fccType = mmioFOURCC('W', 'A', 'V', 'E');
	if (mmioDescend(outputData->waveFile, (LPMMCKINFO)&mmckinfoParent, 0, MMIO_FINDRIFF))
	{
		throwError(ERR_BAD_INPUT_FILE, "This file doesn't contain a WAVE!");
	}
	mmckinfoSubchunk.ckid = mmioFOURCC('f', 'm', 't', ' ');
	if (mmioDescend(outputData->waveFile, &mmckinfoSubchunk, &mmckinfoParent, MMIO_FINDCHUNK))
	{
		throwError(ERR_BAD_WAV_FORMAT, "Required fmt chunk was not found!");
	}

	format = (WAVEFORMATEXTENSIBLE *)malloc(mmckinfoSubchunk.cksize);

	
	if (mmioRead(outputData->waveFile, (HPSTR)format, mmckinfoSubchunk.cksize) != (LRESULT)mmckinfoSubchunk.cksize)
	{
		throwError(ERR_BAD_WAV_FORMAT, "Reading the fmt chunk!");
	}

	if ((format->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE) &&
		(format->Samples.wValidBitsPerSample != format->Format.wBitsPerSample))
	{
		throwError(ERR_NOT_SUPPORTED, "Different container size and bit depth not supported");
	}

	mmioAscend(outputData->waveFile, &mmckinfoSubchunk, 0);
	mmckinfoSubchunk.ckid = mmioFOURCC('d', 'a', 't', 'a');
	if (mmioDescend(outputData->waveFile, &mmckinfoSubchunk, &mmckinfoParent, MMIO_FINDCHUNK))
	{
		throwError(ERR_BAD_WAV_FORMAT, "Reading the data chunk!");
	}

	outputData->fs = format->Format.nSamplesPerSec;
	outputData->numChannels = format->Format.nChannels;
	outputData->totalBytes = mmckinfoSubchunk.cksize;
	outputData->bitsPerSample = format->Format.wBitsPerSample;
	outputData->bytesPerSample = outputData->bitsPerSample / 8;
	outputData->blockAlign = format->Format.nBlockAlign;

	if (format->Format.wFormatTag == WAVE_FORMAT_PCM)
	{
		outputData->waveFileFormat = FF_PCM;
	}
	else if (format->Format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
	{
		outputData->waveFileFormat = FF_FLOAT32;
	}
	else if (format->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE)
	{
		if (format->SubFormat == KSDATAFORMAT_SUBTYPE_PCM)
		{
			outputData->waveFileFormat = FF_PCM;
		}
		else if (format->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
		{
			outputData->waveFileFormat == FF_FLOAT32;
		}
		else
		{
			throwError(ERR_NOT_SUPPORTED, "Error: Unsupported WAVEFORMAT EXTENSIBLE SUBTYPE!");
		}
	}

	return(0);
}

unsigned long read_entire_wav_file(UserData *outputData, /* Input options*/
								   void *audioData)      /* Output data read form file */
{
	unsigned long readCount;

	readCount = mmioRead(outputData->waveFile, (char *)audioData, outputData->totalBytes);

	mmioClose(outputData->waveFile, 0);

	if (readCount != outputData->totalBytes)
	{
		throwError(ERR_INCOMPLETE_INPUT_FILE, "Failed to read all of audio data in wave file");
	}
	return(readCount);
}

#else
int read_wav_file_header(char *playbackWaveFile, /* Input filename string */
						 UserData *outputData)   /* Output options */
{
	uint32_t waveFormat;
	unsigned int i;
	uint8_t tmpByte;
	SF_INFO fileInfo;
	unsigned long long readCount;
	unsigned char *pByte;

	outputData->waveFile = sf_open(playbackWaveFile, SFM_READ, &fileInfo);
	if (!outputData->waveFile)
	{
		throwError(ERR_FILE_NOT_FOUND, "File %s not found\n", playbackWaveFile);
	}

	outputData->fs = fileInfo.samplerate;
	outputData->numChannels = fileInfo.channels;

	if ((waveFormat & SF_FORMAT_TYPEMASK) == SF_FORMAT_RF64)
	{
		throwError(ERR_NOT_SUPPORTED, "RF64 format not yet supported");		
	}

	waveFormat = fileInfo.format;
	if (((waveFormat & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV) &&
		((waveFormat & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAVEX))
	{
		throwError(ERR_BAD_INPUT_FILE, "Input file is not a wavefile");
	}

	switch(waveFormat & SF_FORMAT_SUBMASK)
	{
	case SF_FORMAT_PCM_16:
		outputData->waveFileFormat = FF_PCM;
		outputData->bitsPerSample = 16;
		break;
	case SF_FORMAT_PCM_24:
		outputData->waveFileFormat = FF_PCM;
		outputData->bitsPerSample = 24;
		break;
	case SF_FORMAT_PCM_32:
		outputData->waveFileFormat = FF_PCM;
		outputData->bitsPerSample = 32;
		break;
	case SF_FORMAT_FLOAT:
		outputData->waveFileFormat = FF_FLOAT32;
		outputData->bitsPerSample = 32;
		break;
	default:
		throwError(ERR_NOT_SUPPORTED, "Unsupported wavefile format supported");
	}

	outputData->bytesPerSample = outputData->bitsPerSample / 8;
	outputData->blockAlign = outputData->numChannels * outputData->bytesPerSample;
	outputData->totalBytes = outputData->blockAlign * fileInfo.frames;

	return(0);
}

unsigned long read_entire_wav_file(UserData *outputData, /* Input Options */
								   void *audioData)      /* Output Audio data from file */
{
	unsigned long readCount;

	readCount = sf_read_raw(outputData->waveFile, audioData, outputData->totalBytes);
	if (readCount != outputData->totalBytes)
	{
		throwError(ERR_INCOMPLETE_INPUT_FILE, "Failed to read all of audio data in wave file");
	}
	return(readCount);
}


#endif



unsigned int countBits(unsigned int a)
{
	unsigned int count = 0;
	while (a)
	{
		count += (a & 0x1);
		a >>= 1;
	}
	return(count);
}

/* This function takes standard PCM audio plus channel status options and creates the samples for AM824 format */

void am824Convert(UserData *userData, /* Input options */
				  void *audio,        /* Input PCM samples */
				  void **am824audio)  /* Output AM824 samples, always 32bit */
{
	unsigned long bytesConverted = 0;
	uint32_t inputSample32;
	uint16_t inputSample16;
	uint8_t *inputPtr;
	uint32_t *outputPtr;
	unsigned int channel;
	unsigned long outputMemSize;
	AM824ErrorCode err;


	if ((userData->bytesPerSample != 3) && (userData->bytesPerSample != 2))
	{
		throwError(ERR_NOT_SUPPORTED, "Only 2 or 3 bytes per sample supported for AM824 mode");
	}

	AM824Framer framer(userData->numChannels, userData->bytesPerSample * 8, AM824_LITTLE_ENDIAN, err);
	if (err != AM824_ERR_OK)
	{
		if (err == AM824_ERR_UNSUPPORTED_BITDEPTH)
		{
			throwError(ERR_NOT_SUPPORTED, "AM824 framer reports bitdepth %d not supported", userData->bytesPerSample * 8);
		}
	}

	//framer.testCRC();

	if (userData->am824_audio)
	{
		framer.setAudioMode();
	}
	else
	{
		framer.setDataMode();
	}
	if (userData->am824_fs_match_wavfile)
	{
		switch(userData->fs)
		{
		case 32000:
			framer.setSamplingFrequency(FS_32000_HZ);
			break;
		case 44100:
			framer.setSamplingFrequency(FS_44100_HZ);
			break;
		case 48000:
			framer.setSamplingFrequency(FS_48000_HZ);
			break;
		default:	
			framer.setSamplingFrequency(FS_NOT_INDICATED);
		}
	}
	else
	{
		framer.setSamplingFrequency(userData->am824_fs);
	}
	if (userData->am824_professional)
	{
		framer.setProfessionalMode();
	}
	else
	{
		framer.setConsumerMode();
	}
	outputMemSize = (userData->totalBytes * sizeof(uint32_t)) / userData->bytesPerSample;
	*am824audio = malloc(outputMemSize);
	outputPtr = (uint32_t *) *am824audio;
	inputPtr = (uint8_t *)audio;

	while(bytesConverted < userData->totalBytes)
	{
		for (channel = 0 ; channel < userData->numChannels ; channel++)
		{
			if (userData->bytesPerSample == 3)
			{
				inputSample32 = *((uint32_t *)inputPtr) & 0xffffff;
				framer.getAM824Sample(inputSample32, (uint8_t *)outputPtr);
			}
			else
			{
				inputSample16 = *((uint16_t *)inputPtr);
				framer.getAM824Sample(inputSample16, (uint8_t *)outputPtr);
			}
			outputPtr += 1;
			inputPtr += userData->bytesPerSample;
			bytesConverted += userData->bytesPerSample;
		}	
	}
	userData->totalBytes = (userData->totalBytes * sizeof(uint32_t))/userData->bytesPerSample;
	userData->bytesPerSample = sizeof(uint32_t);
	userData->bitsPerSample = 32;
	userData->blockAlign = userData->numChannels * userData->bytesPerSample;

}

void list_devices(void)
{
	int     i, numDevices, defaultDisplayed;
	const   PaDeviceInfo *deviceInfo;

	printf("PortAudio version: 0x%08X\n", Pa_GetVersion());
	printf("Version text: '%s'\n", Pa_GetVersionText());

	numDevices = Pa_GetDeviceCount();
	if (numDevices < 0)
	{
		throwError(ERR_NO_DEVICE, "Pa_GetDeviceCount returned 0x%x\n", numDevices);
	}

	printf("Number of devices = %d\n", numDevices);
	for (i = 0; i < numDevices; i++)
	{
		deviceInfo = Pa_GetDeviceInfo(i);
		if (deviceInfo->maxOutputChannels > 0) {
			printf("--------------------------------------- device #%d\n", i);

			/* Mark global and API specific default devices */
			defaultDisplayed = 0;
			if (i == Pa_GetDefaultOutputDevice())
			{
				printf("[ Default Output");
				defaultDisplayed = 1;
			}
			else if (i == Pa_GetHostApiInfo(deviceInfo->hostApi)->defaultOutputDevice)
			{
				const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo(deviceInfo->hostApi);
				printf("[ Default %s Output", hostInfo->name);
				defaultDisplayed = 1;
			}

			if (defaultDisplayed)
				printf(" ]\n");

			/* print device info fields */
#ifdef WIN32
			{   /* Use wide char on windows, so we can show UTF-8 encoded device names */
				wchar_t wideName[MAX_PATH];
				MultiByteToWideChar(CP_UTF8, 0, deviceInfo->name, -1, wideName, MAX_PATH - 1);
				wprintf(L"Name                        = %s\n", wideName);
			}
#else
			printf("Name                        = %s\n", deviceInfo->name);
#endif
			printf("Host API                    = %s\n", Pa_GetHostApiInfo(deviceInfo->hostApi)->name);
			printf("Max output channels          = %d\n", deviceInfo->maxOutputChannels);

			printf("Default low output latency   = %4.4f\n", deviceInfo->defaultLowOutputLatency);
			printf("Default high output latency  = %4.4f\n", deviceInfo->defaultHighOutputLatency);

#ifdef WIN32
#if PA_USE_ASIO
			/* ASIO specific latency information */
			if (Pa_GetHostApiInfo(deviceInfo->hostApi)->type == paASIO) {
				long minLatency, maxLatency, preferredLatency, granularity;

				err = PaAsio_GetAvailableLatencyValues(i,
					&minLatency, &maxLatency, &preferredLatency, &granularity);

				printf("ASIO minimum buffer size    = %ld\n", minLatency);
				printf("ASIO maximum buffer size    = %ld\n", maxLatency);
				printf("ASIO preferred buffer size  = %ld\n", preferredLatency);

				if (granularity == -1)
					printf("ASIO buffer granularity     = power of 2\n");
				else
					printf("ASIO buffer granularity     = %ld\n", granularity);
			}
#endif /* PA_USE_ASIO */
#endif /* WIN32 */

			printf("Default sample rate         = %8.2f\n", deviceInfo->defaultSampleRate);
		}
	}
	return;
}

void print_usage(void)
{
	fprintf(stderr, "wavplay_am824 [OPTION]... <OUTPUT FILE> v%s\n", VERSION_STRING);
	fprintf(stderr, "Copyright Dolby Laboratories Inc., 2020. All rights reserved.\n\n");
	fprintf(stderr, "-h                   Display this messgage\n");
	fprintf(stderr, "-ld                  List playback devices\n");
	fprintf(stderr, "-l                   Preferred playout latency in seconds\n");
    fprintf(stderr, "-buf <samples>       Playout buffer size in frames (samples x channels)\n");
	fprintf(stderr, "-d <index>           Device index to use for playback\n");
	fprintf(stderr, "-am824               Using virtual sound card feeding an AM824/2110-31 stream\n");
	fprintf(stderr, "                     The following keywords can follow the '-am824' switch to modify channel status:\n");
	fprintf(stderr, "    audio, nonaudio, fs_not_indicated, fs_48k, fs_441k, fs_32k, professional, consumer\n");
#if defined(_WIN32) || defined(_WIN64)
	fprintf(stderr, "-e <index>           Uses WASPI exclusive mode if selected device is a WASAPI device\n");
#endif
	fprintf(stderr, "\n");
}


/*******************************************************************/
int main(int argc, char *argv[])
{
    PaStreamParameters  outputParameters;
    PaStream*           stream;
    PaError             err = paNoError;
    UserData          data;
    unsigned int                 i;
    unsigned long       bytesRead;
    unsigned int        framesPerBuffer = 128;
    float               userLatency = 0.0;
	char				playbackWavFileName[256] = "";
	unsigned int bitDepth = 16;
	PaSampleFormat		sampleFormat = paInt16;
	double sampleRate = 48000.0;
	double startTime, finishTime;
	void *audio;
#if defined(_WIN32) || defined(_WIN64)
	struct PaWasapiStreamInfo wasapiInfo;
	int 				waspiExclusiveMode = 0;
#endif
	unsigned int am824Mode = 0;
	void *am824audio;


    outputParameters.device = paNoDevice;
    audio = NULL;

	err = Pa_Initialize();

	if( err != paNoError )
	{
		throwError(ERR_PORTAUDIO, "Pa_Initialize returned %d, %s", err, Pa_GetErrorText(err));
	}

    if (argc < 2)
    {
    	print_usage();
    	list_devices();
    	exit(0);
    }

	for (i = 1; i < (unsigned int)argc; i++)
	{
		// check to see if its a filename first
		if ((argv[i][0] != '-') && (strlen(playbackWavFileName) == 0))
		{
			strcpy(playbackWavFileName, argv[i]);
		}
		else if (!strcmp(argv[i], "-d"))
		{
			if (i == (argc - 1))
			{
				print_usage();
				throwError(ERR_NO_DEVICE, "Can't find soundcard index");
			}
			outputParameters.device = atoi(argv[i + 1]);
			// We increment i here to step over the next parameter
			// which has been parsed as the value
			i++;
		}
#if defined(_WIN32) || defined(_WIN64)
		else if (!strcmp(argv[i], "-e"))
		{
			waspiExclusiveMode = 1;
		}
#endif
		else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-help") || !strcmp(argv[i], "--help"))
		{
				print_usage();
				exit(0);
		}
		else if (!strcmp(argv[i], "-buf"))
		{
			if (i == (argc - 1))
			{
				print_usage();
				throwError(ERR_NO_DEVICE, "Can't find buffer size");
			}
			framesPerBuffer = atoi(argv[i + 1]);
			// We increment i here to step over the next parameter
			// which has been parsed as the value
			i++;
		}
		else if (!strcmp(argv[i], "-l"))
		{
			if (i == (argc - 1))
			{
				print_usage();
				throwError(ERR_NO_DEVICE, "Can't find latency");
			}
			userLatency = atof(argv[i + 1]);
			// We increment i here to step over the next parameter
			// which has been parsed as the value
			i++;				
		}
		else if (!strcmp(argv[i], "-ld"))
		{
				list_devices();
				exit(0);
		}
		else if (!strcmp(argv[i], "-am824"))
		{
			am824Mode = 1;
			data.am824_audio = 1;
			data.am824_professional = 0;
			data.am824_fs_match_wavfile = 1;
			i++;
			while((i < (unsigned int)argc) && (argv[i][0] != '-'))
			{
				if (!strcmp(argv[i], "audio"))
				{
					data.am824_audio=1;
				}
				else if (!strcmp(argv[i], "nonaudio"))
				{
					data.am824_audio=0;
				}
				else if (!strcmp(argv[i], "fs_not_indicated"))
				{
					data.am824_fs = FS_NOT_INDICATED;
					data.am824_fs_match_wavfile = 0;
				}
				else if (!strcmp(argv[i], "fs_48k"))
				{
					data.am824_fs = FS_48000_HZ;
					data.am824_fs_match_wavfile = 0;

				}
				else if (!strcmp(argv[i], "fs_441k"))
				{
					data.am824_fs = FS_44100_HZ;
					data.am824_fs_match_wavfile = 0;
				}
				else if (!strcmp(argv[i], "fs_32k"))
				{
					data.am824_fs = FS_32000_HZ;
					data.am824_fs_match_wavfile = 0;
				}
				else if (!strcmp(argv[i], "professional"))
				{
					data.am824_professional=1;
				}
				else if (!strcmp(argv[i], "consumer"))
				{
					data.am824_professional=0;
				}
				else
				{
					break;
				}
				i++;
			}
			i--;
		}
		else
		{
			print_usage();
			throwError(ERR_BAD_CMD_OPTION, "Option %s not recognized", argv[i]);
		}
	}

	if (read_wav_file_header(playbackWavFileName, &data))
	{
		throwError(ERR_BAD_WAV_FORMAT, "Bad wav header");
	}

	audio = malloc(data.totalBytes);
	if (!audio)
	{
		throwError(ERR_NO_MEMORY, "Memory allocation failed");
	}

	bytesRead = read_entire_wav_file(&data, audio);
	if (bytesRead != data.totalBytes)
	{
		throwError(ERR_INCOMPLETE_INPUT_FILE, "Couldn't read all input data");
	}

	if (am824Mode)
	{
		printf("AM824 output mode selected\n");
		am824Convert(&data, audio, &am824audio);
		free(audio);
		audio = am824audio;
	}

	if (outputParameters.device == paNoDevice)
	{
		outputParameters.device = Pa_GetDefaultOutputDevice(); 
	}

	if (outputParameters.device == paNoDevice)
	{
		throwError(ERR_NO_DEVICE, "No default output device");
	}

#if defined(_WIN32) || defined(_WIN64)
	if 	((waspiExclusiveMode) &&
		(Pa_GetHostApiInfo(Pa_GetDeviceInfo(outputParameters.device)->hostApi)->type == paWASAPI)) {
		wasapiInfo.size = sizeof(PaWasapiStreamInfo);
		wasapiInfo.hostApiType = paWASAPI;
		wasapiInfo.version = 1;
		wasapiInfo.flags = (paWinWasapiExclusive | paWinWasapiThreadPriority);
		wasapiInfo.channelMask = 0;
		wasapiInfo.hostProcessorOutput = NULL;
		wasapiInfo.hostProcessorInput = NULL;
		wasapiInfo.threadPriority = eThreadPriorityProAudio;
		outputParameters.hostApiSpecificStreamInfo = (&wasapiInfo);
		printf("Detected WASAPI device and setting exclusive mode\n");
	}
	else
	{
#endif
		outputParameters.hostApiSpecificStreamInfo = NULL;
#if defined(_WIN32) || defined(_WIN64)
	}
#endif

	if (userLatency > 0.0)
	{
		outputParameters.suggestedLatency = userLatency;
	}
	else
	{
    	outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultHighOutputLatency;
    }

    switch(data.waveFileFormat)
    {
    case FF_PCM:
  
    	if ((data.bitsPerSample != 8) && (data.bitsPerSample != 16)
			&& (data.bitsPerSample != 24) && (data.bitsPerSample != 32))
    	{
        	throwError(ERR_NOT_SUPPORTED, "Unsupported WAVE_FORMAT_PCM bitdepth");
    	}
    	break;
    case FF_FLOAT32:
 		if (data.bitsPerSample != 32)
 		{
        	throwError(ERR_BAD_WAV_FORMAT, "Wavefile indicated floating point but bits per sample is not 32");   					
 		}
     	break;
    default:
        throwError(ERR_NOT_SUPPORTED, "Unsupported WAV format tag (WAVE_FORMAT_PCM & WAVE_FORMAT_IEEE_FLOAT supported)");
    }
	
   	if (data.bitsPerSample == 16)
    {
    	outputParameters.sampleFormat = paInt16;
    }
   	else if (data.bitsPerSample == 8)
    {
    	outputParameters.sampleFormat = paInt8;
    }
    else if (data.bitsPerSample == 24)
    {
    	outputParameters.sampleFormat = paInt24;
    }
    else if (data.bitsPerSample == 32)
    {
    	if (data.waveFileFormat == FF_FLOAT32)
    	{
    		outputParameters.sampleFormat = paFloat32;
    	}
    	else
    	{
    		outputParameters.sampleFormat = paInt32;
    	}
    }
    else
    {
    	throwError(ERR_NOT_SUPPORTED, "Unsupported bitdepth %d", data.bitsPerSample);
    }

    outputParameters.channelCount = data.numChannels;
	sampleRate = (double)data.fs;
	err = Pa_IsFormatSupported(NULL, &outputParameters, sampleRate);
	if (err != paNoError)
	{
		throwError(ERR_NOT_SUPPORTED, "Pa_IsFormatSupported returned %d, %s", err, Pa_GetErrorText(err));
	}

	printf("Pa_IsFormatSupported succeeded\n");

	printf("device: %u\nchannels: %u\nsampleFormat: %lu\nlatency: %f\nsampleRate: %u\n", outputParameters.device, outputParameters.channelCount, outputParameters.sampleFormat, outputParameters.suggestedLatency, data.fs);

    // Set callback parameters
    data.frameIndex = 0;  /* Index into sample array. */
    data.maxFrameIndex = (unsigned long) data.totalBytes / data.blockAlign;
    data.audio = (unsigned char *)audio;

    printf("\n=== Now playing back. ===\n"); fflush(stdout);
    err = Pa_OpenStream(
              &stream,
              NULL, /* no input */
              &outputParameters,
              data.fs,
              framesPerBuffer,
              paClipOff,      /* we won't output out of range samples so don't bother clipping them */
              playCallback,
              &data );
    if( err != paNoError )
    {
    	Pa_Terminate();
    	throwError(ERR_NO_STREAM, "Pa_OpenStream returned %d, %s", err, Pa_GetErrorText( err ));
    }

    if( stream )
    {
#if defined(__linux__ )
    	PaAlsa_EnableRealtimeScheduling( stream, 1 );
    	printf("RealTime Scheduling enabled\n");
#endif
        err = Pa_StartStream( stream );
        if( err != paNoError )
        {
        	Pa_Terminate();
       		throwError(ERR_NO_STREAM, "Pa_StartStream returned %d, %s", err, Pa_GetErrorText( err ));
        }
        printf("Waiting for playback to start.\n");

        do
        {
        	startTime = Pa_GetStreamTime(stream);
        }
        while (startTime == 0.0);

        printf("Waiting for playback to finish.\n");
        fflush(stdout);
		finishTime = data.maxFrameIndex / data.fs;
		while ((err = Pa_IsStreamActive(stream)) == 1)
		{
			Pa_Sleep(1000);			
		}
		printf("\n");

        if( err != paNoError )
        {
        	Pa_Terminate();
       		throwError(ERR_NO_STREAM, "Pa error %d received during playback: %s", err, Pa_GetErrorText( err ));
        }
        
        err = Pa_CloseStream( stream );
        if( err != paNoError )

        
        printf("Done.\n"); fflush(stdout);
    }

	if (audio)
	{
		free(audio);
	}
    Pa_Terminate();
    exit(0);
}

