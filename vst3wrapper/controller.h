#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"

namespace HEG {

class Controller : public Steinberg::Vst::EditController {
public:
    static Steinberg::FUnknown* createInstance(void*) {
        return static_cast<Steinberg::Vst::IEditController*>(
            new Controller()
        );
    }
};

}
