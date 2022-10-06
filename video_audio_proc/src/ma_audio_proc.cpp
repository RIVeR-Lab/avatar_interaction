#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <iostream>

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    ma_encoder* pEncoder = (ma_encoder*)pDevice->pUserData;
    MA_ASSERT(pEncoder != NULL);
    ma_encoder_write_pcm_frames(pEncoder, pInput, frameCount, NULL);

    (void)pOutput;
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_context context;
    ma_device_info* pPlaybackDeviceInfos;
    ma_uint32 playbackDeviceCount;
    ma_device_info* pCaptureDeviceInfos;
    ma_uint32 captureDeviceCount;
    ma_uint32 iDevice;
    ma_encoder_config encoderConfig;
    ma_encoder encoder;
    ma_device_config deviceConfig;
    ma_uint32 deviceId;
    ma_device device;
    ma_backend backend_list[] = {ma_backend_alsa};

    
    char device_name[] = "Cam Link 4K";
    bool device_found = false;

    if (argc < 2) 
    {
      printf("No output file.\n");
      return -1;
    }

    if (ma_context_init(NULL, sizeof(backend_list)/sizeof(backend_list[0]), NULL, &context) != MA_SUCCESS) 
    {
      printf("Failed to initialize context.\n");
      return -2;
    }

    result = ma_context_get_devices(&context, NULL, NULL, &pCaptureDeviceInfos, &captureDeviceCount);
    if (result != MA_SUCCESS) 
    {
      printf("Failed to retrieve device information.\n");
      return -3;
    }

    // printf("Playback Devices\n");
    // for (iDevice = 0; iDevice < playbackDeviceCount; ++iDevice) 
    // {
    //   printf("    %u: %s\n", iDevice, pPlaybackDeviceInfos[iDevice].name);
    // }

    // printf("\n");

    printf("Capture Devices\n");
    for (iDevice = 0; iDevice < captureDeviceCount; ++iDevice) 
    {
      if (std::string(pCaptureDeviceInfos[iDevice].name).find(device_name) != std::string::npos)
      {
        std::cout << std::string(pCaptureDeviceInfos[iDevice].name) << std::endl;
        // std::cout << "Device " << iDevice << ": " << pCaptureDeviceInfos[iDevice].id.alsa << std::endl;
        deviceId = iDevice;
        device_found = true;
        break;
      }
      std::cout << iDevice << std::endl;
    }

    if (!device_found)
    {
      std::cout << "Could not find the device, exit!" << std::endl;
      exit(-1);
    }



    encoderConfig = ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, 2, 44100);

    if (ma_encoder_init_file(argv[1], &encoderConfig, &encoder) != MA_SUCCESS) 
    {
      printf("Failed to initialize output file.\n");
      return -1;
    }
    deviceConfig = ma_device_config_init(ma_device_type_capture);
    deviceConfig.capture.format   = encoder.config.format;
    deviceConfig.capture.channels = encoder.config.channels;
    deviceConfig.capture.pDeviceID = &pCaptureDeviceInfos[deviceId].id;
    deviceConfig.sampleRate       = encoder.config.sampleRate;
    deviceConfig.dataCallback     = data_callback;
    deviceConfig.pUserData        = &encoder;

    std::cout << "Initiazliing device" << std::endl;
    result = ma_device_init(&context, &deviceConfig, &device);
    if (result != MA_SUCCESS) 
    {
        printf("Failed to initialize capture device.\n");
        return -2;
    }

    std::cout << "Starting device" << std::endl;
    result = ma_device_start(&device);
    if (result != MA_SUCCESS) 
    {
        ma_device_uninit(&device);
        printf("Failed to start device.\n");
        return -3;
    }

    printf("Press Enter to stop recording...\n");
    getchar();
    
    ma_device_uninit(&device);
    ma_encoder_uninit(&encoder);
    ma_context_uninit(&context);

    return 0;
}