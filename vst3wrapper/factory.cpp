#include "processor.h"
#include "controller.h"
#include "ids.h"

#include "public.sdk/source/main/pluginfactory_constexpr.h"

#define HEG_VERSION "0.1.0"
#define HEG_PLUGIN_NAME "HEG VST Bridge"

BEGIN_FACTORY_DEF(
    "HEG Software Studio",
    "https://github.com/funjocke4/HEG-VST-Bridge",
    "",
    2
)

DEF_CLASS(
    HEG::ProcessorUID,
    Steinberg::PClassInfo::kManyInstances,
    kVstAudioEffectClass,
    HEG_PLUGIN_NAME,
    Steinberg::Vst::kDistributable,
    "Fx",
    HEG_VERSION,
    kVstVersionString,
    HEG::Processor::createInstance,
    nullptr
)

DEF_CLASS(
    HEG::ControllerUID,
    Steinberg::PClassInfo::kManyInstances,
    kVstComponentControllerClass,
    "HEG VST Bridge Controller",
    0,
    "",
    HEG_VERSION,
    kVstVersionString,
    HEG::Controller::createInstance,
    nullptr
)

END_FACTORY
