#include "fake_andorlib.h"
#include <stdlib.h>

unsigned int AbortAcquisition(void)
{
    return DRV_NOT_INITIALIZED;
}
unsigned int GetCapabilities(AndorCapabilities *caps)
{
    return DRV_NOT_INITIALIZED;
}
unsigned int GetEMGainRange(int *l, int *h)
{
    return DRV_NOT_INITIALIZED;
}
unsigned int GetMaximumExposure(float *m)
{
    return DRV_NOT_INITIALIZED;
}
unsigned int GetMaximumBinning(int r, int h, int *m)
{
    return DRV_NOT_INITIALIZED;
}
unsigned int GetAcquiredData(at_32 * array, at_u32 size)
{
    return DRV_NOT_INITIALIZED;
}
unsigned int GetAcquiredData16(unsigned short int * array, at_u32 size)
{
    return DRV_NOT_INITIALIZED;
}
unsigned int GetDetector(int* xpixels, int* ypixels)
{
    return DRV_NOT_INITIALIZED;
}
unsigned int Initialize(char * dir)
{
  return DRV_ERROR_NOCAMERA;
}
unsigned int GetAcquisitionTimings(float* exposure, float* accumulate, float* kinetic)
{
    return DRV_NOT_INITIALIZED;
}
unsigned int GetAvailableCameras(at_32* totalCameras)
{
  *totalCameras=0;
  return DRV_SUCCESS;
}
unsigned int GetCameraHandle(at_32 cameraIndex, at_32* cameraHandle)
{
    return DRV_NOT_INITIALIZED;
}
unsigned int GetStatus(int* status)
{
    return DRV_NOT_INITIALIZED;
}
unsigned int ShutDown(void)
{
    return DRV_NOT_INITIALIZED;
}
unsigned int StartAcquisition(void)
{
    return DRV_NOT_INITIALIZED;
}
unsigned int SetShutter(int type, int mode, int closingtime, int openingtime)
{
    return DRV_NOT_INITIALIZED;
}
unsigned int SetExposureTime(float time)
{
    return DRV_NOT_INITIALIZED;
}
unsigned int SetImage(int hbin, int vbin, int hstart, int hend, int vstart, int vend)
{
    return DRV_NOT_INITIALIZED;
}
unsigned int SetTriggerMode(int mode)
{
    return DRV_NOT_INITIALIZED;
}
unsigned int SetAcquisitionMode(int mode)
{
    return DRV_NOT_INITIALIZED;
}
unsigned int SetReadMode(int mode)
{
    return DRV_NOT_INITIALIZED;
}
unsigned int GetCurrentCamera(at_32* cameraHandle)
{
    return DRV_NOT_INITIALIZED;
}
unsigned int SaveAsRaw(char* szFileTitle, int type)
{
    return DRV_NOT_INITIALIZED;
}
unsigned int SetCurrentCamera(at_32 cameraHandle)
{
    return DRV_NOT_INITIALIZED;
}
unsigned int SetEMCCDGain(int gain)
{
    return DRV_NOT_INITIALIZED;
}
unsigned int SetEMGainMode(int mode)
{
    return DRV_NOT_INITIALIZED;
}
