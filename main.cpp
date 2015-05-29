#include <assert.h>
#include <string.h>
#include "LLVMAPI.h"
#include "InitializeLLVM.h"
#include "CompilerState.h"
#include "Output.h"
#include "Compile.h"
#include "Link.h"
#include "log.h"
typedef jit::CompilerState State;

static void buildIR(State& state)
{
    using namespace jit;
    Output output(state);
    LValue arg0 = output.getParam(0);
    LBasicBlock entry = output.appendBasicBlock("Prologue");
    output.positionToBBEnd(entry);
    LValue one = output.constInt32(1);
    LValue gep = output.buildStructGEP(arg0, 0);
    LValue loaded = output.buildLoad(gep);
    LValue add = output.buildAdd(loaded, one);
    LBasicBlock patch = output.appendBasicBlock("Patch");
    output.buildBr(patch);
    output.positionToBBEnd(patch);
    LValue call = output.buildCall(output.repo().patchpointInt64Intrinsic(), output.constInt64(0), output.constInt32(13), output.constInt64(0x12345678), output.constInt32(2), arg0, add);
    output.buildRet(call);
}

int main()
{
    initLLVM();
    using namespace jit;
    State state("test");
    buildIR(state);
    dumpModule(state.m_module);
    compile(state);
    assert(state.m_entryPoint == state.m_codeSectionList.front().data());
    link(state);
    return 0;
}
