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

#include <vtkMRMLLandmarkDetectionNode.h>

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLMarkupsFiducialNode.h>
#include <vtkMRMLTransformNode.h>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLLandmarkDetectionNode);

//----------------------------------------------------------------------------
vtkMRMLLandmarkDetectionNode::vtkMRMLLandmarkDetectionNode()
{
  vtkNew<vtkIntArray> inputTransformEvents;
  inputTransformEvents->InsertNextTuple1(vtkMRMLTransformNode::TransformModifiedEvent);
  this->AddNodeReferenceRole(this->GetInputTransformNodeReferenceRole(), this->GetInputTransformNodeReferenceMRMLAttributeName(), inputTransformEvents);
  this->AddNodeReferenceRole(this->GetOutputMarkupsNodeReferenceRole(), this->GetOutputMarkupsNodeReferenceMRMLAttributeName());
}

//----------------------------------------------------------------------------
vtkMRMLLandmarkDetectionNode::~vtkMRMLLandmarkDetectionNode() = default;

//----------------------------------------------------------------------------
void vtkMRMLLandmarkDetectionNode::WriteXML(ostream& of, int nIndent)
{
  Superclass::WriteXML(of, nIndent);
  vtkMRMLWriteXMLBeginMacro(of);
  vtkMRMLWriteXMLEndMacro();
}

//----------------------------------------------------------------------------
void vtkMRMLLandmarkDetectionNode::ReadXMLAttributes(const char** atts)
{
  MRMLNodeModifyBlocker blocker(this);
  Superclass::ReadXMLAttributes(atts);
  vtkMRMLReadXMLBeginMacro(atts);
  vtkMRMLReadXMLEndMacro();
}

//----------------------------------------------------------------------------
void vtkMRMLLandmarkDetectionNode::Copy(vtkMRMLNode* anode)
{
  MRMLNodeModifyBlocker blocker(this);
  Superclass::Copy(anode);
  vtkMRMLCopyBeginMacro(anode);
  vtkMRMLCopyEndMacro();
}

//----------------------------------------------------------------------------
void vtkMRMLLandmarkDetectionNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);
  vtkMRMLPrintBeginMacro(os, indent);
  vtkMRMLPrintEndMacro();
}

//----------------------------------------------------------------------------
void vtkMRMLLandmarkDetectionNode::ProcessMRMLEvents(vtkObject* caller, unsigned long eventID, void* callData)
{
  Superclass::ProcessMRMLEvents(caller, eventID, callData);
  if (!this->Scene)
  {
    vtkErrorMacro("ProcessMRMLEvents: Invalid MRML scene");
    return;
  }

  vtkMRMLTransformNode* transformNode = vtkMRMLTransformNode::SafeDownCast(caller);
  if (transformNode && transformNode == this->GetInputTransformNode())
  {
    this->InvokeCustomModifiedEvent(InputTransformModifiedEvent, transformNode);
  }
}

//----------------------------------------------------------------------------
void vtkMRMLLandmarkDetectionNode::SetAndObserveInputTransformNode(vtkMRMLTransformNode* node)
{
  this->SetNodeReferenceID(this->GetInputTransformNodeReferenceRole(), (node ? node->GetID() : NULL));
}

//----------------------------------------------------------------------------
void vtkMRMLLandmarkDetectionNode::SetAndObserveOutputMarkupsNode(vtkMRMLMarkupsFiducialNode* node)
{
  this->SetNodeReferenceID(this->GetOutputMarkupsNodeReferenceRole(), (node ? node->GetID() : NULL));
}

//----------------------------------------------------------------------------
vtkMRMLTransformNode* vtkMRMLLandmarkDetectionNode::GetInputTransformNode()
{
  return vtkMRMLTransformNode::SafeDownCast(this->GetNodeReference(this->GetInputTransformNodeReferenceRole()));
}

//----------------------------------------------------------------------------
vtkMRMLMarkupsFiducialNode* vtkMRMLLandmarkDetectionNode::GetOutputMarkupsNode()
{
  return vtkMRMLMarkupsFiducialNode::SafeDownCast(this->GetNodeReference(this->GetOutputMarkupsNodeReferenceRole()));
}

//----------------------------------------------------------------------------
void vtkMRMLLandmarkDetectionNode::ResetDetection()
{
  this->InvokeEvent(ResetDetectionEvent);
}
