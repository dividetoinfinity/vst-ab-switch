#include <base/source/fstreamer.h>
#include <public.sdk/source/vst/vstaudioprocessoralgo.h>

#include "ABSwitchProcessor.h"
#include "ABSwitchProcess.h"
#include "ABSwitchCIDs.h"

namespace pongasoft {
namespace VST {

ABSwitchProcessor::ABSwitchProcessor() : AudioEffect(), fSwitchState(ESwitchState::kA)
{
  setControllerClass(ABSwitchControllerUID);
}

ABSwitchProcessor::~ABSwitchProcessor() = default;

tresult PLUGIN_API ABSwitchProcessor::initialize(FUnknown *context)
{
  tresult result = AudioEffect::initialize(context);
  if(result != kResultOk)
  {
    return result;
  }

  // 2 ins (A and B) => 1 out
  addAudioInput(STR16 ("Stereo In A"), SpeakerArr::kStereo);
  addAudioInput(STR16 ("Stereo In B"), SpeakerArr::kStereo);
  addAudioOutput(STR16 ("Stereo Out"), SpeakerArr::kStereo);

  return result;
}

tresult PLUGIN_API ABSwitchProcessor::terminate()
{
  return AudioEffect::terminate();
}

tresult PLUGIN_API ABSwitchProcessor::setActive(TBool state)
{
  return AudioEffect::setActive(state);
}

/**
 * This is where the processing actually happens
 */
tresult PLUGIN_API ABSwitchProcessor::process(ProcessData &data)
{
  // 1. process parameter changes
  if(data.inputParameterChanges != nullptr)
    processParameters(*data.inputParameterChanges);

  // 2. process inputs
  if(data.numInputs == 0 || data.numOutputs == 0)
  {
    // nothing to do
    return kResultOk;
  }

  // sanity check... not sure what to do... need to investigate
  if(data.numInputs != 2)
  {
    // nothing to do
    return kResultOk;
  }

  // there is at least 1 input (data.numImputs != 0 at this point)
  int inputIndex = 0;

  // this is where the "magic" happens => determines which input we use (A or B)
  if(data.numInputs > 1)
    inputIndex = fSwitchState == ESwitchState::kA ? 0 : 1;

  AudioBusBuffers &stereoInput = data.inputs[inputIndex];
  AudioBusBuffers &stereoOutput = data.outputs[0];

  //---get audio buffers----------------
  uint32 sampleFramesSize = getSampleFramesSizeInBytes(processSetup, data.numSamples);
  void **in = getChannelBuffersPointer(processSetup, stereoInput);
  void **out = getChannelBuffersPointer(processSetup, stereoOutput);

  // since we copy input to output, we end up with the same flags (true only if stereoInput.numChannels == stereoOutput.numChannels)
  stereoOutput.silenceFlags = stereoInput.silenceFlags;

  for(int i = 0; i < stereoInput.numChannels; ++i)
  {
    // sanity check => we make sure there is enough output channels
    if(stereoOutput.numChannels >= i)
    {
      // no need to copy if input and output are the same...
      if(in[i] != out[i])
      {
        // simply copy the samples
        memcpy(out[i], in[i], sampleFramesSize);
      }
    }
  }

  return kResultOk;
}

/** Overridden so that we can declare we support 64bits */
tresult ABSwitchProcessor::canProcessSampleSize(int32 symbolicSampleSize)
{
  if(symbolicSampleSize == kSample32)
    return kResultTrue;

  // we support double processing
  if(symbolicSampleSize == kSample64)
    return kResultTrue;

  return kResultFalse;
}

///////////////////////////////////////////
// ABSwitchProcessor::processParameters
///////////////////////////////////////////
void ABSwitchProcessor::processParameters(IParameterChanges &inputParameterChanges)
{
  int32 numParamsChanged = inputParameterChanges.getParameterCount();
  for(int i = 0; i < numParamsChanged; ++i)
  {
    IParamValueQueue *paramQueue = inputParameterChanges.getParameterData(i);
    if(paramQueue != nullptr)
    {
      ParamValue value;
      int32 sampleOffset;
      int32 numPoints = paramQueue->getPointCount();

      // we read the "last" point (ignoring multiple changes for now)
      if(paramQueue->getPoint(numPoints - 1, sampleOffset, value) == kResultOk)
      {
        switch(paramQueue->getParameterId())
        {
          case kAudioSwitch:
            fSwitchState = ESwitchStateFromValue(value);
            break;

          default:
            // shouldn't happen?
            break;
        }
      }

    }
  }
}

///////////////////////////////////
// ABSwitchProcessor::setState
///////////////////////////////////
tresult ABSwitchProcessor::setState(IBStream *state)
{
  if(state == nullptr)
    return kResultFalse;

  IBStreamer streamer(state, kLittleEndian);

  // ABSwitchParamID::kAudioSwitch
  float savedParam1 = 0.f;
  if(!streamer.readFloat(savedParam1))
    return kResultFalse;

  fSwitchState = ESwitchStateFromValue(savedParam1);

  return kResultOk;
}

///////////////////////////////////
// ABSwitchProcessor::getState
///////////////////////////////////
tresult ABSwitchProcessor::getState(IBStream *state)
{
  IBStreamer streamer(state, kLittleEndian);
  streamer.writeFloat(fSwitchState == ESwitchState::kA ? 0 : 1.0f);
  return kResultOk;
}

}
}