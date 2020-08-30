
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

#include <stdint.h>

#define CHANNEL_STATUS_BYTES 24

#define WIDTH (8)
#define BOTTOMBIT 1
#define REFLECTED_POLYNOMIAL 0xb8  // Unreflected is 0x1d

enum AM824ErrorCode {
  AM824_ERR_OK = 0,
  AM824_ERR_BAD_SAMPLING_FREQUENCY = -1,
  AM824_ERR_UNSUPPORTED_BITDEPTH = -2
};

enum AM824SamplingFrequency {
  FS_NOT_INDICATED = 0,
  FS_44100_HZ = 1,
  FS_48000_HZ = 2,
  FS_32000_HZ = 3
};

enum AM824Endianess { AM824_BIG_ENDIAN, AM824_LITTLE_ENDIAN };

class AM824Framer {
  uint8_t channelStatusIndex;
  uint8_t channelStatusMask;
  uint8_t channelStatus[CHANNEL_STATUS_BYTES];
  uint8_t subFrameCounter;
  uint8_t numChannels;
  uint8_t bitDepth;
  uint8_t crcTable[256];
  AM824Endianess endian;

  static uint8_t getParity(unsigned int n) {
    uint8_t parity = 0;
    while (n) {
      parity = 1 - parity;
      n = n & (n - 1);
    }
    return parity;
  }

  void crcTableInit(void) {
    uint8_t remainder;

    for (int dividend = 0; dividend < 256; ++dividend) {
      remainder = dividend << (WIDTH - 8);
      for (uint8_t bit = 0; bit < 8; bit++) {
        if (remainder & BOTTOMBIT) {
          remainder = (remainder >> 1) ^ REFLECTED_POLYNOMIAL;
        } else {
          remainder = (remainder >> 1);
        }
      }
      crcTable[dividend] = remainder;
    }
  }

  void setCRC(void) {
    uint8_t data;
    uint8_t remainder = 0xff;

    for (int byte = 0; byte < 23; byte++) {
      data = channelStatus[byte] ^ remainder;
      remainder = crcTable[data];
    }
    channelStatus[23] = remainder;
  }

 public:
  // Input number of channels and the bitdepth of the input samples
  // Note that the output bit depth is always 24 bit
  AM824Framer(
      uint8_t
          newNumChannels, /* Input - Number of channels of input/output audio */
      uint8_t newBitDepth, /* Input - Bit depth of input audio, output is always
                              32 bit */
      AM824Endianess outputEndianess, /* Input = Endianess of output samples,
                                         input is always machine order */
      AM824ErrorCode& err)            /* Output - error code */
  {
    uint8_t i;
    channelStatusIndex = 0;
    channelStatusMask = 1;
    numChannels = newNumChannels;
    subFrameCounter = 0;

    bitDepth = newBitDepth;
    endian = outputEndianess;
    // Set certain channel status bits
    // Clear it first
    for (i = 0; i < CHANNEL_STATUS_BYTES; i++) {
      channelStatus[i] = 0;
    }
    // Default Channel Status
    // Professional Bit
    channelStatus[0] |= 1;
    // Non-audio PCM
    channelStatus[0] |= 1 << 1;
    // 48kHz
    channelStatus[0] |= 2 << 6;

    switch (bitDepth) {
      case 16:
        channelStatus[2] |= 1 << 3;
        break;
      case 20:
        // Use of Auxillary bits
        channelStatus[2] |= 4;
        // 20 bit data
        channelStatus[2] |= 1 << 3;
        break;
      case 24:
        // Use of Auxillary bits
        channelStatus[2] |= 4;
        // 24 bit data
        channelStatus[2] |= 5 << 3;
        break;
      default:
        err = AM824_ERR_UNSUPPORTED_BITDEPTH;
        return;
    }
    crcTableInit();
    setCRC();
    err = AM824_ERR_OK;
  }

  AM824ErrorCode setSamplingFrequency(AM824SamplingFrequency fs_code) {
    if (fs_code > FS_32000_HZ) {
      return (AM824_ERR_BAD_SAMPLING_FREQUENCY);
    }
    // Reset top two bits and set accordingly
    channelStatus[0] &= 0x3f;
    channelStatus[0] |= fs_code << 6;
    setCRC();
    return (AM824_ERR_OK);
  }

  void setProfessionalMode(void) {
    channelStatus[0] |= 1;
    setCRC();
  }

  void setConsumerMode(void) {
    channelStatus[0] &= 0xfe;
    setCRC();
  }

  void setAudioMode(void) {
    channelStatus[0] &= 0xfd;
    setCRC();
  }

  void setDataMode(void) {
    channelStatus[0] |= 2;
    setCRC();
  }

  void getAM824Sample(uint32_t inputSample, uint8_t* outputBytes) {
    uint32_t outputSample;
    bool channelStatusBit =
        channelStatus[channelStatusIndex] & channelStatusMask;
    bool userBit = false;
    bool validityBit = false;

    // Input samples are MSB justified as per AES3
    if (bitDepth == 16) {
      outputSample = inputSample << 8;
    } else {
      outputSample = inputSample;
    }

    // Detect block start
    if ((channelStatusIndex == 0) && (channelStatusMask == 1)) {
      outputSample |= 1 << 29;
    }
    // Detect frame start
    if (subFrameCounter == 0) {
      outputSample |= 1 << 28;
    }

    outputSample |= channelStatusBit << 26;
    outputSample |= userBit << 25;
    outputSample |= validityBit << 24;

    // Now complete except for parity so calculate that
    outputSample |= getParity(outputSample) << 27;

    // Now complete all the wraparound checks
    // Note that channel status chan be different for the different subframes
    // (channels) but in this example channel status is set to be the same for
    // all subframes (channels)
    subFrameCounter++;
    if (subFrameCounter == numChannels) {
      subFrameCounter = 0;
      // Move to next channel status bit
      if (channelStatusMask == 0x80) {
        channelStatusMask = 1;
        channelStatusIndex++;
        if (channelStatusIndex == CHANNEL_STATUS_BYTES) {
          channelStatusIndex = 0;
        }
      } else {
        channelStatusMask = channelStatusMask << 1;
      }
    }
    if (endian == AM824_BIG_ENDIAN) {
      // Return in 32 bit Big Endian format
      *outputBytes++ = (outputSample & 0xff000000) >> 24;
      *outputBytes++ = (outputSample & 0x00ff0000) >> 16;
      *outputBytes++ = (outputSample & 0x0000ff00) >> 8;
      *outputBytes++ = (outputSample & 0x000000ff) >> 0;
    } else {
      // Return in 32 bit Little Endian format
      *outputBytes++ = (outputSample & 0x000000ff) >> 0;
      *outputBytes++ = (outputSample & 0x0000ff00) >> 8;
      *outputBytes++ = (outputSample & 0x00ff0000) >> 16;
      *outputBytes++ = (outputSample & 0xff000000) >> 24;
    }
  }

  /* Simple test code to check CRC implementation */
  /* See EBU Tech 3250 or AES3 for the reference for these examples */
  void testCRC(void) {
    unsigned int i;
    for (i = 0; i < CHANNEL_STATUS_BYTES; i++) {
      channelStatus[i] = 0;
    }
    // From AES3-2-2009-r2019 - Example 1
    channelStatus[0] = 0x3d;
    channelStatus[1] = 2;
    channelStatus[4] = 2;
    setCRC();
    if (channelStatus[23] == 0x9b) {
      printf("Example 1 - passed\n");
    } else {
      printf("Example 1 - failed, expecting 0x9b, got 0x%x\n",
             channelStatus[23]);
    }
    channelStatus[0] = 0x01;
    channelStatus[1] = 0;
    channelStatus[4] = 0;
    setCRC();
    if (channelStatus[23] == 0x32) {
      printf("Example 2 - passed\n");
    } else {
      printf("Example 2 - failed, expecting 0x32, got 0x%x\n",
             channelStatus[23]);
    }
  }
};