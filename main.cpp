#include <assert.h>
#include <string.h>
#include "LLVMAPI.h"
#include "InitializeLLVM.h"
#include "CompilerState.h"
#include "IntrinsicRepository.h"
#include "log.h"
typedef jit::CompilerState State;
#define SECTION_NAME_PREFIX "."
#define SECTION_NAME(NAME) (SECTION_NAME_PREFIX NAME)

static uint8_t* mmAllocateCodeSection(
    void* opaqueState, uintptr_t size, unsigned alignment, unsigned, const char* sectionName)
{
    State& state = *static_cast<State*>(opaqueState);

    jit::ByteBuffer bb(size);
    bb.resize(size);
    assert((reinterpret_cast<uintptr_t>(bb.data()) & (alignment - 1)) == 0);

    state.m_codeSectionList.push_back(std::move(bb));
    state.m_codeSectionNames.push_back(sectionName);

    return const_cast<uint8_t*>(state.m_codeSectionList.back().data());
}

static uint8_t* mmAllocateDataSection(
    void* opaqueState, uintptr_t size, unsigned alignment, unsigned,
    const char* sectionName, LLVMBool)
{
    State& state = *static_cast<State*>(opaqueState);
    jit::ByteBuffer bb(size);
    bb.resize(size);
    assert((reinterpret_cast<uintptr_t>(bb.data()) & (alignment - 1)) == 0);
    state.m_codeSectionList.push_back(std::move(bb));
    state.m_codeSectionNames.push_back(sectionName);
    jit::ByteBuffer& bb2 = state.m_codeSectionList.back();

    if (!strcmp(sectionName, SECTION_NAME("llvm_stackmaps")))
        state.m_stackMapsSection = &bb2;
    return const_cast<uint8_t*>(bb2.data());
}

static LLVMBool mmApplyPermissions(void*, char**)
{
    return false;
}

static void mmDestroy(void*)
{
}

static void compile(State& state)
{
    LLVMMCJITCompilerOptions options;
    llvmAPI->InitializeMCJITCompilerOptions(&options, sizeof(options));
    options.OptLevel = 2;
    LLVMExecutionEngineRef engine;
    char* error = 0;

    options.MCJMM = llvmAPI->CreateSimpleMCJITMemoryManager(
        &state, mmAllocateCodeSection, mmAllocateDataSection, mmApplyPermissions, mmDestroy);
    if (llvmAPI->CreateMCJITCompilerForModule(&engine, state.m_module, &options, sizeof(options), &error)) {
        LOGE("FATAL: Could not create LLVM execution engine: %s", error);
        assert(false);
    }
    LLVMModuleRef module = state.m_module;
    LLVMPassManagerRef functionPasses = 0;
    LLVMPassManagerRef modulePasses;
    LLVMTargetDataRef targetData = llvmAPI->GetExecutionEngineTargetData(engine);
    char* stringRepOfTargetData = llvmAPI->CopyStringRepOfTargetData(targetData);
    llvmAPI->SetDataLayout(module, stringRepOfTargetData);
    free(stringRepOfTargetData);

    LLVMPassManagerBuilderRef passBuilder = llvmAPI->PassManagerBuilderCreate();
    llvmAPI->PassManagerBuilderSetOptLevel(passBuilder, 2);
    llvmAPI->PassManagerBuilderUseInlinerWithThreshold(passBuilder, 275);
    llvmAPI->PassManagerBuilderSetSizeLevel(passBuilder, 0);

    functionPasses = llvmAPI->CreateFunctionPassManagerForModule(module);
    modulePasses = llvmAPI->CreatePassManager();

    llvmAPI->AddTargetData(llvmAPI->GetExecutionEngineTargetData(engine), modulePasses);

    llvmAPI->PassManagerBuilderPopulateFunctionPassManager(passBuilder, functionPasses);
    llvmAPI->PassManagerBuilderPopulateModulePassManager(passBuilder, modulePasses);

    llvmAPI->PassManagerBuilderDispose(passBuilder);

    llvmAPI->InitializeFunctionPassManager(functionPasses);
    for (LLVMValueRef function = llvmAPI->GetFirstFunction(module); function; function = llvmAPI->GetNextFunction(function))
        llvmAPI->RunFunctionPassManager(functionPasses, function);
    llvmAPI->FinalizeFunctionPassManager(functionPasses);

    llvmAPI->RunPassManager(modulePasses, module);
    state.m_entryPoint = reinterpret_cast<void*>(llvmAPI->GetPointerToGlobal(engine, state.m_function));

    if (functionPasses)
        llvmAPI->DisposePassManager(functionPasses);
    llvmAPI->DisposePassManager(modulePasses);
    llvmAPI->DisposeExecutionEngine(engine);
}

int main()
{
    initLLVM();
    State state("test");
    using namespace jit;
    LType int32Type_ = int32Type(state.m_context);
    LType structElements[] = { int32Type_ };
    LType argumentType = pointerType(structType(state.m_context, structElements, sizeof(structElements) / sizeof(structElements[0])));
    state.m_function = addFunction(
        state.m_module, "test", functionType(int32Type_, argumentType));
    IntrinsicRepository repo(state.m_context, state.m_module);
    LValue arg0 = getParam(state.m_function, 0);
    LBasicBlock entry = appendBasicBlock(state.m_context, state.m_function, "Prologue");
    LBuilder builder = llvmAPI->CreateBuilderInContext(state.m_context);
    llvmAPI->PositionBuilderAtEnd(builder, entry);
    LValue one = constInt(int32Type_, 1);
    LValue gep = buildStructGEP(builder, arg0, 0);
    LValue loaded = buildLoad(builder, gep);
    LValue add = buildAdd(builder, loaded, one);
    buildCall(builder, repo.patchpointVoidIntrinsic(), constInt(int64Type(state.m_context), 0), constInt(int32Type_, 8), constNull(repo.ref8), constInt(int32Type_, 1), add);
    buildRet(builder, add);
    llvmAPI->DisposeBuilder(builder);

    dumpModule(state.m_module);
    compile(state);
    dumpModule(state.m_module);
    assert(state.m_entryPoint == state.m_codeSectionList.front().data());
    return 0;
}
