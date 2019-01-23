#include "src/llvm/liveness-analysis-visitor.h"
#include <algorithm>
#include <deque>
#include <iterator>
#include <unordered_set>
#include "src/llvm/basic-block-manager.h"
#include "src/llvm/basic-block.h"
#include "src/llvm/log.h"

namespace v8 {
namespace internal {
namespace tf_llvm {
namespace {
struct PhiDesc {
  int from;
  int value;
};

struct LivenessBasicBlockImpl {
  std::vector<PhiDesc> phis;
  std::set<int> defines;
};

static inline LivenessBasicBlockImpl* GetImpl(BasicBlock* bb) {
  return bb->GetImpl<LivenessBasicBlockImpl>();
}

static bool CompareLiveins(const std::vector<int>& left,
                           const std::vector<int>& right) {
  if (left.size() != right.size()) return true;
  auto left_iterator = left.begin();
  auto right_iterator = right.begin();
  for (; left_iterator != left.end(); ++left_iterator, ++right_iterator) {
    if (*left_iterator != *right_iterator) return true;
  }
  return false;
}
}  // namespace
LivenessAnalysisVisitor::LivenessAnalysisVisitor(BasicBlockManager& bbm)
    : basic_block_manager_(&bbm), current_basic_block_(nullptr) {}

void LivenessAnalysisVisitor::AddIfNotInDefines(int id) {
  auto found = current_defines_.find(id);
  if (found == current_defines_.end()) {
    current_references_.insert(id);
  }
}

void LivenessAnalysisVisitor::Define(int id) { current_defines_.insert(id); }

void LivenessAnalysisVisitor::EndBlock() {
  std::copy(current_references_.begin(), current_references_.end(),
            std::back_inserter(current_basic_block_->liveins()));
  GetImpl(current_basic_block_)->defines.swap(current_defines_);
  current_basic_block_ = nullptr;
  current_references_.clear();
}

void LivenessAnalysisVisitor::CalculateLivesIns() {
  if (current_basic_block_) {
    EndBlock();
  }
  std::deque<int> worklist;
  std::copy(basicBlockManager().rpo().rbegin(),
            basicBlockManager().rpo().rend(), std::back_inserter(worklist));
  while (!worklist.empty()) {
    int id = worklist.front();
    worklist.pop_front();
    BasicBlock* now = basicBlockManager().findBB(id);
    std::vector<int> liveins(now->liveins());
    for (auto successor : now->successors()) {
      std::vector<int> result;
      std::set_union(liveins.begin(), liveins.end(),
                     successor->liveins().begin(), successor->liveins().end(),
                     std::back_inserter(result));
      auto& phis = GetImpl(successor)->phis;
      for (auto& phi : phis) {
        if (phi.from == now->id()) {
          auto insert_point =
              std::lower_bound(result.begin(), result.end(), phi.value);
          if (insert_point == result.end()) {
            result.push_back(phi.value);
          } else if (*insert_point != phi.value) {
            result.insert(insert_point, phi.value);
          }
        }
      }
      liveins.swap(result);
    }
    // clear those defined in this basic block.
    std::vector<int> result;
    std::copy_if(
        liveins.begin(), liveins.end(), std::back_inserter(result),
        [&](int value) {
          if (GetImpl(now)->defines.end() == GetImpl(now)->defines.find(value))
            return true;
          return false;
        });

    if (CompareLiveins(now->liveins(), result)) {
      // FIXME: use marker to optimize.
      for (auto pred : now->predecessors()) {
        worklist.push_back(pred->id());
      }
    }
    now->liveins().swap(result);
  }
// add back the use of phi in this pass
#if 0
  for (auto it = basicBlockManager().rpo().begin();
       it != basicBlockManager().rpo().end(); ++it) {
    BasicBlock* now = basicBlockManager().findBB(*it);
    auto& phis = GetImpl(now)->phis;
    auto& liveins = now->liveins();
    for (auto& phi : phis) {
      auto insert_point =
          std::lower_bound(liveins.begin(), liveins.end(), phi.value);
      if (insert_point == liveins.end()) {
        liveins.push_back(phi.value);
      } else if (*insert_point != phi.value) {
        liveins.insert(insert_point, phi.value);
      }
    }
  }
#endif
  ResetImpls<LivenessBasicBlockImpl>(basicBlockManager());
}

void LivenessAnalysisVisitor::VisitBlock(int id, bool is_deferred,
                                         const OperandsVector& predecessors) {
  if (current_basic_block_) {
    EndBlock();
  }
  BasicBlock* bb = basicBlockManager().ensureBB(id);
  for (int predecessor : predecessors) {
    BasicBlock* pred_bb = basicBlockManager().ensureBB(predecessor);
    bb->AddPredecessor(pred_bb);
  }
  current_basic_block_ = bb;
  current_basic_block_->set_deffered(is_deferred);
  basicBlockManager().rpo().push_back(id);
  std::unique_ptr<LivenessBasicBlockImpl> bb_impl(new LivenessBasicBlockImpl);
  bb->SetImpl(bb_impl.release());
}

void LivenessAnalysisVisitor::VisitGoto(int bid) {
  BasicBlock* successor = basicBlockManager().ensureBB(bid);
  current_basic_block_->successors().push_back(successor);
  EndBlock();
}

void LivenessAnalysisVisitor::VisitParameter(int id, int pid) { Define(id); }

void LivenessAnalysisVisitor::VisitLoadParentFramePointer(int id) {
  Define(id);
}

void LivenessAnalysisVisitor::VisitLoadFramePointer(int id) { Define(id); }

void LivenessAnalysisVisitor::VisitLoadStackPointer(int id) { Define(id); }

void LivenessAnalysisVisitor::VisitDebugBreak(int id) {}

void LivenessAnalysisVisitor::VisitInt32Constant(int id, int32_t value) {
  Define(id);
}

void LivenessAnalysisVisitor::VisitFloat64SilenceNaN(int id, int value) {
  Define(id);
  AddIfNotInDefines(value);
}

void LivenessAnalysisVisitor::VisitIdentity(int id, int value) {
  Define(id);
  AddIfNotInDefines(value);
}

void LivenessAnalysisVisitor::VisitLoad(int id, MachineRepresentation rep,
                                        MachineSemantic semantic, int base,
                                        int offset) {
  Define(id);
  AddIfNotInDefines(base);
  AddIfNotInDefines(offset);
}

void LivenessAnalysisVisitor::VisitStore(int id, MachineRepresentation rep,
                                         WriteBarrierKind barrier, int base,
                                         int offset, int value) {
  Define(id);
  AddIfNotInDefines(base);
  AddIfNotInDefines(offset);
  AddIfNotInDefines(value);
  if (barrier != kNoWriteBarrier) basicBlockManager().set_needs_frame(true);
}
void LivenessAnalysisVisitor::VisitBitcastWordToTagged(int id, int e) {
  Define(id);
  AddIfNotInDefines(e);
}

void LivenessAnalysisVisitor::VisitChangeInt32ToFloat64(int id, int e) {
  Define(id);
  AddIfNotInDefines(e);
}

void LivenessAnalysisVisitor::VisitChangeFloat32ToFloat64(int id, int e) {
  Define(id);
  AddIfNotInDefines(e);
}

void LivenessAnalysisVisitor::VisitChangeUint32ToFloat64(int id, int e) {
  Define(id);
  AddIfNotInDefines(e);
}

void LivenessAnalysisVisitor::VisitTruncateFloat64ToWord32(int id, int e) {
  Define(id);
  AddIfNotInDefines(e);
}

void LivenessAnalysisVisitor::VisitTruncateFloat64ToFloat32(int id, int e) {
  Define(id);
  AddIfNotInDefines(e);
}

void LivenessAnalysisVisitor::VisitRoundFloat64ToInt32(int id, int e) {
  Define(id);
  AddIfNotInDefines(e);
}

void LivenessAnalysisVisitor::VisitFloat64ExtractHighWord32(int id, int e) {
  Define(id);
  AddIfNotInDefines(e);
}

void LivenessAnalysisVisitor::VisitRoundInt32ToFloat32(int id, int e) {
  Define(id);
  AddIfNotInDefines(e);
}

void LivenessAnalysisVisitor::VisitInt32Add(int id, int e1, int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitFloat64Add(int id, int e1, int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitFloat64Sub(int id, int e1, int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitFloat64Mul(int id, int e1, int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitFloat64Div(int id, int e1, int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitFloat64Mod(int id, int e1, int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitFloat64LessThan(int id, int e1, int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitFloat64LessThanOrEqual(int id, int e1,
                                                          int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitFloat64Equal(int id, int e1, int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitFloat64Neg(int id, int e) {
  Define(id);
  AddIfNotInDefines(e);
}

void LivenessAnalysisVisitor::VisitFloat64Abs(int id, int e) {
  Define(id);
  AddIfNotInDefines(e);
}

void LivenessAnalysisVisitor::VisitProjection(int id, int e, int index) {
  Define(id);
  AddIfNotInDefines(e);
}

void LivenessAnalysisVisitor::VisitInt32AddWithOverflow(int id, int e1,
                                                        int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitInt32SubWithOverflow(int id, int e1,
                                                        int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitInt32Sub(int id, int e1, int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitInt32Mul(int id, int e1, int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitInt32Div(int id, int e1, int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitInt32Mod(int id, int e1, int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitInt32MulWithOverflow(int id, int e1,
                                                        int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitWord32Shl(int id, int e1, int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitWord32Xor(int id, int e1, int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitWord32Shr(int id, int e1, int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitWord32Sar(int id, int e1, int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitWord32Mul(int id, int e1, int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitWord32And(int id, int e1, int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitWord32Or(int id, int e1, int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitWord32Equal(int id, int e1, int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitInt32LessThanOrEqual(int id, int e1,
                                                        int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitInt32LessThan(int id, int e1, int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitUint32LessThanOrEqual(int id, int e1,
                                                         int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitUint32LessThan(int id, int e1, int e2) {
  Define(id);
  AddIfNotInDefines(e1);
  AddIfNotInDefines(e2);
}

void LivenessAnalysisVisitor::VisitBranch(int id, int cmp, int btrue,
                                          int bfalse) {
  AddIfNotInDefines(cmp);
  BasicBlock* bb_true = basicBlockManager().ensureBB(btrue);
  BasicBlock* bb_false = basicBlockManager().ensureBB(bfalse);
  current_basic_block_->successors().push_back(bb_true);
  current_basic_block_->successors().push_back(bb_false);
  EndBlock();
}

void LivenessAnalysisVisitor::VisitSwitch(int id, int val,
                                          const OperandsVector& successors) {
  AddIfNotInDefines(val);
  for (int successor : successors) {
    BasicBlock* bb = basicBlockManager().ensureBB(successor);
    current_basic_block_->successors().push_back(bb);
  }
  EndBlock();
}

void LivenessAnalysisVisitor::VisitIfValue(int id, int val) {}

void LivenessAnalysisVisitor::VisitIfDefault(int id) {}

void LivenessAnalysisVisitor::VisitHeapConstant(int id, int64_t magic) {
  Define(id);
}

void LivenessAnalysisVisitor::VisitExternalConstant(int id, int64_t magic) {
  Define(id);
}

void LivenessAnalysisVisitor::VisitPhi(int id, MachineRepresentation rep,
                                       const OperandsVector& operands) {
  Define(id);
  int i = 0;
  for (BasicBlock* pred : current_basic_block_->predecessors()) {
    int value = operands[i++];
    GetImpl(current_basic_block_)->phis.push_back({pred->id(), value});
  }
}

void LivenessAnalysisVisitor::VisitCall(int id, bool code,
                                        const CallDescriptor&,
                                        const OperandsVector& operands) {
  Define(id);
  for (int e : operands) {
    AddIfNotInDefines(e);
  }
  basicBlockManager().set_needs_frame(true);
}

void LivenessAnalysisVisitor::VisitCallWithCallerSavedRegisters(
    int id, const OperandsVector& operands) {
  Define(id);
  for (int operand : operands) {
    AddIfNotInDefines(operand);
  }
}

void LivenessAnalysisVisitor::VisitTailCall(int id, bool code,
                                            const CallDescriptor&,
                                            const OperandsVector& operands) {
  for (int e : operands) {
    AddIfNotInDefines(e);
  }
  EndBlock();
}

void LivenessAnalysisVisitor::VisitRoot(int id, int) { Define(id); }
void LivenessAnalysisVisitor::VisitCodeForCall(int id, int64_t) { Define(id); }
void LivenessAnalysisVisitor::VisitSmiConstant(int id, void*) { Define(id); }
void LivenessAnalysisVisitor::VisitFloat64Constant(int id, double) {
  Define(id);
}

void LivenessAnalysisVisitor::VisitReturn(int id, int pop_count,
                                          const OperandsVector& operands) {
  Define(id);
  Define(pop_count);
  for (int e : operands) {
    AddIfNotInDefines(e);
  }
  EndBlock();
}
}  // namespace tf_llvm
}  // namespace internal
}  // namespace v8