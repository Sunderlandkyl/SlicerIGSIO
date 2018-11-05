/*=auto=========================================================================

Portions (c) Copyright 2009 Brigham and Women's Hospital (BWH) All Rights Reserved.

See Doc/copyright/copyright.txt
or http://www.slicer.org/copyright/copyright.txt for details.

Program:   3D Slicer
Module:    $RCSfile: vtkVP9VolumeCodec.h,v $
Date:      $Date: 2006/03/19 17:12:29 $
Version:   $Revision: 1.3 $

=========================================================================auto=*/

// SlicerIGSIO includes
#include "vtkVP9VolumeCodec.h"

// VTK includes
#include <vtkMatrix4x4.h>

// vtksys includes
#include <vtksys\SystemTools.hxx>

// libvpx includes
#include <vp8cx.h>
#include <vp8dx.h>
#include <vpx_codec.h>
#include <vpx_decoder.h>
#include <vpx_image.h>
#include <vpx_integer.h>

vtkCodecNewMacro(vtkVP9VolumeCodec);

#ifdef __cplusplus
extern "C" {
#endif
  typedef struct VpxCodecInterface
  {
    vpx_codec_iface_t *(*const codec_interface)();
  } VpxCodecInterface;
#ifdef __cplusplus
} /* extern "C" */
#endif

#ifdef __cplusplus
extern "C" {
#endif
  typedef struct VpxInterfaceEncoder {
    vpx_codec_iface_t *(*const codec_interface)();
  } VpxInterfaceEncoder;

#ifdef __cplusplus
} /* extern "C" */
#endif

static const VpxCodecInterface VPXStaticCodecInterface[] = { { &vpx_codec_vp9_dx } };
static const VpxInterfaceEncoder VPXStaticCodecEncodeInterface[] = { { &vpx_codec_vp9_cx } };

// -------------------------------------------------------------------------- -
class vtkVP9VolumeCodec::vtkInternal
{
public:
  //---------------------------------------------------------------------------
  vtkInternal(vtkVP9VolumeCodec* external);
  virtual ~vtkInternal();

  vtkVP9VolumeCodec* External;

  const VpxCodecInterface* Decoder;
  const VpxInterfaceEncoder* Encoder;

  // Decoder
  vpx_codec_dec_cfg_t VPXDecodeConfiguration;
  vpx_codec_ctx_t VPXDecodeContext;
  vpx_codec_iter_t VPXDecodeIter;
  vpx_image_t* VPXDecodeImage;

  // Encoder
  vpx_codec_enc_cfg_t VPXEncodeConfiguration;
  vpx_codec_ctx_t* VPXEncodeContext;
  vpx_image_t* VPXEncodeImage;
  vpx_codec_iter_t VPXEncodeIter;
  vpx_fixed_buf_t *VPXEncodeBuffer;
  const vpx_codec_cx_pkt_t *VPXEncodePacket;

  // Encoder parameters
  bool Lossless;
  unsigned int MinimumKeyFrameDistance;
  unsigned int MaximumKeyFrameDistance;
  int BitRate;
  int Speed;
  int DeadlineMode;

  // Common functions
  int vpx_img_plane_width(const vpx_image_t *img, int plane);
  int vpx_img_plane_height(const vpx_image_t *img, int plane);
  void error_output(vpx_codec_ctx_t *ctx, const char *s);
  bool ConvertRGBToYUV(unsigned char* RGBFrame, unsigned char* YUVFrame, int width, int height);

  // Decoder functions
  bool InitializeDecoder();
  bool DecodeFrame(vtkStreamingVolumeFrame* inputFrame, vtkImageData* outputImageData, bool saveDecodedImage);
  void ComposeByteSteam(unsigned char** inputData, int dimension[2], int iStride[3], unsigned char* outputFrame);
  bool ConvertYUVToRGB(unsigned char* YUVFrame, unsigned char* RGBFrame, int width, int height);

  // Encoder functions
  bool EncoderInitialized;
  bool InitializeEncoder();
  bool EncodeFrame(vtkImageData* inputImageData, vtkStreamingVolumeFrame* outputFrame, bool forceKeyFrame);
  bool ConvertToLocalImageFormat(vtkImageData* imageData);

  bool SetSpeed(int speed);

  int ImageWidth;
  int ImageHeight;
  vtkSmartPointer<vtkImageData> YUVImage;
  vtkSmartPointer<vtkStreamingVolumeFrame> LastEncodedFrame;
};

//---------------------------------------------------------------------------
vtkVP9VolumeCodec::vtkVP9VolumeCodec()
  : Internal(new vtkInternal(this))
{
  this->AvailiableParameterNames.push_back(this->GetLosslessEncodingParameter());
  this->AvailiableParameterNames.push_back(this->GetMinimumKeyFrameDistanceParameter());
  this->AvailiableParameterNames.push_back(this->GetMaximumKeyFrameDistanceParameter());
  this->AvailiableParameterNames.push_back(this->GetSpeedParameter());
  this->AvailiableParameterNames.push_back(this->GetBitRateParameter());

  this->SetParameter(this->GetLosslessEncodingParameter(), "1");
  this->SetParameter(this->GetMinimumKeyFrameDistanceParameter(), "10");
  this->SetParameter(this->GetMaximumKeyFrameDistanceParameter(), "50");
  this->SetParameter(this->GetSpeedParameter(), "8");
}

//---------------------------------------------------------------------------
vtkVP9VolumeCodec::~vtkVP9VolumeCodec()
{
  delete this->Internal;
  this->Internal = NULL;
}

//---------------------------------------------------------------------------
std::string vtkVP9VolumeCodec::GetParameterDescription(std::string parameterName)
{
  if (parameterName == this->GetMinimumKeyFrameDistanceParameter())
  {
    return "Minimum distance between key frames";
  }

  if (parameterName == this->GetMaximumKeyFrameDistanceParameter())
  {
    return "Maximum distance between key frames";
  }

  if (parameterName == this->GetBitRateParameter())
  {
    return "Encoding bitrate (Bits per second)";
  }

  if (parameterName == this->GetSpeedParameter())
  {
    return "Encoding speed (-8..8 Slowest..Fastest)";
  }

  if (parameterName == this->GetLosslessEncodingParameter())
  {
    return "Lossless encoding flag";
  }

  return "";
}

//---------------------------------------------------------------------------
bool vtkVP9VolumeCodec::UpdateParameterInternal(std::string parameterName, std::string parameterValue)
{
  vtkVariant inputParameter = vtkVariant(parameterValue);
  std::string lowerParameterValue = vtksys::SystemTools::LowerCase(parameterValue);

  if (parameterName == this->GetMinimumKeyFrameDistanceParameter())
  {
    vtkVariant minKeyFrameDistanceVariant = vtkVariant(inputParameter);
    bool valid = true;
    int minKeyFrameDistance = minKeyFrameDistanceVariant.ToInt(&valid);
    if (!valid)
    {
      return false;
    }
    if (this->Internal->MinimumKeyFrameDistance == minKeyFrameDistance)
    {
      return true;
    }
    this->Internal->MinimumKeyFrameDistance = minKeyFrameDistance;
  }
  else if (parameterName == this->GetMaximumKeyFrameDistanceParameter())
  {
    vtkVariant maxKeyFrameDistanceVariant = vtkVariant(inputParameter);
    bool valid = true;
    int maxKeyFrameDistance = maxKeyFrameDistanceVariant.ToInt(&valid);
    if (!valid)
    {
      return false;
    }
    if (this->Internal->MaximumKeyFrameDistance == maxKeyFrameDistance)
    {
      return true;
    }
    this->Internal->MaximumKeyFrameDistance = maxKeyFrameDistance;
  }
  else if (parameterName == this->GetBitRateParameter())
  {
    vtkVariant bitRateVariant = vtkVariant(inputParameter);
    bool valid = true;
    int bitRate = bitRateVariant.ToInt(&valid);
    if (!valid)
    {
      return false;
    }
    if (this->Internal->BitRate == bitRate)
    {
      return true;
    }
    this->Internal->BitRate = bitRate;
  }
  else if (parameterName == this->GetLosslessEncodingParameter())
  {
    vtkVariant losslessVariant = vtkVariant(inputParameter);
    bool valid = true;
    bool lossless = (losslessVariant.ToInt(&valid) == 1);
    if (!valid)
    {
      return false;
    }
    if (this->Internal->Lossless == lossless)
    {
      return true;
    }
    this->Internal->Lossless = lossless;
    if (lossless)
    {
      this->Internal->DeadlineMode = VPX_DL_BEST_QUALITY;
    }
    else
    {
      this->Internal->DeadlineMode = VPX_DL_REALTIME;
    }
  }
  else if (parameterName == this->GetSpeedParameter())
  {
    vtkVariant speedVariant = vtkVariant(inputParameter);
    bool valid = true;
    int speed = speedVariant.ToInt(&valid);
    if (!valid)
    {
      return false;
    }
    if (this->Internal->Speed == speed)
    {
      return true;
    }
    this->Internal->Speed = speed;
  }
  else
  {
    vtkErrorMacro("Unknown parameter!");
    return false;
  }

  this->Internal->EncoderInitialized = false;
  return true;
}

//---------------------------------------------------------------------------
bool vtkVP9VolumeCodec::DecodeFrameInternal(vtkStreamingVolumeFrame* inputFrame, vtkImageData* outputImageData, bool saveDecodedImage/*=true*/)
{
  if (!inputFrame || !outputImageData)
  {
    vtkErrorMacro("Incorrect arguments!");
    return false;
  }

  return this->Internal->DecodeFrame(inputFrame, outputImageData, saveDecodedImage);
}

//---------------------------------------------------------------------------
bool vtkVP9VolumeCodec::EncodeImageDataInternal(vtkImageData* inputImageData, vtkStreamingVolumeFrame* outputFrame, bool forceKeyFrame)
{
  if (!inputImageData || !outputFrame)
  {
    vtkErrorMacro("Incorrect arguments!");
    return false;
  }
  return this->Internal->EncodeFrame(inputImageData, outputFrame, forceKeyFrame);
}

//---------------------------------------------------------------------------
void vtkVP9VolumeCodec::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
// vtkInternal methods

//---------------------------------------------------------------------------
vtkVP9VolumeCodec::vtkInternal::vtkInternal(vtkVP9VolumeCodec* external)
  : External(external)
  , ImageWidth(0)
  , ImageHeight(0)
  , LastEncodedFrame(NULL)
  , YUVImage(NULL)
  , Lossless(false)
  , MinimumKeyFrameDistance(250)
  , MaximumKeyFrameDistance(500)
  , BitRate(-1)
  , DeadlineMode(VPX_DL_REALTIME)
{
  // Decode
  this->Decoder = &VPXStaticCodecInterface[0];
  vpx_codec_dec_init(&this->VPXDecodeContext, this->Decoder->codec_interface(), NULL, 0);

  // Encode
  this->Encoder = &VPXStaticCodecEncodeInterface[0];
  this->VPXEncodeContext = new vpx_codec_ctx_t();
  this->VPXEncodeBuffer = new vpx_fixed_buf_t();
  this->VPXEncodeImage = new vpx_image_t(); 
  
  if (vpx_codec_enc_config_default(this->Encoder->codec_interface(), &this->VPXEncodeConfiguration, 0))
  {
    vtkErrorWithObjectMacro(this->External, "Failed to get default codec config.");
  }
}

//---------------------------------------------------------------------------
vtkVP9VolumeCodec::vtkInternal::~vtkInternal()
{
  vpx_codec_destroy(&this->VPXDecodeContext);
  vpx_codec_encode(this->VPXEncodeContext, NULL, -1, 1, 0, 0); //Flush the codec
  vpx_codec_destroy(this->VPXEncodeContext);
  if (this->VPXEncodeContext)
  {
    delete this->VPXEncodeContext;
  }
  if (this->VPXEncodeBuffer)
  {
    delete this->VPXEncodeBuffer;
  }
  if (this->VPXEncodeImage)
  {
    delete this->VPXEncodeImage;
  }
  this->Decoder = NULL;
  this->Encoder = NULL;
}

// TODO(dkovalev): move this function to vpx_image.{c, h}, so it will be part
// of vpx_image_t support, this section will be removed when it is moved to vpx_image
//---------------------------------------------------------------------------
int vtkVP9VolumeCodec::vtkInternal::vpx_img_plane_width(const vpx_image_t *img, int plane) {
  if (plane > 0 && img->x_chroma_shift > 0)
    return (img->d_w + 1) >> img->x_chroma_shift;
  else
    return img->d_w;
}

//---------------------------------------------------------------------------
int vtkVP9VolumeCodec::vtkInternal::vpx_img_plane_height(const vpx_image_t *img, int plane) {
  if (plane > 0 && img->y_chroma_shift > 0)
    return (img->d_h + 1) >> img->y_chroma_shift;
  else
    return img->d_h;
}

//---------------------------------------------------------------------------
bool vtkVP9VolumeCodec::vtkInternal::InitializeEncoder()
{
  if (this->ImageWidth != this->VPXEncodeConfiguration.g_w ||
      this->ImageHeight != this->VPXEncodeConfiguration.g_h)
  {
    this->EncoderInitialized = false;
  }

  if (this->EncoderInitialized)
  {
    return true;
  }

  this->VPXEncodeConfiguration.g_lag_in_frames = 0;
  this->VPXEncodeConfiguration.g_w = this->ImageWidth;
  this->VPXEncodeConfiguration.g_h = this->ImageHeight;
  vpx_img_alloc(
    this->VPXEncodeImage,
    VPX_IMG_FMT_I420,
    this->VPXEncodeConfiguration.g_w,
    this->VPXEncodeConfiguration.g_h,
    1);

  long flags = 0;
  if (vpx_codec_enc_init(this->VPXEncodeContext, this->Encoder->codec_interface(), &this->VPXEncodeConfiguration, flags))
  {
    vtkErrorWithObjectMacro(this->External, "Failed to initialize encoder");
    return false;
  }

  if (vpx_codec_enc_config_set(this->VPXEncodeContext, &this->VPXEncodeConfiguration))
  {
    //vtkErrorMacro("Failed to set parameter: " << parameterName);
    return false;
  }

  if (this->BitRate > 0)
  {
    int bitRateInKilo = this->BitRate / 1000;
    this->VPXEncodeConfiguration.rc_target_bitrate = bitRateInKilo;
    this->VPXEncodeConfiguration.layer_target_bitrate[0] = bitRateInKilo;
    for (unsigned int i = 0; i < this->VPXEncodeConfiguration.ss_number_layers; i++)
    {
      this->VPXEncodeConfiguration.ss_target_bitrate[i] =
        bitRateInKilo / this->VPXEncodeConfiguration.ss_number_layers;
    }
  }
  this->VPXEncodeConfiguration.kf_min_dist = this->MinimumKeyFrameDistance;
  this->VPXEncodeConfiguration.kf_max_dist = this->MaximumKeyFrameDistance;
  this->VPXEncodeConfiguration.g_error_resilient = true;

  if (vpx_codec_control(this->VPXEncodeContext, VP8E_SET_CPUUSED, this->Speed))
  {
    vtkErrorWithObjectMacro(this->External, "Failed to set lossless mode");
    return false;
  }

  if (vpx_codec_control_(this->VPXEncodeContext, VP9E_SET_LOSSLESS, this->Lossless))
  {
    vtkErrorWithObjectMacro(this->External, "Failed to set lossless mode");
    return false;
  }

  this->EncoderInitialized = true;
  return true;
}

//---------------------------------------------------------------------------
bool vtkVP9VolumeCodec::vtkInternal::InitializeDecoder()
{

  return true;
}

//---------------------------------------------------------------------------
bool vtkVP9VolumeCodec::vtkInternal::SetSpeed(int speed)
{
  //this->SetDeadlineMode(VPX_DL_REALTIME);
  //this->codecSpeed = speed;
  //if (speed >= SlowestSpeed && speed <= FastestSpeed)
  //{
  if (vpx_codec_control(this->VPXEncodeContext, VP8E_SET_CPUUSED, speed))
  {
    error_output(this->VPXEncodeContext, "Failed to set Speed"); //TODO
    return false;
  }

  return true;
}

//---------------------------------------------------------------------------
bool vtkVP9VolumeCodec::vtkInternal::DecodeFrame(vtkStreamingVolumeFrame* inputFrame, vtkImageData* outputImageData, bool saveDecodedImage/*=true*/)
{
  if (!inputFrame || !outputImageData)
  {
    vtkErrorWithObjectMacro(this->External, "Incorrect arguments!");
    return false;
  }

  if (!this->YUVImage)
  {
    this->YUVImage = vtkSmartPointer<vtkImageData>::New();
  }

  int dimensions[3] = { 0,0,0 };
  inputFrame->GetDimensions(dimensions);

  unsigned int numberOfVoxels = dimensions[0] * dimensions[1] * dimensions[2];
  if (numberOfVoxels == 0)
  {
    vtkErrorWithObjectMacro(this->External, "Cannot decode frame, number of voxels is zero");
    return false;
  }

  vtkUnsignedCharArray* frameData = inputFrame->GetFrameData();
  if (!frameData)
  {
    vtkErrorWithObjectMacro(this->External, "Frame data missing!");
    return false;
  }
  void* framePointer = frameData->GetPointer(0);

  this->YUVImage->SetDimensions(dimensions[0], dimensions[1] * 3 / 2, 1); //TODO: ONLY VALID UNLESS GREYSCALE
  this->YUVImage->AllocateScalars(VTK_UNSIGNED_CHAR, 1);
  void* yuvPointer = this->YUVImage->GetScalarPointer();

  unsigned int size = frameData->GetSize() * frameData->GetElementComponentSize();
  int frameSize[3] = { dimensions[0], dimensions[1], dimensions[2] };

  // Convert compressed frame to YUV image
  if (!vpx_codec_decode(&this->VPXDecodeContext, (unsigned char*)framePointer, (unsigned int)size, NULL, 0))
  {
    if (!saveDecodedImage)
    {
      return true;
    }

    this->VPXDecodeIter = NULL;
    if ((this->VPXDecodeImage = vpx_codec_get_frame(&this->VPXDecodeContext, &this->VPXDecodeIter)) != NULL)
    {
      int stride[3] = { this->VPXDecodeImage->stride[0], this->VPXDecodeImage->stride[1], this->VPXDecodeImage->stride[2] };

      int convertedDimensions[2] = { this->vpx_img_plane_width(this->VPXDecodeImage, 0) *
        ((this->VPXDecodeImage->fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 2 : 1), this->vpx_img_plane_height(this->VPXDecodeImage, 0) };
      this->ComposeByteSteam(this->VPXDecodeImage->planes, dimensions, stride, (unsigned char*)yuvPointer);
    }
  }
  else
  {
    vpx_codec_dec_init(&this->VPXDecodeContext, this->Decoder->codec_interface(), NULL, 0);
    vtkErrorWithObjectMacro(this->External, "VP9: Could not decode frame!");
    return false;
  }

  // Convert YUV image to RGB image
  if (saveDecodedImage)
  {
    void* imagePointer = outputImageData->GetScalarPointer();
    this->ConvertYUVToRGB((unsigned char*)yuvPointer, (unsigned char*)imagePointer, dimensions[0], dimensions[1]);
  }

  return true;
}

//---------------------------------------------------------------------------
bool vtkVP9VolumeCodec::vtkInternal::ConvertYUVToRGB(unsigned char* yuvFrame, unsigned char* rgbFrame, int width, int height)
{
  int componentLength = height*width;
  const unsigned char* srcY = yuvFrame;
  const unsigned char* srcU = yuvFrame + componentLength;
  const unsigned char* srcV = yuvFrame + componentLength * 5 / 4;
  unsigned char* YUV444 = new unsigned char[componentLength * 3];
  unsigned char* dstY = YUV444;
  unsigned char* dstU = dstY + componentLength;
  unsigned char* dstV = dstU + componentLength;

  memcpy(dstY, srcY, componentLength);
  const int halfHeight = height / 2;
  const int halfWidth = width / 2;

#pragma omp parallel for default(none) shared(dstV,dstU,srcV,srcU,iWidth)
  for (int y = 0; y < halfHeight; y++)
  {
    for (int x = 0; x < halfWidth; x++)
    {
      dstU[2 * x + 2 * y*width] = dstU[2 * x + 1 + 2 * y*width] = srcU[x + y*width / 2];
      dstV[2 * x + 2 * y*width] = dstV[2 * x + 1 + 2 * y*width] = srcV[x + y*width / 2];
    }
    memcpy(&dstU[(2 * y + 1)*width], &dstU[(2 * y)*width], width);
    memcpy(&dstV[(2 * y + 1)*width], &dstV[(2 * y)*width], width);
  }


  const int yOffset = 16;
  const int cZero = 128;
  int yMult, rvMult, guMult, gvMult, buMult;
  yMult = 298;
  rvMult = 409;
  guMult = -100;
  gvMult = -208;
  buMult = 517;

  static unsigned char clp_buf[384 + 256 + 384];
  static unsigned char *clip_buf = clp_buf + 384;

  // initialize clipping table
  memset(clp_buf, 0, 384);

  for (int i = 0; i < 256; i++)
  {
    clp_buf[384 + i] = i;
  }
  memset(clp_buf + 384 + 256, 255, 384);


#pragma omp parallel for default(none) shared(dstY,dstU,dstV,RGBFrame,yMult,rvMult,guMult,gvMult,buMult,clip_buf,componentLength)// num_threads(2)
  for (int i = 0; i < componentLength; ++i)
  {
    const int Y_tmp = ((int)dstY[i] - yOffset) * yMult;
    const int U_tmp = (int)dstU[i] - cZero;
    const int V_tmp = (int)dstV[i] - cZero;

    const int R_tmp = (Y_tmp + V_tmp * rvMult) >> 8;//32 to 16 bit conversion by left shifting
    const int G_tmp = (Y_tmp + U_tmp * guMult + V_tmp * gvMult) >> 8;
    const int B_tmp = (Y_tmp + U_tmp * buMult) >> 8;

    rgbFrame[3 * i] = clip_buf[R_tmp];
    rgbFrame[3 * i + 1] = clip_buf[G_tmp];
    rgbFrame[3 * i + 2] = clip_buf[B_tmp];
  }
  delete[] YUV444;
  YUV444 = NULL;
  dstY = NULL;
  dstU = NULL;
  dstV = NULL;
  return true;
}

//---------------------------------------------------------------------------
bool vtkVP9VolumeCodec::vtkInternal::ConvertRGBToYUV(unsigned char* rgbFrame, unsigned char* yuvFrame, int width, int height)
{
  size_t image_size = width * height;
  size_t upos = image_size;
  size_t vpos = upos + upos / 4;
  size_t i = 0;

  for (size_t line = 0; line < height; ++line)
  {
    if (!(line % 2))
    {
      for (size_t x = 0; x < width; x += 2)
      {
        unsigned char r = rgbFrame[3 * i];
        unsigned char g = rgbFrame[3 * i + 1];
        unsigned char b = rgbFrame[3 * i + 2];

        yuvFrame[i++] = ((66 * r + 129 * g + 25 * b) >> 8) + 16;

        yuvFrame[upos++] = ((-38 * r - 74 * g + 112 * b) >> 8) + 128;
        yuvFrame[vpos++] = ((112 * r - 94 * g - 18 * b) >> 8) + 128;

        r = rgbFrame[3 * i];
        g = rgbFrame[3 * i + 1];
        b = rgbFrame[3 * i + 2];

        yuvFrame[i++] = ((66 * r + 129 * g + 25 * b) >> 8) + 16;
      }
    }
    else
    {
      for (size_t x = 0; x < width; x += 1)
      {
        unsigned char r = rgbFrame[3 * i];
        unsigned char g = rgbFrame[3 * i + 1];
        unsigned char b = rgbFrame[3 * i + 2];

        yuvFrame[i++] = ((66 * r + 129 * g + 25 * b) >> 8) + 16;
      }
    }
  }
  return true;
}

//---------------------------------------------------------------------------
void vtkVP9VolumeCodec::vtkInternal::ComposeByteSteam(unsigned char** inputData, int dimension[2], int iStride[3], unsigned char* outputFrame)
{
  int plane;
  int dimensionW[3] = { dimension[0],dimension[0] / 2,dimension[0] / 2 };
  int dimensionH[3] = { dimension[1],dimension[1] / 2,dimension[1] / 2 };
  int shift = 0;
  for (plane = 0; plane < 3; ++plane)
  {
    const unsigned char *buf = inputData[plane];
    const int stride = iStride[plane];
    int w = dimensionW[plane];
    int h = dimensionH[plane];
    int y;
    for (y = 0; y < h; ++y)
    {
      memcpy(outputFrame + shift + w*y, buf, w);
      buf += stride;
    }
    shift += w*h;
  }
}

//---------------------------------------------------------------------------
bool vtkVP9VolumeCodec::vtkInternal::EncodeFrame(vtkImageData* inputImageData, vtkStreamingVolumeFrame* outputFrame, bool forceKeyFrame)
{
  if (!inputImageData || !outputFrame)
  {
    vtkErrorWithObjectMacro(this->External, "Incorrect arguments!");
    return false;
  }

  int dimensions[3] = { 0,0,0 };
  inputImageData->GetDimensions(dimensions);
  this->ImageWidth = dimensions[0];
  this->ImageHeight = dimensions[1];
  this->InitializeEncoder();

  if (!this->YUVImage)
  {
    this->YUVImage = vtkSmartPointer<vtkImageData>::New();
  }
  this->YUVImage->SetDimensions(dimensions[0], dimensions[1] * 3 / 2, 1); //TODO: ONLY VALID UNLESS GREYSCALE
  this->YUVImage->AllocateScalars(VTK_UNSIGNED_CHAR, 1);
  unsigned char* rgbPointer = (unsigned char*)inputImageData->GetScalarPointer();
  unsigned char* yuvPointer = (unsigned char*)this->YUVImage->GetScalarPointer();
  this->ConvertRGBToYUV(rgbPointer, yuvPointer, this->ImageWidth, this->ImageHeight);

  static int timestamp = 0;

  long flags = 0;
  if (forceKeyFrame)
  {
    flags |= VPX_EFLAG_FORCE_KF;
  }

  this->ConvertToLocalImageFormat(this->YUVImage);
  vpx_codec_err_t errorCode = vpx_codec_encode(
    this->VPXEncodeContext,
    this->VPXEncodeImage,
    timestamp,
    1,
    flags,
    VPX_DL_GOOD_QUALITY // VPX_DL_REALTIME   VPX_DL_GOOD_QUALITY   VPX_DL_BEST_QUALITY
  );
  ++timestamp;

  if (errorCode != VPX_CODEC_OK)
  {
    vtkErrorWithObjectMacro(this->External, "VP9: Could not encode frame!");
    error_output(this->VPXEncodeContext, "Failed to encode frame"); //TODO
    return false;
  }

  this->VPXEncodeIter = NULL;
  while ((this->VPXEncodePacket = vpx_codec_get_cx_data(this->VPXEncodeContext, &this->VPXEncodeIter)) != NULL)
  {
    if (this->VPXEncodePacket->kind == VPX_CODEC_CX_FRAME_PKT)
    {
      if ((this->VPXEncodePacket->data.frame.flags & VPX_FRAME_IS_KEY) != 0)
      {
        outputFrame->SetFrameType(vtkStreamingVolumeFrame::IFrame);
        outputFrame->SetPreviousFrame(NULL);
      }
      else
      {
        outputFrame->SetFrameType(vtkStreamingVolumeFrame::PFrame);
        outputFrame->SetPreviousFrame(this->LastEncodedFrame);
      }

      vtkSmartPointer<vtkUnsignedCharArray> frameData = vtkSmartPointer<vtkUnsignedCharArray>::New();
      frameData->Allocate(this->VPXEncodePacket->data.frame.sz);
      memcpy(frameData->GetPointer(0),
             this->VPXEncodePacket->data.frame.buf,
             this->VPXEncodePacket->data.frame.sz);
      outputFrame->SetFrameData(frameData);
    }
  }

  outputFrame->SetCodecFourCC(this->External->GetFourCC());
  outputFrame->SetDimensions(dimensions);
  outputFrame->SetNumberOfComponents(3);
  this->LastEncodedFrame = outputFrame;

  return true;
}

//---------------------------------------------------------------------------
bool vtkVP9VolumeCodec::vtkInternal::ConvertToLocalImageFormat(vtkImageData* imageData)
{
  unsigned char* imagePointer = (unsigned char*)imageData->GetScalarPointer();
  int plane;
  for (plane = 0; plane < 3; ++plane)
  {
    if (plane == 1)
    {
      imagePointer += (this->ImageWidth*this->ImageHeight);
    }
    else if (plane == 2)
    {
      imagePointer += (this->ImageWidth*this->ImageHeight) >> 2;
    }
    unsigned char *buffer = this->VPXEncodeImage->planes[plane];
    const int w = vpx_img_plane_width(this->VPXEncodeImage, plane) *
      ((this->VPXEncodeImage->fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
    const int h = vpx_img_plane_height(this->VPXEncodeImage, plane);
    memcpy(buffer, imagePointer, w*h);
  }

  return true;
}

//---------------------------------------------------------------------------
void vtkVP9VolumeCodec::vtkInternal::error_output(vpx_codec_ctx_t *ctx, const char *s) {
  const char *detail = vpx_codec_error_detail(ctx);

  printf("%s: %s\n", s, vpx_codec_error(ctx));
  if (detail)
  {
    printf("    %s\n", detail);
  }
}