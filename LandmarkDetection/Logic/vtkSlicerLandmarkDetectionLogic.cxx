/*==============================================================================

  Program: 3D Slicer

  Portions (c) Copyright Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

==============================================================================*/

// LandmarkDetection Logic includes
#include "vtkSlicerLandmarkDetectionLogic.h"

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLTransformNode.h>
#include <vtkMRMLMarkupsFiducialNode.h>

// VTK includes
#include <vtkIntArray.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>

// STD includes
#include <cassert>

// Landmark Detection MRML includes
#include <vtkMRMLLandmarkDetectionNode.h>

// IGSIO LandmarkDetection includes
#include <vtkIGSIOLandmarkDetectionAlgo.h>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerLandmarkDetectionLogic);

typedef std::map<vtkSmartPointer<vtkMRMLLandmarkDetectionNode>, vtkSmartPointer<vtkIGSIOLandmarkDetectionAlgo>> LandmarkDetectionMap;

//----------------------------------------------------------------------------
class vtkSlicerLandmarkDetectionLogic::vtkInternal
{
public:
  vtkInternal(vtkSlicerLandmarkDetectionLogic* external);
  ~vtkInternal();

  vtkSlicerLandmarkDetectionLogic* External;
  vtkIGSIOLandmarkDetectionAlgo* GetLandmarkDetectionAlgoFromNode(vtkMRMLLandmarkDetectionNode* landmarkDetectionNode);

  LandmarkDetectionMap LandmarkDetectionMap;
};

//----------------------------------------------------------------------------
// vtkInternal methods

//----------------------------------------------------------------------------
vtkSlicerLandmarkDetectionLogic::vtkInternal::vtkInternal(vtkSlicerLandmarkDetectionLogic* external)
  : External(external)
{
}

//----------------------------------------------------------------------------
vtkSlicerLandmarkDetectionLogic::vtkInternal::~vtkInternal()
{
}

//----------------------------------------------------------------------------
vtkIGSIOLandmarkDetectionAlgo* vtkSlicerLandmarkDetectionLogic::vtkInternal::GetLandmarkDetectionAlgoFromNode(vtkMRMLLandmarkDetectionNode* landmarkDetectionNode)
{
  auto landmarkDetectionIt = this->LandmarkDetectionMap.find(landmarkDetectionNode);
  if (landmarkDetectionIt == this->LandmarkDetectionMap.end())
  {
    return nullptr;
  }
  return landmarkDetectionIt->second;
}

//----------------------------------------------------------------------------
// vtkSlicerLandmarkDetectionLogic methods

//----------------------------------------------------------------------------
vtkSlicerLandmarkDetectionLogic::vtkSlicerLandmarkDetectionLogic()
{
  this->Internal = new vtkInternal(this);
}

//----------------------------------------------------------------------------
vtkSlicerLandmarkDetectionLogic::~vtkSlicerLandmarkDetectionLogic()
{
  delete this->Internal;
}

//----------------------------------------------------------------------------
void vtkSlicerLandmarkDetectionLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//---------------------------------------------------------------------------
void vtkSlicerLandmarkDetectionLogic::SetMRMLSceneInternal(vtkMRMLScene* newScene)
{
  vtkNew<vtkIntArray> events;
  events->InsertNextValue(vtkMRMLScene::NodeAddedEvent);
  events->InsertNextValue(vtkMRMLScene::NodeRemovedEvent);
  events->InsertNextValue(vtkMRMLScene::EndBatchProcessEvent);
  this->SetAndObserveMRMLSceneEventsInternal(newScene, events.GetPointer());
}

//-----------------------------------------------------------------------------
void vtkSlicerLandmarkDetectionLogic::RegisterNodes()
{
  if (this->GetMRMLScene() == NULL)
  {
    vtkErrorMacro("Scene is invalid");
    return;
  }
  this->GetMRMLScene()->RegisterNodeClass(vtkSmartPointer<vtkMRMLLandmarkDetectionNode>::New());
}

//---------------------------------------------------------------------------
void vtkSlicerLandmarkDetectionLogic::UpdateFromMRMLScene()
{
  assert(this->GetMRMLScene() != 0);
}

//---------------------------------------------------------------------------
void vtkSlicerLandmarkDetectionLogic::OnMRMLSceneNodeAdded(vtkMRMLNode* node)
{
  vtkMRMLLandmarkDetectionNode* landmarkDetectionNode = vtkMRMLLandmarkDetectionNode::SafeDownCast(node);
  if (!landmarkDetectionNode)
  {
    return;
  }
  vtkNew<vtkIntArray> events;
  events->InsertNextValue(vtkCommand::ModifiedEvent);
  events->InsertNextValue(vtkMRMLLandmarkDetectionNode::InputTransformModifiedEvent);
  events->InsertNextValue(vtkMRMLLandmarkDetectionNode::ResetDetectionEvent);
  vtkObserveMRMLNodeEventsMacro(landmarkDetectionNode, events);

  vtkNew<vtkIGSIOLandmarkDetectionAlgo> landmarkDetectionAlgo;
  this->Internal->LandmarkDetectionMap[landmarkDetectionNode] = vtkNew<vtkIGSIOLandmarkDetectionAlgo>();
}

//---------------------------------------------------------------------------
void vtkSlicerLandmarkDetectionLogic::OnMRMLSceneNodeRemoved(vtkMRMLNode* node)
{
  vtkMRMLLandmarkDetectionNode* landmarkDetectionNode = vtkMRMLLandmarkDetectionNode::SafeDownCast(node);
  if (!landmarkDetectionNode)
  {
    return;
  }
  vtkUnObserveMRMLNodeMacro(landmarkDetectionNode);
  this->Internal->LandmarkDetectionMap.erase(landmarkDetectionNode);
}

//---------------------------------------------------------------------------
void vtkSlicerLandmarkDetectionLogic::ProcessMRMLNodesEvents(vtkObject* caller, unsigned long event, void* callData)
{
  vtkMRMLLandmarkDetectionNode* landmarkDetectionNode = vtkMRMLLandmarkDetectionNode::SafeDownCast(caller);
  if (!landmarkDetectionNode)
  {
    return;
  }

  if (event == vtkCommand::ModifiedEvent)
  {
    this->UpdateLandmarkDetectionAlgo(landmarkDetectionNode);
  }
  else if (event == vtkMRMLLandmarkDetectionNode::InputTransformModifiedEvent && landmarkDetectionNode->GetLandmarkDetectionInProgress())
  {
    this->AddTransformToAlgo(landmarkDetectionNode);
  }
  else if (event == vtkMRMLLandmarkDetectionNode::ResetDetectionEvent)
  {
    this->ResetDetectionForNode(landmarkDetectionNode);
  }
}

//----------------------------------------------------------------------------
void vtkSlicerLandmarkDetectionLogic::ResetDetectionForNode(vtkMRMLLandmarkDetectionNode* landmarkDetectionNode)
{
  vtkIGSIOLandmarkDetectionAlgo* algo = this->Internal->GetLandmarkDetectionAlgoFromNode(landmarkDetectionNode);
  if (!algo)
  {
    vtkErrorMacro("ResetDetectionForNode: Could not find algorithm for specified node");
  }
  algo->ResetDetection();
}

//----------------------------------------------------------------------------
vtkMRMLLandmarkDetectionNode* vtkSlicerLandmarkDetectionLogic::AddLandmarkDetectionNode(vtkMRMLTransformNode* inputTransformNode, vtkMRMLMarkupsFiducialNode* outputFiducialNode)
{
  if (!this->GetMRMLScene())
  {
    vtkErrorMacro("Invalid MRML scene");
    return nullptr;
  }

  vtkSmartPointer<vtkMRMLLandmarkDetectionNode> landmarkDetectionNode =
    vtkMRMLLandmarkDetectionNode::SafeDownCast(this->GetMRMLScene()->AddNewNodeByClass("vtkMRMLLandmarkDetectionNode"));
  if (inputTransformNode)
  {
    landmarkDetectionNode->SetAndObserveInputTransformNode(inputTransformNode);
  }
  if (outputFiducialNode)
  {
    landmarkDetectionNode->SetAndObserveOutputMarkupsNode(outputFiducialNode);
  }
  return landmarkDetectionNode;
}

//----------------------------------------------------------------------------
void vtkSlicerLandmarkDetectionLogic::UpdateLandmarkDetectionAlgo(vtkMRMLLandmarkDetectionNode* landmarkDetectionNode)
{
  vtkIGSIOLandmarkDetectionAlgo* algo = this->Internal->GetLandmarkDetectionAlgoFromNode(landmarkDetectionNode);
  if (!algo)
  {
    vtkErrorMacro("UpdateLandmarkDetectionAlgo: Could not find algorithm for specified node");
  }
  algo->SetAcquisitionRate(landmarkDetectionNode->GetAcquisitionRateHz());
  algo->SetFilterWindowTimeSec(landmarkDetectionNode->GetFilterWindowTimeSec());
  algo->SetDetectionTimeSec(landmarkDetectionNode->GetDetectionTimeSec());
  algo->SetStylusShaftMinimumDisplacementThresholdMm(landmarkDetectionNode->GetStylusShaftMinimumDisplacementThresholdMm());
  algo->SetStylusTipMaximumDisplacementThresholdMm(landmarkDetectionNode->GetStylusTipMaximumDisplacementThresholdMm());
  algo->SetMinimunDistanceBetweenLandmarksMm(landmarkDetectionNode->GetStylusShaftMinimumDisplacementThresholdMm());
}

//----------------------------------------------------------------------------
void vtkSlicerLandmarkDetectionLogic::AddTransformToAlgo(vtkMRMLLandmarkDetectionNode* landmarkDetectionNode)
{
  vtkIGSIOLandmarkDetectionAlgo* algo = this->Internal->GetLandmarkDetectionAlgoFromNode(landmarkDetectionNode);
  if (!algo)
  {
    vtkErrorMacro("AddTransformToAlgo: Could not find algorithm for specified node");
    return;
  }

  vtkMRMLTransformNode* transformNode = landmarkDetectionNode->GetInputTransformNode();

  vtkMRMLMarkupsFiducialNode* outputFiducials = landmarkDetectionNode->GetOutputMarkupsNode();

  // Get a matrix from the stylus tip position to the markups node coordinate system.
  vtkNew<vtkMatrix4x4> stylusTipToReferenceMatrix;
  vtkMRMLTransformNode::GetMatrixTransformBetweenNodes(transformNode, outputFiducials->GetParentTransformNode(), stylusTipToReferenceMatrix);

  if (outputFiducials)
  {
    // Copy existing landmarks to the algorithm so that we reject the points that are near to existing ones.
    vtkNew<vtkPoints> definedControlPoints;
    for (int i = 0; i < outputFiducials->GetNumberOfControlPoints(); ++i)
    {
      if (outputFiducials->GetNthControlPointPositionStatus(i) != vtkMRMLMarkupsNode::PositionDefined)
      {
        continue;
      }
      definedControlPoints->InsertNextPoint(outputFiducials->GetNthControlPointPosition(i));
    }
    algo->GetDetectedLandmarkPoints_Reference()->DeepCopy(definedControlPoints);
  }

  int newLandmarkDetected = 0;
  if (algo->InsertNextStylusTipToReferenceTransform(stylusTipToReferenceMatrix, newLandmarkDetected) != IGSIO_SUCCESS)
  {
    vtkErrorMacro("AddTransformToAlgo: Could not add StylusTipToReferenceTransform");
    return;
  }

  if (newLandmarkDetected > 0)
  {
    this->UpdateDetectedLandmarks(landmarkDetectionNode);
  }  
}

//----------------------------------------------------------------------------
void vtkSlicerLandmarkDetectionLogic::UpdateDetectedLandmarks(vtkMRMLLandmarkDetectionNode* landmarkDetectionNode)
{
  vtkIGSIOLandmarkDetectionAlgo* algo = this->Internal->GetLandmarkDetectionAlgoFromNode(landmarkDetectionNode);
  if (!algo)
  {
    vtkErrorMacro("UpdateDetectedLandmarks: Could not find algorithm for specified node");
    return;
  }
  
  vtkPoints* detectedLandmarkPoints = algo->GetDetectedLandmarkPoints_Reference();

  vtkMRMLMarkupsFiducialNode* outputFiducials = landmarkDetectionNode->GetOutputMarkupsNode();
  MRMLNodeModifyBlocker blocker(outputFiducials);

  int detectedLandmarkIndex = 0;
  for (int i = 0; i < outputFiducials->GetNumberOfControlPoints() && detectedLandmarkIndex < detectedLandmarkPoints->GetNumberOfPoints(); ++i)
  {
    if (outputFiducials->GetNthControlPointPositionStatus(i) != vtkMRMLMarkupsNode::PositionDefined)
    {
      continue;
    }
    outputFiducials->SetNthControlPointPosition(i, detectedLandmarkPoints->GetPoint(i));
  }

  if (detectedLandmarkPoints->GetNumberOfPoints() == outputFiducials->GetMaximumNumberOfControlPoints())
  {
    landmarkDetectionNode->LandmarkDetectionInProgressOff();
  }
}
