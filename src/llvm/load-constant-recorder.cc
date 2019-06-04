// Copyright 2019 UCWeb Co., Ltd.

#include "src/llvm/load-constant-recorder.h"
#include "src/llvm/log.h"

namespace v8 {
namespace internal {
namespace tf_llvm {
void LoadConstantRecorder::Register(int64_t magic,
                                    LoadConstantRecorder::Type type) {
  map_.insert(std::make_pair(magic, type));
}

LoadConstantRecorder::Type LoadConstantRecorder::Query(int64_t magic) const {
  if (magic == IsolateExternalReferenceMagic())
    return kIsolateExternalReference;
  else if (magic == RecordStubCodeConstantMagic())
    return kRecordStubCodeConstant;
  else if (magic == ModuloExternalReferenceMagic())
    return kModuloExternalReference;
  auto found = map_.find(magic);
  EMASSERT(found != map_.end());
  return found->second;
}

int64_t LoadConstantRecorder::IsolateExternalReferenceMagic() {
  return 0xfdfdfdfd;
}

int64_t LoadConstantRecorder::RecordStubCodeConstantMagic() {
  return 0xfefefefe;
}

int64_t LoadConstantRecorder::ModuloExternalReferenceMagic() {
  return 0xdfdfdfdf;
}
}  // namespace tf_llvm
}  // namespace internal
}  // namespace v8
