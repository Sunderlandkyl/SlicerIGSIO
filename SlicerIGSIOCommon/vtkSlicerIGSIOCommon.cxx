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

// SlicerIGSIOCommon includes
#include <igsioTrackedFrame.h>
#include <igsioVideoFrame.h>
#include "vtkSlicerIGSIOCommon.h"
#include "vtkStreamingVolumeCodec.h"
#include <vtkIGSIOTrackedFrameList.h>

// vtkAddon includes
#include <vtkStreamingVolumeCodecFactory.h>

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLSequenceBrowserNode.h>
#include <vtkMRMLSequenceNode.h>
#include <vtkMRMLStreamingVolumeNode.h>
#include <vtkMRMLLinearTransformNode.h>
#include <vtkMRMLSelectionNode.h>

// VTK includes
#include <vtkMatrix4x4.h>

// vtkSequenceIO includes
#include <vtkIGSIOMkvSequenceIO.h>

#include <stack>

std::string FRAME_STATUS_TRACKNAME = "FrameStatus";
std::string TRACKNAME_FIELD_NAME = "TrackName";
enum FrameStatus
{
  Frame_OK,
  Frame_Invalid,
  Frame_Skip,
};

//----------------------------------------------------------------------------
bool vtkSlicerIGSIOCommon::TrackedFrameListToVolumeSequence(vtkIGSIOTrackedFrameList* trackedFrameList, vtkMRMLSequenceNode* sequenceNode)
{
  if (!trackedFrameList || !sequenceNode)
  {
    vtkErrorWithObjectMacro(trackedFrameList, "Invalid arguments");
    return false;
  }

  std::string trackedFrameName = "Video";
  if (!trackedFrameList->GetCustomString(TRACKNAME_FIELD_NAME).empty())
  {
    trackedFrameName = trackedFrameList->GetCustomString(TRACKNAME_FIELD_NAME);
  }

  sequenceNode->SetIndexName("time");
  sequenceNode->SetIndexUnit("s");

  FrameSizeType frameSize = { 0,0,0 };
  std::string encodingFourCC;
  trackedFrameList->GetEncodingFourCC(encodingFourCC);
  if (!encodingFourCC.empty())
  {
    vtkSmartPointer<vtkStreamingVolumeCodec> codec = vtkSmartPointer<vtkStreamingVolumeCodec>::Take(
      vtkStreamingVolumeCodecFactory::GetInstance()->CreateCodecByFourCC(encodingFourCC));
    if (!codec)
    {
      vtkErrorWithObjectMacro(sequenceNode, "Could not find codec: " << encodingFourCC);
      return false;
    }
  }

  vtkSmartPointer<vtkStreamingVolumeFrame> previousFrame = NULL;

  // How many digits are required to represent the frame numbers
  int frameNumberMaxLength = std::floor(std::log10(trackedFrameList->GetNumberOfTrackedFrames())) + 1;

  for (int i = 0; i < trackedFrameList->GetNumberOfTrackedFrames(); ++i)
  {
    // Convert frame to a string with the a maximum number of digits (frameNumberMaxLength)
    // ex. 0, 1, 2, 3 or 0000, 0001, 0002, 0003 etc.
    std::stringstream frameNumberSS;
    frameNumberSS << std::setw(frameNumberMaxLength) << std::setfill('0') << i;

    igsioTrackedFrame* trackedFrame = trackedFrameList->GetTrackedFrame(i);
    std::stringstream timestampSS;
    timestampSS << trackedFrame->GetTimestamp();

    vtkSmartPointer<vtkMRMLVolumeNode> volumeNode;
    if (!trackedFrame->GetImageData()->IsFrameEncoded())
    {
      volumeNode = vtkSmartPointer<vtkMRMLVectorVolumeNode>::New();
      volumeNode->SetAndObserveImageData(trackedFrame->GetImageData()->GetImage());
    }
    else
    {
      vtkSmartPointer<vtkMRMLStreamingVolumeNode> streamingVolumeNode = vtkSmartPointer<vtkMRMLStreamingVolumeNode>::New();
      vtkSmartPointer<vtkStreamingVolumeFrame> currentFrame = trackedFrame->GetImageData()->GetEncodedFrame();

      // The previous frame is only relevant if the current frame is not a keyframe
      if (!trackedFrame->GetImageData()->GetEncodedFrame()->IsKeyFrame())
      {
        currentFrame->SetPreviousFrame(previousFrame);
      }
      streamingVolumeNode->SetAndObserveFrame(currentFrame);
      volumeNode = streamingVolumeNode;
      previousFrame = currentFrame;
    }

    vtkSmartPointer<vtkMatrix4x4> ijkToRASTransformMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
    igsioTransformName imageToPhysicalTransformName;
    imageToPhysicalTransformName.SetTransformName(trackedFrameName+"ToPhysical");
    if (trackedFrame->GetFrameField(imageToPhysicalTransformName.GetTransformName()+"Transform"))
    {
      if (trackedFrame->GetFrameTransform(imageToPhysicalTransformName, ijkToRASTransformMatrix) == IGSIO_SUCCESS)
      {
        if (volumeNode)
        {
          volumeNode->SetIJKToRASMatrix(ijkToRASTransformMatrix);
        }
      }
    }

    volumeNode->SetName(trackedFrameName.c_str());

    const char* frameStatus = trackedFrame->GetFrameField(FRAME_STATUS_TRACKNAME);
    if (!frameStatus || vtkVariant(frameStatus).ToInt() != Frame_Skip)
    {
      sequenceNode->SetDataNodeAtValue(volumeNode, timestampSS.str());
    }
  }

  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerIGSIOCommon::TrackedFrameListToSequenceBrowser(vtkIGSIOTrackedFrameList* trackedFrameList, vtkMRMLSequenceBrowserNode* sequenceBrowserNode)
{
  if (!trackedFrameList || !sequenceBrowserNode)
  {
    vtkErrorWithObjectMacro(trackedFrameList, "Invalid argument!");
    return false;
  }

  vtkMRMLScene* scene = sequenceBrowserNode->GetScene();
  if (!scene)
  {
    vtkErrorWithObjectMacro(sequenceBrowserNode, "No scene found in sequence browser nodes!");
    return false;
  }

  std::string trackedFrameName = "Video";
  if (!trackedFrameList->GetCustomString(TRACKNAME_FIELD_NAME).empty())
  {
    trackedFrameName = trackedFrameList->GetCustomString(TRACKNAME_FIELD_NAME);
  }

  vtkSmartPointer<vtkMRMLSequenceNode> videoSequenceNode = vtkSmartPointer <vtkMRMLSequenceNode>::New();
  videoSequenceNode->SetName(scene->GetUniqueNameByString(trackedFrameName.c_str()));
  vtkSlicerIGSIOCommon::TrackedFrameListToVolumeSequence(trackedFrameList, videoSequenceNode);
  scene->AddNode(videoSequenceNode);

  if (videoSequenceNode->GetNumberOfDataNodes() < 1)
  {
    vtkErrorWithObjectMacro(trackedFrameList, "No frames in trackedframelist!");
    return false;
  }
  sequenceBrowserNode->AddSynchronizedSequenceNode(videoSequenceNode);

  FrameSizeType frameSize = { 0,0,0 };
  trackedFrameList->GetFrameSize(frameSize);

  int dimensions[3] = { 0,0,0 };
  dimensions[0] = frameSize[0];
  dimensions[1] = frameSize[1];
  dimensions[2] = frameSize[2];

  std::map<std::string, vtkSmartPointer<vtkMRMLSequenceNode>> transformSequenceNodes;

  int frameNumberMaxLength = std::floor(std::log10(trackedFrameList->GetNumberOfTrackedFrames())) + 1;
  for (int i = 0; i < trackedFrameList->GetNumberOfTrackedFrames(); ++i)
  {
    igsioTrackedFrame* trackedFrame = trackedFrameList->GetTrackedFrame(i);
    std::stringstream timestampSS;
    timestampSS << trackedFrame->GetTimestamp();

    std::vector<igsioTransformName> transformNames;
    trackedFrame->GetFrameTransformNameList(transformNames);
    for (std::vector<igsioTransformName>::iterator transformNameIt = transformNames.begin(); transformNameIt != transformNames.end(); ++transformNameIt)
    {
      vtkSmartPointer<vtkMatrix4x4> transformMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
      trackedFrame->GetFrameTransform(*transformNameIt, transformMatrix);

      std::string transformName;
      transformNameIt->GetTransformName(transformName);
      if (transformName != trackedFrameName+"ToPhysical")
      {
        vtkSmartPointer<vtkMRMLLinearTransformNode> transformNode = vtkSmartPointer<vtkMRMLLinearTransformNode>::New();
        transformNode->SetMatrixTransformToParent(transformMatrix);

        std::string transformName = transformNameIt->GetTransformName();
        if (transformSequenceNodes.find(transformName) == transformSequenceNodes.end())
        {
          vtkSmartPointer<vtkMRMLSequenceNode> transformSequenceNode = vtkMRMLSequenceNode::SafeDownCast(
            scene->AddNewNodeByClass("vtkMRMLSequenceNode"));
          transformSequenceNode->SetName(transformName.c_str());
          transformSequenceNode->SetIndexName("time");
          transformSequenceNode->SetIndexUnit("s");
          sequenceBrowserNode->AddSynchronizedSequenceNode(transformSequenceNode);
          transformSequenceNodes[transformName] = transformSequenceNode;
        }
        transformSequenceNodes[transformName]->SetDataNodeAtValue(transformNode, timestampSS.str());
      }
    }

  }

  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerIGSIOCommon::VolumeSequenceToTrackedFrameList(vtkMRMLSequenceNode* sequenceNode, vtkIGSIOTrackedFrameList* trackedFrameList)
{
  if (!sequenceNode || !trackedFrameList)
  {
    vtkErrorWithObjectMacro(sequenceNode, "Invalid arguments");
    return false;
  }

  if (sequenceNode->GetNumberOfDataNodes() < 1)
  {
    vtkErrorWithObjectMacro(sequenceNode, "No data nodes in sequence");
    return false;
  }

  std::string codecFourCC;
  bool useTimestamp = sequenceNode->GetIndexName() == "time";
  std::string trackName = sequenceNode->GetName();
  if (sequenceNode->GetNumberOfDataNodes() > 0)
  {
    trackName = sequenceNode->GetNthDataNode(0)->GetName();
  }
  if (trackedFrameList->SetCustomString(TRACKNAME_FIELD_NAME, trackName) == IGSIO_FAIL)
  {
    vtkErrorWithObjectMacro(sequenceNode, "Could not set track name!")
      return false;
  }


  int dimensions[3] = { 0,0,0 };
  vtkSmartPointer<vtkStreamingVolumeFrame> lastFrame = NULL;
  double timestamp = 0;
  double lastTimestamp = 0.0;
  for (int i = 0; i < sequenceNode->GetNumberOfDataNodes(); ++i)
  {
    vtkMRMLStreamingVolumeNode* streamingVolumeNode = vtkMRMLStreamingVolumeNode::SafeDownCast(sequenceNode->GetNthDataNode(i));
    if (!streamingVolumeNode)
    {
      continue;
    }

    vtkStreamingVolumeFrame* frame = streamingVolumeNode->GetFrame();
    if (!frame)
    {
      continue;
    }

    frame->GetDimensions(dimensions);
    codecFourCC = streamingVolumeNode->GetCodecFourCC();

    std::stringstream timestampSS;
    timestampSS << sequenceNode->GetNthIndexValue(i);
    if (useTimestamp)
    {
      timestampSS >> timestamp;
    }
    else
    {
      timestamp += 0.1;
    }

    vtkSmartPointer<vtkMatrix4x4> ijkToRASTransform = vtkSmartPointer<vtkMatrix4x4>::New();
    streamingVolumeNode->GetIJKToRASMatrix(ijkToRASTransform);

    igsioTransformName imageToPhysicalName;
    imageToPhysicalName.SetTransformName(trackName+"ToPhysical");

    std::stack<vtkSmartPointer<vtkStreamingVolumeFrame> > frameStack;
    frameStack.push(frame);
    if (!frame->IsKeyFrame())
    {
      vtkStreamingVolumeFrame* currentFrame = frame->GetPreviousFrame();
      while (currentFrame && currentFrame != lastFrame)
      {
        frameStack.push(currentFrame);
        currentFrame = currentFrame->GetPreviousFrame();
      }
    }
    lastFrame = frame;

    int initialStackSize = frameStack.size();
    while (frameStack.size() > 0)
    {
      vtkStreamingVolumeFrame* currentFrame = frameStack.top();
      igsioTrackedFrame trackedFrame;
      igsioVideoFrame videoFrame;
      videoFrame.SetEncodedFrame(currentFrame);
      trackedFrame.SetImageData(videoFrame);
      double currentTimestamp = lastTimestamp + (timestamp-lastTimestamp)*((double)(initialStackSize - frameStack.size() + 1.0) / initialStackSize);
      trackedFrame.SetTimestamp(currentTimestamp);
      trackedFrame.SetFrameTransform(imageToPhysicalName, ijkToRASTransform);
      trackedFrame.SetFrameField(FRAME_STATUS_TRACKNAME, vtkVariant(frameStack.size() == 1 ? Frame_OK : Frame_Skip).ToString());
      trackedFrameList->AddTrackedFrame(&trackedFrame);
      frameStack.pop();
    }
    lastTimestamp = timestamp;
  }
  trackedFrameList->GetEncodingFourCC(codecFourCC);

  FrameSizeType frameSize = { 0,0,0 };
  for (int i=0; i<3; ++i)
  {
    frameSize[i] = (unsigned int)dimensions[i];
  }

  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerIGSIOCommon::SequenceBrowserToTrackedFrameList(vtkMRMLSequenceBrowserNode* sequenceBrowserNode, vtkIGSIOTrackedFrameList* trackedFrameList)
{
  // TODO: Not implemented yet
  return false;
}

//----------------------------------------------------------------------------
bool vtkSlicerIGSIOCommon::ReEncodeVideoSequence(vtkMRMLSequenceNode* videoStreamSequenceNode, int startIndex, int endIndex, std::string codecFourCC, std::map<std::string, std::string> codecParameters, bool forceReEncoding, bool minimalReEncoding)
{
  if (!videoStreamSequenceNode)
  {
    vtkErrorWithObjectMacro(videoStreamSequenceNode, "Cannot convert reference node to vtkMRMLSequenceNode");
    return false;
  }

  int dimensions[3] = { 0,0,0 };
  int numberOfFrames = videoStreamSequenceNode->GetNumberOfDataNodes();

  if (endIndex < 0)
  {
    endIndex = numberOfFrames - 1;
  }

  if (startIndex < 0 || startIndex >= numberOfFrames || startIndex > endIndex
    || endIndex >= numberOfFrames)
  {
    vtkErrorWithObjectMacro(videoStreamSequenceNode, "Invalid start and end indices!");
    return false;
  }

  std::vector<FrameBlock> frameBlocks;

  if (!forceReEncoding)
  {
    FrameBlock currentFrameBlock;
    currentFrameBlock.StartFrame = 0;
    currentFrameBlock.EndFrame = 0;
    currentFrameBlock.ReEncodingRequired = false;
    vtkSmartPointer<vtkStreamingVolumeFrame> previousFrame = NULL;
    for (int i = startIndex; i <= endIndex; ++i)
    {
      // TODO: for now, only support sequences of vtkMRMLStreamingVolumeNode
      // In the future, this could be changed to allow all types of volume nodes to be encoded
      vtkMRMLStreamingVolumeNode* streamingNode = vtkMRMLStreamingVolumeNode::SafeDownCast(videoStreamSequenceNode->GetNthDataNode(i));
      if (!streamingNode)
      {
        vtkErrorWithObjectMacro(videoStreamSequenceNode, "Invalid data node at index " << i);
        return false;
      }

      if (codecFourCC == "")
      {
        codecFourCC = streamingNode->GetCodecFourCC();
      }

      vtkStreamingVolumeFrame* currentFrame = streamingNode->GetFrame();
      if (currentFrameBlock.ReEncodingRequired == false)
      {
        if (i == 0 && !streamingNode->IsKeyFrame())
        {
          currentFrameBlock.ReEncodingRequired = true;
        }
        else if (!currentFrame)
        {
          currentFrameBlock.ReEncodingRequired = true;
        }
        else if (codecFourCC == "")
        {
          currentFrameBlock.ReEncodingRequired = true;
        }
        else if (codecFourCC != streamingNode->GetCodecFourCC())
        {
          currentFrameBlock.ReEncodingRequired = true;
        }
        else if (!minimalReEncoding && currentFrame && !currentFrame->IsKeyFrame() && previousFrame != currentFrame->GetPreviousFrame())
        {
          currentFrameBlock.ReEncodingRequired = true;
        }
      }

      if (currentFrame && // Current frame exists
        previousFrame && // Current frame is not the initial frame
        currentFrame->IsKeyFrame() && // Current frame is a keyframe
        !previousFrame->IsKeyFrame()) // Previous frame was not also a keyframe
      {
        frameBlocks.push_back(currentFrameBlock);
        currentFrameBlock = FrameBlock();
        currentFrameBlock.StartFrame = i;
        currentFrameBlock.EndFrame = i;
        currentFrameBlock.ReEncodingRequired = false;
      }
      currentFrameBlock.EndFrame = i;
      previousFrame = currentFrame;

      /// TODO: If dimension changes, re-encode smaller images with padding
    }
    frameBlocks.push_back(currentFrameBlock);
  }
  else
  {
    FrameBlock totalFrameBlock;
    totalFrameBlock.StartFrame = startIndex;
    totalFrameBlock.EndFrame = endIndex;
    totalFrameBlock.ReEncodingRequired = true;
    frameBlocks.push_back(totalFrameBlock);
  }

  if (codecFourCC == "")
  {
    std::vector<std::string> codecFourCCs = vtkStreamingVolumeCodecFactory::GetInstance()->GetStreamingCodecFourCCs();
    if (codecFourCCs.empty())
    {
      vtkErrorWithObjectMacro(videoStreamSequenceNode, "Re-encode failed! No codecs registered!");
      return false;
    }

    codecFourCC = codecFourCCs.front();
    vtkDebugWithObjectMacro(videoStreamSequenceNode, "Streaming volume codec not specified! Using: " << codecFourCC);
  }

  std::vector<FrameBlock>::iterator frameBlockIt;
  for (frameBlockIt = frameBlocks.begin(); frameBlockIt != frameBlocks.end(); ++frameBlockIt)
  {
    if (frameBlockIt->ReEncodingRequired)
    {
      vtkNew<vtkMRMLStreamingVolumeNode> decodingProxyNode;
      vtkSmartPointer<vtkStreamingVolumeCodec> codec = vtkSmartPointer<vtkStreamingVolumeCodec>::Take(
        vtkStreamingVolumeCodecFactory::GetInstance()->CreateCodecByFourCC(codecFourCC));
      if (!codec)
      {
        vtkErrorWithObjectMacro(videoStreamSequenceNode, "Could not find codec: " << codecFourCC);
        return false;
      }
      codec->SetParameters(codecParameters);

      for (int i = frameBlockIt->StartFrame; i <= frameBlockIt->EndFrame; ++i)
      {
        vtkMRMLVolumeNode* volumeNode = vtkMRMLVolumeNode::SafeDownCast(videoStreamSequenceNode->GetNthDataNode(i));
        if (!volumeNode)
        {
          vtkErrorWithObjectMacro(videoStreamSequenceNode, "Invalid data node at index " << i);
          return false;
        }

        vtkSmartPointer<vtkImageData> imageData = NULL;
        vtkMRMLStreamingVolumeNode* streamingNode = vtkMRMLStreamingVolumeNode::SafeDownCast(volumeNode);
        if (streamingNode && streamingNode->GetFrame())
        {
          decodingProxyNode->SetAndObserveFrame(streamingNode->GetFrame());
          decodingProxyNode->DecodeFrame();
          imageData = decodingProxyNode->GetImageData();
        }
        else
        {
          imageData = volumeNode->GetImageData();
        }

        vtkSmartPointer<vtkStreamingVolumeFrame> frame = vtkSmartPointer<vtkStreamingVolumeFrame>::New();
        if (!codec->EncodeImageData(imageData, frame))
        {
          vtkErrorWithObjectMacro(videoStreamSequenceNode, "Error encoding frame!");
          return false;
        }
        streamingNode->SetAndObserveFrame(frame);
      }
    }
  }
  return true;
}
