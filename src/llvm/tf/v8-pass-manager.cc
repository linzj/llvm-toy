// Copyright 2019 UCWeb Co., Ltd.

#include "src/llvm/tf/v8-pass-manager.h"

#include <iostream>
#include "src/assembler-inl.h"
#include "src/compiler/linkage.h"
#include "src/compiler/schedule.h"
#include "src/llvm/compile.h"
#include "src/llvm/compiler-state.h"
#include "src/llvm/initialize-llvm.h"
#include "src/llvm/log.h"
#include "src/llvm/output.h"
#include "src/llvm/stack-map-info.h"
#include "src/llvm/tf/schedule-emitter.h"

#include "src/llvm/basic-block-manager.h"
#include "src/llvm/basic-block.h"
#include "src/llvm/liveness-analysis-visitor.h"
#include "src/llvm/llvm-tf-builder.h"
#include "src/llvm/load-constant-recorder.h"
#include "src/llvm/tf/tf-parser.h"
#include "src/llvm/tf/v8-codegen.h"

namespace v8 {
namespace internal {
namespace tf_llvm {

static tf_llvm::LType GetLLVMType(CommonValues& cv, MachineType mt) {
  switch (mt.representation()) {
    case MachineRepresentation::kWord8:
      return cv.int8;
    case MachineRepresentation::kWord16:
      return cv.int16;
    case MachineRepresentation::kWord32:
      return cv.int32;
    case MachineRepresentation::kTaggedSigned:
    case MachineRepresentation::kTaggedPointer:
    case MachineRepresentation::kTagged:
      return cv.taggedType;
    default:
      UNREACHABLE();
  }
}

static const char* symbolLookupCallback(void* DisInfo, uint64_t ReferenceValue,
                                        uint64_t* ReferenceType,
                                        uint64_t ReferencePC,
                                        const char** ReferenceName) {
  *ReferenceType = LLVMDisassembler_ReferenceType_InOut_None;
  return nullptr;
}

static void disassemble(ByteBuffer& code) {
  LLVMDisasmContextRef DCR = LLVMCreateDisasm("armv7-linux-android", nullptr, 0,
                                              nullptr, symbolLookupCallback);

  uint8_t* BytesP = code.data();

  unsigned NumBytes = code.size();
  unsigned PC = 0;
  const char OutStringSize = 100;
  char OutString[OutStringSize];
  printf(
      "========================================================================"
      "========\n");
  while (NumBytes != 0) {
    size_t InstSize = LLVMDisasmInstruction(DCR, BytesP, NumBytes, PC,
                                            OutString, OutStringSize);
    if (InstSize == 0) {
      printf("%08x: %08x maybe constant\n", PC,
             *reinterpret_cast<uint32_t*>(BytesP));
      PC += 4;
      BytesP += 4;
      NumBytes -= 4;
    }
    printf("%08x: %08x %s\n", PC, *reinterpret_cast<uint32_t*>(BytesP),
           OutString);
    PC += InstSize;
    BytesP += InstSize;
    NumBytes -= InstSize;
  }
  printf(
      "========================================================================"
      "========\n");
  LLVMDisasmDispose(DCR);
}

static void disassemble(CompilerState& state) {
  for (auto& code : state.codeSectionList_) {
    disassemble(code);
  }
}

Handle<Code> V8PassManager::Run(Isolate* isolate, compiler::Schedule* schedule,
                                compiler::CallDescriptor* call_descriptor,
                                const char* name, Code::Kind kind) {
  static bool llvm_initialized = false;
#if 0
  std::cout << "name: " << name << "\n" << *schedule;
  std::cout.flush();
#endif
  if (!llvm_initialized) {
    tf_llvm::initLLVM();
    llvm_initialized = true;
  }
  tf_llvm::ScheduleEmitter llvm_emitter(isolate, schedule, call_descriptor);
  tf_llvm::BasicBlockManager BBM;
  {
    tf_llvm::LivenessAnalysisVisitor lav(BBM);
    llvm_emitter.Visit(&lav);
    lav.CalculateLivesIns();
#if 0
    for (auto& item : BBM) {
      tf_llvm::BasicBlock* bb = item.second.get();
      using namespace std;

      cout << "BasicBlock " << bb->id() << ": lives:";
      for (int live : bb->liveins()) {
        cout << " " << live;
      }
      cout << endl;
    }
#endif
  }
  do {
    tf_llvm::CompilerState compiler_state(name);
    compiler_state.code_kind_ = static_cast<int>(kind);
    compiler_state.needs_frame_ = BBM.needs_frame();
    if (call_descriptor->IsJSFunctionCall()) {
      compiler_state.prologue_kind_ = PrologueKind::JSFunctionCall;
    } else {
      compiler_state.prologue_kind_ = PrologueKind::Stub;
    }

    tf_llvm::Output output(compiler_state);
    tf_llvm::RegisterParameterDesc input_desc;
    for (int i = 1; i <= call_descriptor->ParameterCount(); ++i) {
      compiler::LinkageLocation location = call_descriptor->GetInputLocation(i);
      CHECK((location.IsRegister() && !location.IsAnyRegister()) ||
            location.IsCallerFrameSlot());
      input_desc.emplace_back(location.GetLocation(),
                              GetLLVMType(output.repo(), location.GetType()));
    }
    output.initializeBuild(
        input_desc, !call_descriptor->HasRestrictedAllocatableRegisters());
    tf_llvm::LLVMTFBuilder builder(output, BBM,
                                   compiler_state.stack_map_info_map_,
                                   compiler_state.load_constant_recorder_);
    llvm_emitter.Visit(&builder);
    builder.End();
#if 0
    tf_llvm::dumpModule(compiler_state.module_);
#endif
    tf_llvm::compile(compiler_state);
    const ByteBuffer& code = compiler_state.codeSectionList_.front();

#if 0
    disassemble(compiler_state);
#endif
    return tf_llvm::GenerateCode(isolate, compiler_state);
  } while (true);
}
}  // namespace tf_llvm
}  // namespace internal
}  // namespace v8
