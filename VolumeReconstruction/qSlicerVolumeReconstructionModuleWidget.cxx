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

// Qt includes
#include <QDebug>
#include <QStandardItemModel>
#include <QTreeView>
#include <QTextEdit>

// SlicerQt includes
#include "qSlicerVolumeReconstructionModuleWidget.h"
#include "ui_qSlicerVolumeReconstructionModule.h"

// SlicerIGSIOCommon includes
#include "vtkSlicerIGSIOCommon.h"

// Sequence includes
#include <vtkMRMLSequenceBrowserNode.h>

#include <vtkMRMLScalarVolumeNode.h>

#include "vtkSlicerVolumeReconstructionLogic.h"

#include "vtkIGSIOVolumeReconstructor.h"
#include "vtkIGSIOTrackedFrameList.h"

// qMRMLWidgets includes
#include <qMRMLNodeFactory.h>
#include <vtkMetaImageWriter.h>

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_VolumeReconstruction
class qSlicerVolumeReconstructionModuleWidgetPrivate: public Ui_qSlicerVolumeReconstructionModule
{
  Q_DECLARE_PUBLIC(qSlicerVolumeReconstructionModuleWidget);
protected:
  qSlicerVolumeReconstructionModuleWidget* const q_ptr;
public:
  qSlicerVolumeReconstructionModuleWidgetPrivate(qSlicerVolumeReconstructionModuleWidget& object);
  ~qSlicerVolumeReconstructionModuleWidgetPrivate();

  vtkSlicerVolumeReconstructionLogic* logic() const;

};

//-----------------------------------------------------------------------------
// qSlicerVolumeReconstructionModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerVolumeReconstructionModuleWidgetPrivate::qSlicerVolumeReconstructionModuleWidgetPrivate(qSlicerVolumeReconstructionModuleWidget& object)
  : q_ptr(&object)
{
}

//-----------------------------------------------------------------------------
qSlicerVolumeReconstructionModuleWidgetPrivate::~qSlicerVolumeReconstructionModuleWidgetPrivate()
{
}

//-----------------------------------------------------------------------------
vtkSlicerVolumeReconstructionLogic* qSlicerVolumeReconstructionModuleWidgetPrivate::logic() const
{
  Q_Q(const qSlicerVolumeReconstructionModuleWidget);
  return vtkSlicerVolumeReconstructionLogic::SafeDownCast(q->logic());
}


//-----------------------------------------------------------------------------
// qSlicerVolumeReconstructionModuleWidget methods

//-----------------------------------------------------------------------------
qSlicerVolumeReconstructionModuleWidget::qSlicerVolumeReconstructionModuleWidget(QWidget* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerVolumeReconstructionModuleWidgetPrivate(*this))
{
}

//-----------------------------------------------------------------------------
qSlicerVolumeReconstructionModuleWidget::~qSlicerVolumeReconstructionModuleWidget()
{
}

//-----------------------------------------------------------------------------
void qSlicerVolumeReconstructionModuleWidget::setup()
{
  Q_D(qSlicerVolumeReconstructionModuleWidget);
  d->setupUi(this);
  this->Superclass::setup();

  d->InterpolationModeComboBox->addItem("Nearest neighbor", vtkIGSIOPasteSliceIntoVolume::InterpolationType::NEAREST_NEIGHBOR_INTERPOLATION);
  d->InterpolationModeComboBox->addItem("Linear", vtkIGSIOPasteSliceIntoVolume::InterpolationType::LINEAR_INTERPOLATION);

  d->OptimizationModeComboBox->addItem("No optimization", vtkIGSIOPasteSliceIntoVolume::OptimizationType::NO_OPTIMIZATION);
  d->OptimizationModeComboBox->addItem("Partial optimization", vtkIGSIOPasteSliceIntoVolume::OptimizationType::PARTIAL_OPTIMIZATION);
  d->OptimizationModeComboBox->addItem("Full optimization", vtkIGSIOPasteSliceIntoVolume::OptimizationType::FULL_OPTIMIZATION);

  d->CompoundingModeComboBox->addItem("Latest", vtkIGSIOPasteSliceIntoVolume::CompoundingType::LATEST_COMPOUNDING_MODE);
  d->CompoundingModeComboBox->addItem("Maximum", vtkIGSIOPasteSliceIntoVolume::CompoundingType::MAXIMUM_COMPOUNDING_MODE);
  d->CompoundingModeComboBox->addItem("Mean", vtkIGSIOPasteSliceIntoVolume::CompoundingType::MEAN_COMPOUNDING_MODE);
  d->CompoundingModeComboBox->addItem("Importance mask", vtkIGSIOPasteSliceIntoVolume::CompoundingType::IMPORTANCE_MASK_COMPOUNDING_MODE);

  connect(d->ApplyButton, SIGNAL(clicked()), this, SLOT(onApply()));
  connect(d->InputSequenceBrowserSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)), this, SLOT(updateWidgetFromMRML()));
  connect(d->OutputVolumeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)), this, SLOT(updateWidgetFromMRML()));
}

//-----------------------------------------------------------------------------
void qSlicerVolumeReconstructionModuleWidget::updateWidgetFromMRML()
{
  Q_D(qSlicerVolumeReconstructionModuleWidget);
  
  vtkMRMLSequenceBrowserNode* inputSequenceBrowser = vtkMRMLSequenceBrowserNode::SafeDownCast(d->InputSequenceBrowserSelector->currentNode());
  vtkMRMLScalarVolumeNode* outputVolunmeNode = vtkMRMLScalarVolumeNode::SafeDownCast(d->OutputVolumeSelector->currentNode());

  //d->ApplyButton->setEnabled(inputSequenceBrowser && outputVolunmeNode);
}

//-----------------------------------------------------------------------------
void qSlicerVolumeReconstructionModuleWidget::onApply()
{
  Q_D(qSlicerVolumeReconstructionModuleWidget);

  vtkMRMLSequenceBrowserNode* inputSequenceBrowser = vtkMRMLSequenceBrowserNode::SafeDownCast(d->InputSequenceBrowserSelector->currentNode());
  vtkMRMLScalarVolumeNode* outputVolunmeNode = vtkMRMLScalarVolumeNode::SafeDownCast(d->OutputVolumeSelector->currentNode());

  //vtkMRMLSequenceBrowserNode* outputSequenceBrowser = vtkMRMLSequenceBrowserNode::SafeDownCast(d->OutputVolumeSelector->currentNode());
  //vtkNew<vtkIGSIOTrackedFrameList> trackedFrameList;
  //vtkSlicerIGSIOCommon::SequenceBrowserToTrackedFrameList(inputSequenceBrowser, trackedFrameList);
  //vtkSlicerIGSIOCommon::TrackedFrameListToSequenceBrowser(trackedFrameList, outputSequenceBrowser); 

  double outputSpacing[3] = { 0,0,0 };
  outputSpacing[0] = d->XSpacingSpinbox->value();
  outputSpacing[1] = d->YSpacingSpinbox->value();
  outputSpacing[2] = d->ZSpacingSpinbox->value();

  int interpolationMode = d->InterpolationModeComboBox->currentData().toInt();
  int optimizationMode = d->OptimizationModeComboBox->currentData().toInt();
  int compoundingMode = d->CompoundingModeComboBox->currentData().toInt();
  bool fillHoles = d->FillHolesCheckBox->isChecked();
  int numberOfThreads = d->NumberOfThreadsSpinBox->value();

  d->logic()->ReconstructVolume(inputSequenceBrowser, outputVolunmeNode, outputSpacing, interpolationMode, optimizationMode, compoundingMode, fillHoles, numberOfThreads);
  vtkNew<vtkMetaImageWriter> writer;
  writer->SetFileName("E:\d\p\PD15\PlusLibData\TestImages\fCal_Test_Calibration_3NWires.mha");
  writer->SetInputData(outputVolunmeNode->GetImageData());
  writer->Write();
}

//-----------------------------------------------------------------------------
void qSlicerVolumeReconstructionModuleWidget::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerVolumeReconstructionModuleWidget);

  this->Superclass::setMRMLScene(scene);
  this->updateWidgetFromMRML();
  if (scene == NULL)
  {
    return;
  }
}
