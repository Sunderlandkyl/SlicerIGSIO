/*==============================================================================

Copyright (c) Laboratory for Percutaneous Surgery (PerkLab)
Queen's University, Kingston, ON, Canada. All Rights Reserved.

See COPYRIGHT.txt
or http://www.slicer.org/copyright/copyright.txt for details.

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

This file was originally developed by Kyle Sunderland, PerkLab, Queen's University
and was supported through CANARIE's Research Software Program, and Cancer
Care Ontario.

==============================================================================*/

// Slicer includes
#include <vtkObjectFactory.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLColorLogic.h>

// MRML includes
#include <vtkMRMLVolumeNode.h>
#include <vtkMRMLStreamingVolumeNode.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLLinearTransformNode.h>
#include <vtkMRMLColorTableNode.h>
#include <vtkMRMLScalarVolumeNode.h>

// IGSIO Common includes
#include <vtkIGSIOTrackedFrameList.h>
#include <vtkIGSIOTransformRepository.h>

// IGSIO VolumeReconstructor includes
#include <vtkIGSIOVolumeReconstructor.h>

// SlicerIGSIOCommon includes
#include "vtkSlicerIGSIOCommon.h"

// Volumes includes
#include <vtkStreamingVolumeCodecFactory.h>

// VolumeReconstruction includes
#include "vtkSlicerVolumeReconstructionLogic.h"

// VTK includes
#include <vtkMatrix4x4.h>

//---------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerVolumeReconstructionLogic);

//---------------------------------------------------------------------------
class vtkSlicerVolumeReconstructionLogic::vtkInternal
{
public:
  //---------------------------------------------------------------------------
  vtkInternal(vtkSlicerVolumeReconstructionLogic* external);
  ~vtkInternal();

  vtkSlicerVolumeReconstructionLogic* External;
};

//----------------------------------------------------------------------------
// vtkInternal methods

//---------------------------------------------------------------------------
vtkSlicerVolumeReconstructionLogic::vtkInternal::vtkInternal(vtkSlicerVolumeReconstructionLogic* external)
  : External(external)
{
}

//---------------------------------------------------------------------------
vtkSlicerVolumeReconstructionLogic::vtkInternal::~vtkInternal()
{
}

//----------------------------------------------------------------------------
// vtkSlicerVolumeReconstructionLogic methods

//---------------------------------------------------------------------------
vtkSlicerVolumeReconstructionLogic::vtkSlicerVolumeReconstructionLogic()
{
  this->Internal = new vtkInternal(this);
}

//---------------------------------------------------------------------------
vtkSlicerVolumeReconstructionLogic::~vtkSlicerVolumeReconstructionLogic()
{
}

//-----------------------------------------------------------------------------
void vtkSlicerVolumeReconstructionLogic::RegisterNodes()
{
  if (this->GetMRMLScene() == NULL)
  {
    vtkErrorMacro("Scene is invalid");
    return;
  }
}
//---------------------------------------------------------------------------
void vtkSlicerVolumeReconstructionLogic::ReconstructVolume(vtkMRMLSequenceBrowserNode* inputSequenceBrowser,
  vtkMRMLScalarVolumeNode* outputVolumeNode,
  double outputSpacing[3],
  int interpolationMode,
  int optimizationMode,
  int compoundingMode,
  bool fillHoles,
  int numberOfThreads)
{
  if (!inputSequenceBrowser)
  {
    vtkErrorMacro("Invalid input sequence browser!");
    return;
  }

  if (!outputVolumeNode)
  {
    vtkErrorMacro("Invalid output volume node!");
    return;
  }

  vtkNew<vtkIGSIOTransformRepository> transformRepository;
  vtkNew<vtkMatrix4x4> imageToProbe;
  imageToProbe->SetElement(0, 0, 0);
  imageToProbe->SetElement(0, 1, 0.08);
  imageToProbe->SetElement(0, 2, 0);
  imageToProbe->SetElement(0, 3, 11);

  imageToProbe->SetElement(1, 0, -0.08);
  imageToProbe->SetElement(1, 1, 0);
  imageToProbe->SetElement(1, 2, 0);
  imageToProbe->SetElement(1, 3, 50);

  imageToProbe->SetElement(2, 0, 0);
  imageToProbe->SetElement(2, 1, 0);
  imageToProbe->SetElement(2, 2, 0.08);
  imageToProbe->SetElement(2, 3, 0);

  imageToProbe->SetElement(3, 0, 0);
  imageToProbe->SetElement(3, 1, 0);
  imageToProbe->SetElement(3, 2, 0);
  imageToProbe->SetElement(3, 3, 1);
  transformRepository->SetTransform(igsioTransformName("ImageToProbe"), imageToProbe, ToolStatus::TOOL_OK);

  vtkNew<vtkIGSIOTrackedFrameList> trackedFrameList;
  if (!vtkSlicerIGSIOCommon::SequenceBrowserToTrackedFrameList(inputSequenceBrowser, trackedFrameList))
  {
    vtkErrorMacro("Error converting sequence browser node to tracked frame list!");
    return;
  }

  vtkSmartPointer<vtkIGSIOVolumeReconstructor> reconstructor = vtkSmartPointer<vtkIGSIOVolumeReconstructor>::New();
  reconstructor->SetOutputSpacing(outputSpacing);
  reconstructor->SetCompoundingMode(vtkIGSIOPasteSliceIntoVolume::CompoundingType(compoundingMode));
  reconstructor->SetOptimization(vtkIGSIOPasteSliceIntoVolume::OptimizationType(optimizationMode));
  reconstructor->SetInterpolation(vtkIGSIOPasteSliceIntoVolume::InterpolationType(interpolationMode));
  reconstructor->SetNumberOfThreads(numberOfThreads);
  reconstructor->SetFillHoles(fillHoles);
  reconstructor->SetImageCoordinateFrame("Image"); // TODO
  reconstructor->SetReferenceCoordinateFrame("Reference"); // TODO
  
  

  
  int clipOrigin[2] = { 0,0 };
  reconstructor->SetClipRectangleOrigin(clipOrigin);
  int clipRectangleSize[2] = { 820, 616 };
  reconstructor->SetClipRectangleSize(clipRectangleSize);
  double origin[3] = { -15,-15,30 };
  reconstructor->SetOutputOrigin(origin);
  int extent[6] = { 0, 300, 0, 300, 0, 300 };
  reconstructor->SetOutputExtent(extent);

  LOG_INFO("Reconstruct volume...");
  const int numberOfFrames = trackedFrameList->GetNumberOfTrackedFrames();
  int numberOfFramesAddedToVolume = 0;

  std::string errorDetail;
  if (reconstructor->SetOutputExtentFromFrameList(trackedFrameList, transformRepository, errorDetail) != IGSIO_SUCCESS)
  {
    vtkErrorMacro("Failed to set output extent of volume! " << errorDetail);
    return;
  }

  for (int frameIndex = 0; frameIndex < numberOfFrames; frameIndex += reconstructor->GetSkipInterval())
  {
    vtkDebugMacro("Frame: " << frameIndex);
    vtkIGSIOLogger::PrintProgressbar((100.0 * frameIndex) / numberOfFrames);

    igsioTrackedFrame* frame = trackedFrameList->GetTrackedFrame(frameIndex);

    if (transformRepository->SetTransforms(*frame) != IGSIO_SUCCESS)
    {
      vtkErrorMacro("Failed to update transform repository with frame #" << frameIndex);
      continue;
    }

    // Insert slice for reconstruction
    bool insertedIntoVolume = false;
    if (reconstructor->AddTrackedFrame(frame, transformRepository, &insertedIntoVolume) != IGSIO_SUCCESS)
    {
      vtkErrorMacro("Failed to add tracked frame to volume with frame #" << frameIndex);
      continue;
    }

    if (insertedIntoVolume)
    {
      numberOfFramesAddedToVolume++;
    }

  }

  vtkIGSIOLogger::PrintProgressbar(100);

  trackedFrameList->Clear();

  vtkDebugMacro("Number of frames added to the volume: " << numberOfFramesAddedToVolume << " out of " << numberOfFrames);

  if (!outputVolumeNode->GetImageData())
  {
    vtkSmartPointer<vtkImageData> imageData = vtkSmartPointer<vtkImageData>::New();
    outputVolumeNode->SetAndObserveImageData(imageData);
  }

  if (reconstructor->GetReconstructedVolume(outputVolumeNode->GetImageData()) != IGSIO_SUCCESS)
  {
    vtkErrorMacro("Could not retreive reconstructed image");
  }

  //outputVolumeNode->GetImageData()->SetSpacing(0, 0, 0);
  //outputVolumeNode->SetSpacing(outputSpacing);

  //outputVolumeNode->GetImageData()->SetOrigin(0, 0, 0);
  //outputVolumeNode->SetOrigin(origin);
}

//---------------------------------------------------------------------------
void vtkSlicerVolumeReconstructionLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->vtkObject::PrintSelf(os, indent);
  os << indent << "vtkSlicerVolumeReconstructionLogic:             " << this->GetClassName() << "\n";
}
