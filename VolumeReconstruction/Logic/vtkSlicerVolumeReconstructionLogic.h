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

/// Manages the logic associated with encoded video input/output.
#ifndef __vtkSlicerVolumeReconstructionLogic_h
#define __vtkSlicerVolumeReconstructionLogic_h

#include "vtkSlicerVolumeReconstructionModuleLogicExport.h"

// Slicer includes
#include <vtkSlicerBaseLogic.h>
#include <vtkSlicerModuleLogic.h>

// VTK includes
#include <vtkCallbackCommand.h>

// Sequences MRML includes
#include <vtkMRMLSequenceBrowserNode.h>

class vtkMRMLIGTLConnectorNode;
class vtkMRMLScalarVolumeNode;
class vtkMRMLAnnotationROINode;

/// \ingroup Slicer_QtModules_VolumeReconstruction
class VTK_SLICER_VOLUMERECONSTRUCTION_MODULE_LOGIC_EXPORT vtkSlicerVolumeReconstructionLogic : public vtkSlicerModuleLogic
{
public:
  static vtkSlicerVolumeReconstructionLogic* New();
  vtkTypeMacro(vtkSlicerVolumeReconstructionLogic, vtkSlicerModuleLogic);
  void PrintSelf(ostream&, vtkIndent) override;

  void RegisterNodes() override;

  void ReconstructVolume(
    vtkMRMLSequenceBrowserNode* inputSequenceBrowser,
    vtkMRMLScalarVolumeNode* outputVolumeNode,
    vtkMRMLAnnotationROINode* roiNode,
    double outputSpacing[3],
    int interpolationMode,
    int optimizationMode,
    int compoundingMode,
    bool fillHoles,
    int numberOfThreads);

  //----------------------------------------------------------------
  // Events
  //----------------------------------------------------------------

  //----------------------------------------------------------------
  // Connector and device Management
  //----------------------------------------------------------------

  //----------------------------------------------------------------
  // MRML Management
  //----------------------------------------------------------------

protected:

  //----------------------------------------------------------------
  // Constructor, destructor etc.
  //----------------------------------------------------------------

  vtkSlicerVolumeReconstructionLogic();
  virtual ~vtkSlicerVolumeReconstructionLogic();

private:

private:
  class vtkInternal;
  vtkInternal* Internal;

  vtkSlicerVolumeReconstructionLogic(const vtkSlicerVolumeReconstructionLogic&); // Not implemented
  void operator=(const vtkSlicerVolumeReconstructionLogic&);               // Not implemented
};

#endif
