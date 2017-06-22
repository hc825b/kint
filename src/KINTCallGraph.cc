//===----------------------------------------------------------------------===//
///
/// \file
/// This pass computes Call Graph specific for KINT algorithm.
///
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "kint-cg"
#include <llvm/IR/Constants.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Module.h>
#include <llvm/Pass.h>

#include "Annotation.h"
#include "KINTGlobal.h"

using namespace llvm;

bool KINTCallGraphPass::doInitialization(Module& M) {
	// collect function pointer assignments in global initializers
	Module::global_iterator i, e;
	for (i = M.global_begin(), e = M.global_end(); i != e; ++i) {
		if (i->hasInitializer())
			processInitializers(M, i->getInitializer(), &*i);
	}

	// collect global function definitions
	for (Module::iterator f = M.begin(), fe = M.end(); f != fe; ++f) {
		if (f->hasExternalLinkage() && !f->empty())
		{
			this->Funcs[f->getName()] = &*f;
		}
	}
	// TODO Test if initialization is done correctly
	return true;
}

bool KINTCallGraphPass::doFinalization(Module& M) {
	// update callee mapping
	for (Module::iterator f = M.begin(), fe = M.end(); f != fe; ++f) {
		Function &F = *f;
		for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
			// map callsite to possible callees
			if (CallInst *CI = dyn_cast<CallInst>(&*i)) {
				FuncSet &FS = this->Callees[CI];
				findFunctions(CI->getCalledValue(), FS);
			}
		}
	}
	// TODO Test if finalization is done correctly
	return true;
}

bool KINTCallGraphPass::runOnModule(Module& M) {
	Callees.clear();

	for (Module::iterator i = M.begin(), e = M.end(); i != e; ++i)
		runOnFunction(*i);
	return false;
}

void KINTCallGraphPass::processInitializers(Module &M, Constant *I, GlobalValue *V) {
	// structs
	if (ConstantStruct *CS = dyn_cast<ConstantStruct>(I)) {
		StructType *STy = CS->getType();
		if (!STy->hasName())
			return;
		for (unsigned i = 0; i != STy->getNumElements(); ++i) {
			Type *ETy = STy->getElementType(i);
			if (ETy->isStructTy() || ETy->isArrayTy()) {
				// nested array or struct
				processInitializers(M, CS->getOperand(i), NULL);
			} else if (isFunctionPointer(ETy)) {
				// found function pointers in struct fields
				if (Function *F = dyn_cast<Function>(CS->getOperand(i))) {
					std::string Id = getStructId(STy, &M, i);
					this->FuncPtrs[Id].insert(F);
				}
			}
		}
	} else if (ConstantArray *CA = dyn_cast<ConstantArray>(I)) {
		// array of structs
		if (CA->getType()->getElementType()->isStructTy())
			for (unsigned i = 0; i != CA->getNumOperands(); ++i)
				processInitializers(M, CA->getOperand(i), NULL);
	} else if (Function *F = dyn_cast<Function>(I)) {
		// global function pointer variables
		if (V) {
			std::string Id = getVarId(V);
			this->FuncPtrs[Id].insert(F);
		}
	}
}

bool KINTCallGraphPass::runOnFunction(Function &F)
{
	bool Changed = false;
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {

		Instruction *I = &*i;

		if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
			// stores to function pointers
			Value *V = SI->getValueOperand();
			if (isFunctionPointer(V->getType())) {
				StringRef Id = getLoadStoreId(SI);
				if (!Id.empty())
					Changed |= findFunctions(V, this->FuncPtrs[Id]);
			}
		} else if (ReturnInst *RI = dyn_cast<ReturnInst>(I)) {
			// function returns
			if (isFunctionPointer(F.getReturnType())) {
				Value *V = RI->getReturnValue();
				std::string Id = getRetId(&F);
				Changed |= findFunctions(V, this->FuncPtrs[Id]);
			}
		} else if (CallInst *CI = dyn_cast<CallInst>(I)) {
			// ignore inline asm or intrinsic calls
			if (CI->isInlineAsm() || (CI->getCalledFunction()
					&& CI->getCalledFunction()->isIntrinsic()))
				continue;

			// might be an indirect call, find all possible callees
			FuncSet FS;
			if (!findFunctions(CI->getCalledValue(), FS))
				continue;

			// looking for function pointer arguments
			for (unsigned no = 0; no != CI->getNumArgOperands(); ++no) {
				Value *V = CI->getArgOperand(no);
				if (!isFunctionPointer(V->getType()))
					continue;

				// find all possible assignments to the argument
				FuncSet VS;
				if (!findFunctions(V, VS))
					continue;

				// update argument FP-set for possible callees
				for (FuncSet::iterator k = FS.begin(), ke = FS.end();
				        k != ke; ++k) {
					llvm::Function *CF = *k;
					std::string Id = getArgId(CF, no);
					Changed |= mergeFuncSet(this->FuncPtrs[Id], VS);
				}
			}
		}
	}
	return Changed;
}

bool KINTCallGraphPass::mergeFuncSet(FuncSet &S, const std::string &Id) {
	FuncPtrMap::iterator i = this->FuncPtrs.find(Id);
	if (i != this->FuncPtrs.end())
		return mergeFuncSet(S, i->second);
	return false;
}

bool KINTCallGraphPass::mergeFuncSet(FuncSet &Dst, const FuncSet &Src) {
	bool Changed = false;
	for (FuncSet::const_iterator i = Src.begin(), e = Src.end(); i != e; ++i)
		Changed |= Dst.insert(*i).second;
	return Changed;
}


bool KINTCallGraphPass::findFunctions(Value *V, FuncSet &S) {
	SmallPtrSet<Value *, 4> Visited;
	return findFunctions(V, S, Visited);
}

bool KINTCallGraphPass::findFunctions(Value *V, FuncSet &S,
                                  SmallPtrSet<Value *, 4> Visited) {
	if (!Visited.insert(V).second)
		return false;

	// real function, S = S + {F}
	if (Function *F = dyn_cast<Function>(V)) {
		if (!F->empty())
			return S.insert(F).second;

		// prefer the real definition to declarations
		FuncMap::iterator it = this->Funcs.find(F->getName());
		if (it != this->Funcs.end())
			return S.insert(it->second).second;
		else
			return S.insert(F).second;
	}

	// bitcast, ignore the cast
	if (BitCastInst *B = dyn_cast<BitCastInst>(V))
		return findFunctions(B->getOperand(0), S, Visited);

	// const bitcast, ignore the cast
	if (ConstantExpr *C = dyn_cast<ConstantExpr>(V)) {
		if (C->isCast())
			return findFunctions(C->getOperand(0), S, Visited);
	}

	// PHI node, recursively collect all incoming values
	if (PHINode *P = dyn_cast<PHINode>(V)) {
		bool Changed = false;
		for (unsigned i = 0; i != P->getNumIncomingValues(); ++i)
			Changed |= findFunctions(P->getIncomingValue(i), S, Visited);
		return Changed;
	}

	// select, recursively collect both paths
	if (SelectInst *SI = dyn_cast<SelectInst>(V)) {
		bool Changed = false;
		Changed |= findFunctions(SI->getTrueValue(), S, Visited);
		Changed |= findFunctions(SI->getFalseValue(), S, Visited);
		return Changed;
	}

	// arguement, S = S + FuncPtrs[arg.ID]
	if (Argument *A = dyn_cast<Argument>(V))
		return mergeFuncSet(S, getArgId(A));

	// return value, S = S + FuncPtrs[ret.ID]
	if (CallInst *CI = dyn_cast<CallInst>(V)) {
		if (Function *CF = CI->getCalledFunction())
			return mergeFuncSet(S, getRetId(CF));

		// TODO: handle indirect calls
		return false;
	}

	// loads, S = S + FuncPtrs[struct.ID]
	if (LoadInst *L = dyn_cast<LoadInst>(V))
		return mergeFuncSet(S, getLoadStoreId(L));

	// ignore other constant (usually null), inline asm and inttoptr
	if (isa<Constant>(V) || isa<InlineAsm>(V) || isa<IntToPtrInst>(V))
		return false;

	V->dump();
	report_fatal_error("findFunctions: unhandled value type\n");
	return false;
}

char KINTCallGraphPass::ID;

static RegisterPass<KINTCallGraphPass>
X("kint-cg", "Compute call graph for KINT", false, true);
