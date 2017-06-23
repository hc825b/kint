//===----------------------------------------------------------------------===//
///
/// \file
/// This pass performs taint analysis
///
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "kint-taint"
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Module.h>
#include <llvm/Pass.h>

#include "Annotation.h"
#include "KINTGlobal.h"

using namespace llvm;

static inline StringRef asString(MDNode *MD) {
	if (MDString *S = dyn_cast_or_null<MDString>(MD->getOperand(0)))
		return S->getString();
	return "";
}

static inline MDString *toMDString(LLVMContext &VMCtx, DescSet *D) {
	std::string s;
	for (DescSet::iterator i = D->begin(), e = D->end(); i != e; ++i) {
		if (i != D->begin())
			s += ", ";
		s += (*i).str();
	}
	return MDString::get(VMCtx, s);
}

// Check both local taint and global sources
DescSet* KINTTaintPass::getTaint(Value *V) {
	if (DescSet *DS = TM.get(V))
		return DS;
	if (DescSet *DS = TM.get(V->stripPointerCasts()))
		return DS;

	// if value is not taint, check global taint.
	// For call, taint if any possible callee could return taint
	if (CallInst *CI = dyn_cast<CallInst>(V)) {
		if (!CI->isInlineAsm() && this->CalleesPtr->count(CI)) {
			const FuncSet &CEEs = this->CalleesPtr->lookup(CI);
			for (FuncSet::const_iterator i = CEEs.begin(), e = CEEs.end();
				 i != e; ++i) {
				if (DescSet *DS = TM.get(getRetId(*i)))
					TM.add(CI, *DS);
			}
		}
	}
	// For arguments and loads
	if (DescSet *DS = TM.get(getValueId(V)))
		TM.add(V, *DS);
	return TM.get(V);
}

// find and mark taint source
bool KINTTaintPass::checkTaintSource(Instruction *I)
{
	Module *M = I->getParent()->getParent()->getParent();
	bool changed = false;

	if (MDNode *MD = I->getMetadata(MD_TaintSrc)) {
		TM.add(I, asString(MD));
		DescSet &D = *TM.get(I);
		changed |= TM.add(getValueId(I), D, true);
		// mark all struct members as taint
		if (PointerType *PTy = dyn_cast<PointerType>(I->getType())) {
			if (StructType *STy = dyn_cast<StructType>(PTy->getElementType())) {
				for (unsigned i = 0; i < STy->getNumElements(); ++i)
					changed |= TM.add(getStructId(STy, M, i), D, true);
			}
		}
	}
	return changed;
}

// Propagate taint within a function
bool KINTTaintPass::runOnFunction(Function& F)
{
	bool changed = false;

	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
		Instruction *I = &*i;

		// find and mark taint sources
		changed |= checkTaintSource(I);

		// for call instruction, propagate taint to arguments instead
		// of from arguments
		if (CallInst *CI = dyn_cast<CallInst>(I)) {
			if (CI->isInlineAsm() || !this->CalleesPtr->count(CI))
				continue;

			const FuncSet &CEEs = this->CalleesPtr->lookup(CI);
			for (FuncSet::iterator j = CEEs.begin(), je = CEEs.end();
				 j != je; ++j) {
				// skip vararg and builtin functions
				if ((*j)->isVarArg()
					|| (*j)->getName().find('.') != StringRef::npos)
					continue;

				// mark corresponding args tainted on all possible callees
				for (unsigned a = 0; a < CI->getNumArgOperands(); ++a) {
					if (DescSet *DS = getTaint(CI->getArgOperand(a)))
						changed |= TM.add(getArgId(*j, a), *DS);
				}
			}
			continue;
		}

		// check if any operand is taint
		DescSet D;
		for (unsigned j = 0; j < I->getNumOperands(); ++j)
			if (DescSet *DS = getTaint(I->getOperand(j)))
				D.insert(DS->begin(), DS->end());
		if (D.empty())
			continue;

		// propagate value and global taint
		TM.add(I, D);
		if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
			if (MDNode *ID = SI->getMetadata(MD_ID))
				changed |= TM.add(asString(ID), D);
		} else if (isa<ReturnInst>(I)) {
			changed |= TM.add(getRetId(&F), D);
		}
	}
	return changed;
}

// write back
bool KINTTaintPass::doFinalization(Module &M) {
	LLVMContext &VMCtx = M.getContext();
	for (Module::iterator f = M.begin(), fe = M.end(); f != fe; ++f) {
		Function &F = *f;
		for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
			Instruction &I = *i;
			if (DescSet *DS = getTaint(&I)) {
				MDNode *MD = MDNode::get(VMCtx, toMDString(VMCtx, DS));
				I.setMetadata(MD_Taint, MD);
			} else
				I.setMetadata(MD_Taint, NULL);
		}
	}
	return true;
}

bool KINTTaintPass::runOnModule(Module& M) {
	TM.GTS.clear();
	TM.VTS.clear();

	this->CalleesPtr = &getAnalysis<KINTCallGraphPass>().getCalleeMap();

	bool changed = true, ret = false;

	while (changed) {
		changed = false;
		for (Module::iterator i = M.begin(), e = M.end(); i != e; ++i)
			changed |= runOnFunction(*i);
		ret |= changed;
	}
	return ret;
}

char KINTTaintPass::ID;

static RegisterPass<KINTTaintPass>
X("kint-taint", "Taint analysis for KINT");
