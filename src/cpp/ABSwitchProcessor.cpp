#include <base/source/fstreamer.h>
#include <public.sdk/source/vst/vstaudioprocessoralgo.h>

#include "ABSwitchProcessor.h"
#include "ABSwitchUtils.h"
#include "ABSwitchCIDs.h"
#include "logging/loguru.hpp"

namespace pongasoft {
namespace VST {

/*
 * We assume that input A is indexed 0 and input B is indexed 1
 */
inline int mapSwitchStateToInput(ESwitchState switchState)
{
  return switchState == ESwitchState::kA ? 0 : 1;
}

///////////////////////////////////////////
// ABSwitchProcessor::ABSwitchProcessor
///////////////////////////////////////////
ABSwitchProcessor::ABSwitchProcessor() : AudioEffect(),
                                         fState{ESwitchState::kA, true},
                                         fPreviousState{fState},
                                         fAudioOn{false},
                                         fStateUpdate{},
                                         fLatestState{fState}
{
  setControllerClass(ABSwitchControllerUID);
  DLOG_F(INFO, "ABSwitchProcessor::ABSwitchProcessor()");
}

///////////////////////////////////////////
// ABSwitchProcessor::~ABSwitchProcessor
///////////////////////////////////////////
ABSwitchProcessor::~ABSwitchProcessor()
{
  DLOG_F(INFO, "ABSwitchProcessor::~ABSwitchProcessor()");
}


///////////////////////////////////////////
// ABSwitchProcessor::initialize
///////////////////////////////////////////
tresult PLUGIN_API ABSwitchProcessor::initialize(FUnknown *context)
{
  DLOG_F(INFO, "ABSwitchProcessor::initialize()");

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

///////////////////////////////////////////
// ABSwitchProcessor::terminate
///////////////////////////////////////////
tresult PLUGIN_API ABSwitchProcessor::terminate()
{
  DLOG_F(INFO, "ABSwitchProcessor::terminate()");

  return AudioEffect::terminate();
}

///////////////////////////////////////////
// ABSwitchProcessor::setupProcessing
///////////////////////////////////////////
tresult ABSwitchProcessor::setupProcessing(ProcessSetup &setup)
{
  tresult result = AudioEffect::setupProcessing(setup);

  if(result != kResultOk)
    return result;

  DLOG_F(INFO,
         "ABSwitchProcessor::setupProcessing(processMode=%d, symbolicSampleSize=%d, maxSamplesPerBlock=%d, sampleRate=%f)",
         setup.processMode,
         setup.symbolicSampleSize,
         setup.maxSamplesPerBlock,
         setup.sampleRate);

  return result;
}

///////////////////////////////////////////
// ABSwitchProcessor::setActive
///////////////////////////////////////////
tresult PLUGIN_API ABSwitchProcessor::setActive(TBool state)
{
  DLOG_F(INFO, "ABSwitchProcessor::setActive(%s)", state ? "true" : "false");
  return AudioEffect::setActive(state);
}

///////////////////////////////////////////
// ABSwitchProcessor::process
///////////////////////////////////////////
tresult PLUGIN_API ABSwitchProcessor::process(ProcessData &data)
{
  // 1. process parameter changes
  if(data.inputParameterChanges != nullptr)
    processParameters(*data.inputParameterChanges);

  // 2. process inputs
  tresult res = processInputs(data);

  // 3. update the state
  fPreviousState = fState;

  return res;
}

///////////////////////////////////////////
// ABSwitchProcessor::processInputs
///////////////////////////////////////////
tresult ABSwitchProcessor::processInputs(ProcessData &data)
{
  // 2. process inputs
  if(data.numInputs == 0 || data.numOutputs == 0)
  {
    // nothing to do
    return kResultOk;
  }

  tresult res;

  fStateUpdate.pop(fState);

  // case when we are switching between A & B and soften is on => need to cross fade
  // also note that we need more than 1 input in order to cross fade...
  if(fPreviousState.fSwitchState != fState.fSwitchState && fState.fSoften && data.numInputs > 1)
    res = processCrossFade(data);
  else
    res = processCopy(data);


  // handle Audio On/Off LED light
  if(res == kResultOk)
  {
    AudioBusBuffers &stereoOutput = data.outputs[0];

    bool audioOn = !Utils::isSilent(stereoOutput);

    if(audioOn != fAudioOn)
    {
      fAudioOn = audioOn;

      IParameterChanges* outParamChanges = data.outputParameterChanges;
      if(outParamChanges != nullptr)
      {
        int32 index = 0;
        auto paramQueue = outParamChanges->addParameterData(kAudioOn, index);
        if(paramQueue != nullptr)
        {
          int32 index2 = 0;
          paramQueue->addPoint(0, audioOn ? 1.0 : 0.0, index2);
        }
      }
    }
  }

  return res;
}

///////////////////////////////////////////
// ABSwitchProcessor::processCopy
///////////////////////////////////////////
tresult ABSwitchProcessor::processCopy(ProcessData &data)
{
  // there is at least 1 input (data.numInputs > 0 at this point)
  int inputIndex = 0;

  // this is where the "magic" happens => determines which input we use (A or B)
  if(data.numInputs > 1)
    inputIndex = mapSwitchStateToInput(fState.fSwitchState);

  AudioBusBuffers &stereoInput = data.inputs[inputIndex];
  AudioBusBuffers &stereoOutput = data.outputs[0];

  if(data.symbolicSampleSize == kSample32)
  {
    return Utils::copy<Sample32>(stereoInput,
                                 stereoOutput,
                                 data.numSamples);
  }
  else
  {
    return Utils::copy<Sample64>(stereoInput,
                                 stereoOutput,
                                 data.numSamples);
  }
}


///////////////////////////////////////////
// ABSwitchProcessor::processCrossFade
///////////////////////////////////////////
tresult ABSwitchProcessor::processCrossFade(ProcessData &data)
{
  AudioBusBuffers &stereoOutput = data.outputs[0];

  AudioBusBuffers &stereoInput1 = data.inputs[mapSwitchStateToInput(fPreviousState.fSwitchState)];
  AudioBusBuffers &stereoInput2 = data.inputs[mapSwitchStateToInput(fState.fSwitchState)];

  if(data.symbolicSampleSize == kSample32)
  {
    return Utils::linearCrossFade<Sample32>(stereoInput1,
                                            stereoInput2,
                                            stereoOutput,
                                            data.numSamples);
  }
  else
  {
    return Utils::linearCrossFade<Sample64>(stereoInput1,
                                            stereoInput2,
                                            stereoOutput,
                                            data.numSamples);
  }
}

///////////////////////////////////////////
// ABSwitchProcessor::canProcessSampleSize
//
// * Overridden so that we can declare we support 64bits
///////////////////////////////////////////
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

      State newSate{fState};

      // we read the "last" point (ignoring multiple changes for now)
      if(paramQueue->getPoint(numPoints - 1, sampleOffset, value) == kResultOk)
      {
        switch(paramQueue->getParameterId())
        {
          case kAudioSwitch:
            newSate.fSwitchState = ESwitchStateFromValue(value);
            DLOG_F(INFO, "ABSwitchProcessor::processParameters => fSwitchState=%i", newSate.fSwitchState);
            break;

          case kSoftenSwitch:
            newSate.fSoften = value == 1.0;
            DLOG_F(INFO, "ABSwitchProcessor::processParameters => fSoften=%s", newSate.fSoften ? "true" : "false");
            break;

          default:
            // shouldn't happen?
            break;
        }
      }

      fState = newSate;
      fLatestState.set(newSate);
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

  DLOG_F(INFO, "ABSwitchProcessor::setState()");

  IBStreamer streamer(state, kLittleEndian);

  State newState{};

  // ABSwitchParamID::kAudioSwitch
  float savedParam1;
  if(!streamer.readFloat(savedParam1))
    savedParam1 = 0;
  newState.fSwitchState = ESwitchStateFromValue(savedParam1);

  // ABSwitchParamID::kSoftenSwitch
  bool savedParam2;
  if(!streamer.readBool(savedParam2))
    savedParam2 = true;
  newState.fSoften = savedParam2;

  fStateUpdate.push(newState);

  DLOG_F(INFO, "ABSwitchProcessor::setState => fSwitchState=%s, fSoften=%s", newState.fSwitchState == ESwitchState::kA ? "kA" : "kB", newState.fSoften ? "true" : "false");

  return kResultOk;
}

///////////////////////////////////
// ABSwitchProcessor::getState
///////////////////////////////////
tresult ABSwitchProcessor::getState(IBStream *state)
{
  if(state == nullptr)
    return kResultFalse;

  DLOG_F(INFO, "ABSwitchProcessor::getState()");

  auto latestState = fLatestState.get();
  IBStreamer streamer(state, kLittleEndian);
  streamer.writeFloat(latestState.fSwitchState == ESwitchState::kA ? 0 : 1.0f);
  streamer.writeBool(latestState.fSoften);
  return kResultOk;
}

}
}