#ifndef INTRINSIC_REPOSITORY_H
#define INTRINSIC_REPOSITORY_H
#include "src/llvm/common-values.h"

// clang-format off
#define FOR_EACH_FTL_INTRINSIC(macro) \
    macro(ceil64, "llvm.ceil.f64", functionType(doubleType, doubleType)) \
    macro(ctlz32, "llvm.ctlz.i32", functionType(int32, int32, boolean)) \
    macro(addWithOverflow32, "llvm.sadd.with.overflow.i32", functionType(structType(context_, int32, boolean), int32, int32)) \
    macro(addWithOverflow64, "llvm.sadd.with.overflow.i64", functionType(structType(context_, int64, boolean), int64, int64)) \
    macro(doubleAbs, "llvm.fabs.f64", functionType(doubleType, doubleType)) \
    macro(doubleSin, "llvm.sin.f64", functionType(doubleType, doubleType)) \
    macro(doubleCos, "llvm.cos.f64", functionType(doubleType, doubleType)) \
    macro(doublePow, "llvm.pow.f64", functionType(doubleType, doubleType, doubleType)) \
    macro(doublePowi, "llvm.powi.f64", functionType(doubleType, doubleType, int32)) \
    macro(doubleSqrt, "llvm.sqrt.f64", functionType(doubleType, doubleType)) \
    macro(doubleLog, "llvm.log.f64", functionType(doubleType, doubleType)) \
    macro(frameAddress, "llvm.frameaddress", functionType(pointerType(int8), int32)) \
    macro(mulWithOverflow32, "llvm.smul.with.overflow.i32", functionType(structType(context_, int32, boolean), int32, int32)) \
    macro(mulWithOverflow64, "llvm.smul.with.overflow.i64", functionType(structType(context_, int64, boolean), int64, int64)) \
    macro(patchpointInt64, "llvm.experimental.patchpoint.i64", functionType(int64, int64, int32, ref8, int32, Variadic)) \
    macro(patchpointVoid, "llvm.experimental.patchpoint.void", functionType(voidType, int64, int32, ref8, int32, Variadic)) \
    macro(stackmap, "llvm.experimental.stackmap", functionType(voidType, int64, int32, Variadic)) \
    macro(gcResult, "llvm.experimental.gc.result.p1s_TaggedStructs", functionType(taggedType, tokenType)) \
    macro(gcResult2, "llvm.experimental.gc.result.sl_p1s_TaggedStructsp1s_TaggedStructss", functionType(structType(context_, taggedType, taggedType), tokenType)) \
    macro(gcRelocate, "llvm.experimental.gc.relocate.p1s_TaggedStructs", functionType(taggedType, tokenType, int32, int32)) \
    macro(subWithOverflow32, "llvm.ssub.with.overflow.i32", functionType(structType(context_, int32, boolean), int32, int32)) \
    macro(subWithOverflow64, "llvm.ssub.with.overflow.i64", functionType(structType(context_, int64, boolean), int64, int64)) \
    macro(stackSave, "llvm.stacksave", functionType(ref8)) \
    macro(trap, "llvm.trap", functionType(voidType)) \
    macro(x86SSE2CvtTSD2SI, "llvm.x86.sse2.cvttsd2si", functionType(int32, vectorType(doubleType, 2))) \
    macro(expect, "llvm.expect.i1", functionType(int1, int1, int1))

// clang-format on
namespace v8 {
namespace internal {
namespace tf_llvm {
class IntrinsicRepository : public CommonValues {
 public:
  IntrinsicRepository(LContext, LModule);

#define INTRINSIC_GETTER(ourName, llvmName, type)      \
  LLVMValueRef ourName##Intrinsic() {                  \
    if (!m_##ourName) return ourName##IntrinsicSlow(); \
    return m_##ourName;                                \
  }
  FOR_EACH_FTL_INTRINSIC(INTRINSIC_GETTER)
#undef INTRINSIC_GETTER
 private:
#define INTRINSIC_GETTER_SLOW_DECLARATION(ourName, llvmName, type) \
  LLVMValueRef ourName##IntrinsicSlow();
  FOR_EACH_FTL_INTRINSIC(INTRINSIC_GETTER_SLOW_DECLARATION)
#undef INTRINSIC_GETTER

#define INTRINSIC_FIELD_DECLARATION(ourName, llvmName, type) \
  LLVMValueRef m_##ourName;
  FOR_EACH_FTL_INTRINSIC(INTRINSIC_FIELD_DECLARATION)
#undef INTRINSIC_FIELD_DECLARATION
};
}  // namespace tf_llvm
}  // namespace internal
}  // namespace v8
#endif  // INTRINSIC_REPOSITORY_H