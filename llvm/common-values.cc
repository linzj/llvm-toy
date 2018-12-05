#include "common-values.h"

namespace jit {

CommonValues::CommonValues(LContext context)
    : voidType(jit::voidType(context)),
      boolean(int1Type(context)),
      int1(int1Type(context)),
      int8(int8Type(context)),
      int16(int16Type(context)),
      int32(int32Type(context)),
      int64(int64Type(context)),
      intPtr(intPtrType(context)),
      floatType(jit::floatType(context)),
      doubleType(jit::doubleType(context)),
      tokenType(LLVMTokenTypeInContext(context)),
      ref8(pointerType(int8)),
      ref16(pointerType(int16)),
      ref32(pointerType(int32)),
      ref64(pointerType(int64)),
      refPtr(pointerType(intPtr)),
      refFloat(pointerType(floatType)),
      refDouble(pointerType(doubleType)),
      // address space 1 means gc recognizable.
      taggedType(
          LLVMPointerType(LLVMStructCreateNamed(context, "TaggedStruct"), 1)),
      booleanTrue(constInt(boolean, true, ZeroExtend)),
      booleanFalse(constInt(boolean, false, ZeroExtend)),
      int8Zero(constInt(int8, 0, SignExtend)),
      int32Zero(constInt(int32, 0, SignExtend)),
      int32One(constInt(int32, 1, SignExtend)),
      int64Zero(constInt(int64, 0, SignExtend)),
      intPtrZero(constInt(intPtr, 0, SignExtend)),
      intPtrOne(constInt(intPtr, 1, SignExtend)),
      intPtrTwo(constInt(intPtr, 2, SignExtend)),
      intPtrThree(constInt(intPtr, 3, SignExtend)),
      intPtrFour(constInt(intPtr, 4, SignExtend)),
      intPtrEight(constInt(intPtr, 8, SignExtend)),
      intPtrPtr(constInt(intPtr, sizeof(void*), SignExtend)),
      doubleZero(constReal(doubleType, 0)),
      rangeKind(mdKindID(context, "range")),
      profKind(mdKindID(context, "prof")),
      branchWeights(mdString(context, "branch_weights")),
      context_(context),
      module_(0) {}
}  // namespace jit
