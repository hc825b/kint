//===----------------------------------------------------------------------===//
///
/// \file
/// This pass performs Value Range analysis
///
/// todo Rewrite KINTRangePass with InstVisitor to dynamic dispatch
//===----------------------------------------------------------------------===//

#include <llvm/ADT/DenseSet.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/CFG.h>
#include <llvm/Analysis/MemoryDependenceAnalysis.h>
#include <llvm/Analysis/MemoryLocation.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Module.h>
#include <llvm/Pass.h>
#include <llvm/PassAnalysisSupport.h>

#include "Annotation.h"
#include "KINTGlobal.h"

using namespace llvm;

static cl::opt<std::string>
WatchID("w", cl::desc("Watch sID"), 
			   cl::value_desc("sID"));

static llvm::MemoryLocation getLocation(const llvm::Value* V) {
	if (const llvm::GlobalObject *GV = dyn_cast<const llvm::GlobalObject>(V))
		return getLocation(GV);
	else if (const llvm::CallInst *CI = dyn_cast<const llvm::CallInst>(V))
		return getLocation(CI);
	else if (const llvm::Instruction *I = dyn_cast<const llvm::Instruction>(V))
		return MemoryLocation::get(I);
	llvm_unreachable("Value does not represent a memory location.");
	return MemoryLocation(V);
}

static llvm::MemoryLocation getLocation(const llvm::GlobalObject *GV) {
	// Create MemoryLocation for the given GlobalVariable or Function Summary
	// XXX need to know if MemoryLoction created is consistent to store/load instruction.
	AAMDNodes N;
	N.TBAA  =   GV->getMetadata(LLVMContext::MD_tbaa);
	N.Scope =   GV->getMetadata(LLVMContext::MD_alias_scope);
	N.NoAlias = GV->getMetadata(LLVMContext::MD_noalias);
	const auto& DL = GV->getParent()->getDataLayout();
	return MemoryLocation(GV, DL.getTypeStoreSize(GV->getValueType()), N);
}

/// Return the memory location representing the summary of the called functions
static llvm::MemoryLocation getLocation(const llvm::CallInst *CI) {
	return getLocation(CI->getCalledFunction());
}

namespace {

class KINTRangePass : public llvm::ModulePass {
private:
	const unsigned MaxIterations;
	const CalleeMap* CalleesPtr;
	const TaintMap*  TMPtr;
	const llvm::TargetLibraryInfo* TLIPtr;
	llvm::AAResults* AARPtr;

	bool safeUnion(CRange &CR, const CRange &R);
	bool unionRange(const llvm::MemoryLocation&, const CRange &, llvm::Value *);
	bool unionRange(llvm::BasicBlock *, llvm::Value *, const CRange &);
	CRange getRange(llvm::BasicBlock *, llvm::Value *);

	void collectInitializers(llvm::GlobalVariable *, llvm::Constant *);
	bool updateRangeFor(llvm::Function &);
	bool updateRangeFor(llvm::BasicBlock *);
	bool updateRangeFor(llvm::Instruction *);

	typedef std::map<llvm::Value *, CRange> ValueRangeMap;
	typedef std::map<llvm::BasicBlock *, ValueRangeMap> FuncValueRangeMaps;
	FuncValueRangeMaps FuncVRMs;

	typedef llvm::DenseSet<llvm::MemoryLocation> ChangeSet;
	ChangeSet Changes;

	typedef std::pair<const llvm::BasicBlock *, const llvm::BasicBlock *> Edge;
	typedef llvm::SmallVector<Edge, 16> EdgeList;
	EdgeList BackEdges;

	// Ranges
	typedef llvm::DenseMap<llvm::MemoryLocation, CRange> LocRangeMap;
	LocRangeMap IntRanges;

	bool isBackEdge(const Edge &);

	CRange visitBinaryOp(llvm::BinaryOperator *);
	CRange visitCastInst(llvm::CastInst *);
	CRange visitSelectInst(llvm::SelectInst *);
	CRange visitPHINode(llvm::PHINode *);
	CRange visitLoadInst(llvm::LoadInst *);

	bool visitCallInst(llvm::CallInst *);
	bool visitReturnInst(llvm::ReturnInst *);
	bool visitStoreInst(llvm::StoreInst *);

	void visitBranchInst(llvm::BranchInst *,
						 llvm::BasicBlock *, ValueRangeMap &);
	void visitTerminator(llvm::TerminatorInst *,
						 llvm::BasicBlock *, ValueRangeMap &);
	void visitSwitchInst(llvm::SwitchInst *,
						 llvm::BasicBlock *, ValueRangeMap &);

public:
	static char ID;

	KINTRangePass(): llvm::ModulePass(ID), MaxIterations(5),
			CalleesPtr(nullptr), TMPtr(nullptr), TLIPtr(nullptr), AARPtr(nullptr) {}

	virtual void getAnalysisUsage(llvm::AnalysisUsage& AU) const {
		AU.setPreservesCFG();
		AU.addRequired<KINTCallGraphPass>();
		AU.addRequired<KINTTaintPass>();
		AU.addRequired<TargetLibraryInfoWrapperPass>();
		AU.addRequired<AAResultsWrapperPass>();
	}

	virtual bool doInitialization(llvm::Module&);
	virtual bool doFinalization(llvm::Module&);
	virtual bool runOnModule(llvm::Module&);
};

}

bool KINTRangePass::unionRange(const MemoryLocation& L, const CRange &R,
						   Value *V = NULL)
{
	if (R.isEmptySet())
		return false;
	const StringRef& sID = getValueId(L.Ptr);
	if (WatchID == sID && V) {
		if (Instruction *I = dyn_cast<Instruction>(V))
			dbgs() << I->getParent()->getParent()->getName() << "(): ";
		V->print(dbgs());
		dbgs() << "\n";
	}
	
	bool changed = true;
	LocRangeMap::iterator it = this->IntRanges.find(L);
	if (it != this->IntRanges.end()) {
		changed = it->second.safeUnion(R);
		if (changed && sID == WatchID)
			dbgs() << sID << " + " << R << " = " << it->second << "\n";
	} else {
		this->IntRanges.insert(std::make_pair(L, R));
		if (sID == WatchID)
			dbgs() << sID << " = " << R << "\n";
	}
	if (changed)
		Changes.insert(L);
	return changed;
}

bool KINTRangePass::unionRange(BasicBlock *BB, Value *V,
						   const CRange &R)
{
	if (R.isEmptySet())
		return false;
	
	bool changed = true;
	ValueRangeMap &VRM = FuncVRMs[BB];
	ValueRangeMap::iterator it = VRM.find(V);
	if (it != VRM.end())
		changed = it->second.safeUnion(R);
	else
		VRM.insert(std::make_pair(V, R));
	return changed;
}

CRange KINTRangePass::getRange(BasicBlock *BB, Value *V)
{
	// constants
	if (ConstantInt *C = dyn_cast<ConstantInt>(V))
		return CRange(C->getValue());
	
	ValueRangeMap &VRM = FuncVRMs[BB];
	ValueRangeMap::iterator invrm = VRM.find(V);
	
	if (invrm != VRM.end())
		return invrm->second;
	
	// V must be integer or pointer to integer
	IntegerType *Ty = dyn_cast<IntegerType>(V->getType());
	if (PointerType *PTy = dyn_cast<PointerType>(V->getType()))
		Ty = dyn_cast<IntegerType>(PTy->getElementType());
	assert(Ty != NULL);
	
	// not found in VRM, lookup global range, return empty set by default
	CRange CR(Ty->getBitWidth(), false);
	CRange Fullset(Ty->getBitWidth(), true);
	
	LocRangeMap &IRM = this->IntRanges;

	if (CallInst *CI = dyn_cast<CallInst>(V)) {
		// calculate union of values ranges returned by all possible callees
		if (!CI->isInlineAsm() && this->CalleesPtr->count(CI)) {
			const FuncSet &CEEs = this->CalleesPtr->lookup(CI);
			for (FuncSet::const_iterator i = CEEs.begin(), e = CEEs.end();
				 i != e; ++i) {
				std::string sID = getRetId(*i);
				if (sID != "" && this->TMPtr->isSource(sID)) {
					CR = Fullset;
					break;
				}
				LocRangeMap::iterator it;
				if ((it = IRM.find(getLocation(*i))) != IRM.end())
					CR.safeUnion(it->second);
			}
		}
	} else {
		// arguments & loads
		std::string sID = getValueId(V);
		if (sID != "") {
			LocRangeMap::iterator it;
			if (this->TMPtr->isSource(sID))
				CR = Fullset;
			else if ((it = IRM.find(getLocation(V))) != IRM.end())
				CR = it->second;
		}
		// might load part of a struct field
		CR = CR.zextOrTrunc(Ty->getBitWidth());
	}
	if (!CR.isEmptySet())
		VRM.insert(std::make_pair(V, CR));
	return CR;
}

void KINTRangePass::collectInitializers(GlobalVariable *GV, Constant *I)
{	
	// global var
	if (ConstantInt *CI = dyn_cast<ConstantInt>(I)) {
		unionRange(getLocation(GV), CI->getValue(), GV);
	}
	
	// structs
	if (ConstantStruct *CS = dyn_cast<ConstantStruct>(I)) {
		// Find integer fields in the struct
		StructType *ST = CS->getType();
		// Skip anonymous structs
		if (!ST->hasName() || ST->getName() == "struct.anon" 
			|| ST->getName().startswith("struct.anon."))
			return;
		
		for (unsigned i = 0; i != ST->getNumElements(); ++i) {
			Type *Ty = ST->getElementType(i);
			if (Ty->isStructTy()) {
				// nested struct
				// TODO: handle nested arrays
				collectInitializers(GV, CS->getOperand(i));
			} else if (Ty->isIntegerTy()) {
				// TODO Cannot handle global struct with constant member for now
#if 0
				ConstantInt *CI = 
					dyn_cast<ConstantInt>(I->getOperand(i));
				StringRef sID = getStructId(ST, GV->getParent(), i);
				if (!sID.empty() && CI)
					unionRange(sID, CI->getValue(), GV);
#endif
			}
		}
	}

	// arrays
	if (ConstantArray *CA = dyn_cast<ConstantArray>(I)) {
		Type *Ty = CA->getType()->getElementType();
		if (Ty->isStructTy() || Ty->isIntegerTy()) {
			for (unsigned i = 0; i != CA->getNumOperands(); ++i)
				collectInitializers(GV, CA->getOperand(i));
		}
	}
}

//
// Handle integer assignments in global initializers
//
bool KINTRangePass::doInitialization(Module &M)
{
	// Looking for global variables
	for (Module::global_iterator i = M.global_begin(),
		 e = M.global_end(); i != e; ++i) {

		// skip strings literals
		if (i->hasInitializer() && !i->getName().startswith("."))
			collectInitializers(&*i, i->getInitializer());
	}
	return true;
}

CRange KINTRangePass::visitBinaryOp(BinaryOperator *BO)
{
	CRange L = getRange(BO->getParent(), BO->getOperand(0));
	CRange R = getRange(BO->getParent(), BO->getOperand(1));
	R.match(L);
	switch (BO->getOpcode()) {
		default: BO->dump(); llvm_unreachable("Unknown binary operator!");
			return CRange(L.getBitWidth(), true); // return full set
		case Instruction::Add:  return L.add(R);
		case Instruction::Sub:  return L.sub(R);
		case Instruction::Mul:  return L.multiply(R);
		case Instruction::UDiv: return L.udiv(R);
		case Instruction::SDiv: return L.sdiv(R);
		case Instruction::URem: return R; // FIXME
		case Instruction::SRem: return R; // FIXME
		case Instruction::Shl:  return L.shl(R);
		case Instruction::LShr: return L.lshr(R);
		case Instruction::AShr: return L; // FIXME
		case Instruction::And:  return L.binaryAnd(R);
		case Instruction::Or:   return L.binaryOr(R);
		case Instruction::Xor:  return L; // FIXME
	}
}

CRange KINTRangePass::visitCastInst(CastInst *CI)
{
	unsigned bits = dyn_cast<IntegerType>(
								CI->getDestTy())->getBitWidth();
	
	BasicBlock *BB = CI->getParent();
	Value *V = CI->getOperand(0);
	switch (CI->getOpcode()) {
		case CastInst::Trunc:    return getRange(BB, V).zextOrTrunc(bits);
		case CastInst::ZExt:     return getRange(BB, V).zextOrTrunc(bits);
		case CastInst::SExt:     return getRange(BB, V).signExtend(bits);
		case CastInst::BitCast:  return getRange(BB, V);
		default:                 return CRange(bits, true);
	}
}

CRange KINTRangePass::visitSelectInst(SelectInst *SI)
{
	CRange T = getRange(SI->getParent(), SI->getTrueValue());
	CRange F = getRange(SI->getParent(), SI->getFalseValue());
	T.safeUnion(F);
	return T;
}

CRange KINTRangePass::visitPHINode(PHINode *PHI)
{
	IntegerType *Ty = cast<IntegerType>(PHI->getType());
	CRange CR(Ty->getBitWidth(), false);
	
	for (unsigned i = 0, n = PHI->getNumIncomingValues(); i < n; ++i) {
		BasicBlock *Pred = PHI->getIncomingBlock(i);
		// skip back edges
		if (isBackEdge(Edge(Pred, PHI->getParent())))
			continue;
		CR.safeUnion(getRange(Pred, PHI->getIncomingValue(i)));
	}
	return CR;
}

// Load and union Ranges from all possible aliased memory locations
CRange KINTRangePass::visitLoadInst(llvm::LoadInst *LI) {
	IntegerType *Ty = dyn_cast<IntegerType>(LI->getType());
	assert(Ty != nullptr);
	// FIXME This algorithm sucks. We need a more efficient one to get
	// aliased memory locations.
	for(LocRangeMap::iterator it = IntRanges.begin();
			it!=IntRanges.end(); ++it)
	{
		ModRefInfo MRI = AARPtr->getModRefInfo(LI, it->first);
		switch(MRI)
		{
			case MRI_Ref:
				unionRange(LI->getParent(), LI, it->second);
				break;
			case MRI_Mod:
			case MRI_ModRef:
				llvm_unreachable("Load Instruction shouldn't modify memory");
				break;
			default:
				break;
		}
	}
	return getRange(LI->getParent(), LI);
}

bool KINTRangePass::visitCallInst(CallInst *CI)
{
	bool changed = false;
	if (CI->isInlineAsm() || this->CalleesPtr->count(CI) == 0)
		return false;

	// update arguments of all possible callees
	const FuncSet &CEEs = this->CalleesPtr->lookup(CI);
	for (FuncSet::const_iterator i = CEEs.begin(), e = CEEs.end(); i != e; ++i) {
		// skip vararg and builtin functions
		if ((*i)->isVarArg() 
			|| (*i)->getName().find('.') != StringRef::npos)
			continue;
		
		for (unsigned j = 0; j < CI->getNumArgOperands(); ++j) {
			Value *V = CI->getArgOperand(j);
			// skip non-integer arguments
			if (!V->getType()->isIntegerTy())
				continue;
			// XXX std::string sID = getArgId(*i, j);
			MemoryLocation L
				= MemoryLocation::getForArgument(ImmutableCallSite(CI), j, *TLIPtr);
			changed |= unionRange(L, getRange(CI->getParent(), V), CI);
		}
	}

	// range for the return value of this call site
	if (CI->getType()->isIntegerTy())
		changed |= unionRange(getLocation(CI), getRange(CI->getParent(), CI), CI);
	return changed;
}

bool KINTRangePass::visitStoreInst(StoreInst *SI)
{
	std::string sID = getValueId(SI);
	Value *V = SI->getValueOperand();
	if (V->getType()->isIntegerTy() && sID != "") {
		CRange CR = getRange(SI->getParent(), V);
		unionRange(SI->getParent(), SI->getPointerOperand(), CR);
		// TODO Store can possibly modify multiple memory locations
		// due to aliases
		return unionRange(MemoryLocation::get(SI), CR, SI);
	}
	return false;
}

bool KINTRangePass::visitReturnInst(ReturnInst *RI)
{
	Value *V = RI->getReturnValue();
	if (!V || !V->getType()->isIntegerTy())
		return false;
	
	//std::string sID = getRetId(RI->getParent()->getParent());
	return unionRange(getLocation(RI->getFunction()), getRange(RI->getParent(), V), RI);
}

bool KINTRangePass::updateRangeFor(Instruction *I)
{
	bool changed = false;
	
	// store, return and call might update global range
	if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
		changed |= visitStoreInst(SI);
	} else if (ReturnInst *RI = dyn_cast<ReturnInst>(I)) {
		changed |= visitReturnInst(RI);
	} else if (CallInst *CI = dyn_cast<CallInst>(I)) {
		changed |= visitCallInst(CI);
	}
	
	IntegerType *Ty = dyn_cast<IntegerType>(I->getType());
	if (!Ty)
		return changed;
	
	CRange CR(Ty->getBitWidth(), true);
	if (BinaryOperator *BO = dyn_cast<BinaryOperator>(I)) {
		CR = visitBinaryOp(BO);
	} else if (CastInst *CI = dyn_cast<CastInst>(I)) {
		CR = visitCastInst(CI);
	} else if (SelectInst *SI = dyn_cast<SelectInst>(I)) {
		CR = visitSelectInst(SI);
	} else if (PHINode *PHI = dyn_cast<PHINode>(I)) {
		CR = visitPHINode(PHI);
	} else if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
		CR = visitLoadInst(LI);
	} else if (CallInst *CI = dyn_cast<CallInst>(I)) {
		CR = getRange(CI->getParent(), CI);
	}
	unionRange(I->getParent(), I, CR);
	
	return changed;
}

bool KINTRangePass::isBackEdge(const Edge &E)
{
	return std::find(BackEdges.begin(), BackEdges.end(), E)	!= BackEdges.end();
}

void KINTRangePass::visitBranchInst(BranchInst *BI, BasicBlock *BB, 
								ValueRangeMap &VRM)
{
	if (!BI->isConditional())
		return;
	
	ICmpInst *ICI = dyn_cast<ICmpInst>(BI->getCondition());
	if (ICI == NULL)
		return;
	
	Value *LHS = ICI->getOperand(0);
	Value *RHS = ICI->getOperand(1);
	
	if (!LHS->getType()->isIntegerTy() || !RHS->getType()->isIntegerTy())
		return;
	
	CRange LCR = getRange(ICI->getParent(), LHS);
	CRange RCR = getRange(ICI->getParent(), RHS);
	RCR.match(LCR);

	if (BI->getSuccessor(0) == BB) {
		// true target
		CRange PLCR = CRange::makeAllowedICmpRegion(
									ICI->getSwappedPredicate(), LCR);
		CRange PRCR = CRange::makeAllowedICmpRegion(
									ICI->getPredicate(), RCR);
		VRM.insert(std::make_pair(LHS, LCR.intersectWith(PRCR)));
		VRM.insert(std::make_pair(RHS, LCR.intersectWith(PLCR)));
	} else {
		// false target, use inverse predicate
		// N.B. why there's no getSwappedInversePredicate()...
		ICI->swapOperands();
		CRange PLCR = CRange::makeAllowedICmpRegion(
									ICI->getInversePredicate(), RCR);
		ICI->swapOperands();
		CRange PRCR = CRange::makeAllowedICmpRegion(
									ICI->getInversePredicate(), RCR);
		VRM.insert(std::make_pair(LHS, LCR.intersectWith(PRCR)));
		VRM.insert(std::make_pair(RHS, LCR.intersectWith(PLCR)));
	}
}

void KINTRangePass::visitSwitchInst(SwitchInst *SI, BasicBlock *BB, 
								ValueRangeMap &VRM)
{
	Value *V = SI->getCondition();
	IntegerType *Ty = dyn_cast<IntegerType>(V->getType());
	if (!Ty)
		return;
	
	CRange VCR = getRange(SI->getParent(), V);
	CRange CR(Ty->getBitWidth(), false);

	if (SI->getDefaultDest() != BB) {
		// union all values that goes to BB
		for (SwitchInst::CaseIt i = SI->case_begin(), e = SI->case_end();
			 i != e; ++i) {
			if (i.getCaseSuccessor() == BB)
				CR.safeUnion(i.getCaseValue()->getValue());
		}
	} else {
		// default case
		for (SwitchInst::CaseIt i = SI->case_begin(), e = SI->case_end();
			 i != e; ++i)
			CR.safeUnion(i.getCaseValue()->getValue());
		CR = CR.inverse();
	}
	VRM.insert(std::make_pair(V, VCR.intersectWith(CR)));
}

void KINTRangePass::visitTerminator(TerminatorInst *I, BasicBlock *BB,
								 ValueRangeMap &VRM) {
	if (BranchInst *BI = dyn_cast<BranchInst>(I))
		visitBranchInst(BI, BB, VRM);
	else if (SwitchInst *SI = dyn_cast<SwitchInst>(I))
		visitSwitchInst(SI, BB, VRM);
	else if (isa<ResumeInst>(I))
		llvm_unreachable("'resume' shouldn't have any successor.");
	else if (isa<UnreachableInst>(I))
		return; // Ignore unreachable instruction
	else {
		// XXX What about other kinds of terminator instructions? (LLVM 3.9)
		//   indirectbr (IndirectBrInst)
		//   ret (ReturnInst)
		//   invoke (InvokeInst)
		//   catchswitch (CatchSwitchInst)
		//   catchret (CatchReturnInst)
		//   cleanupret (CleanupReturnInst)
		llvm::errs() << "Ignore:" << I->getOpcodeName() << '\n';
		//ignore: I->dump(); llvm_unreachable("Unknown terminator!");
	}
}

bool KINTRangePass::updateRangeFor(BasicBlock *BB) {
	bool changed = false;

	// propagate value ranges from pred BBs, ranges in BB are union of ranges
	// in pred BBs, constrained by each terminator.
	for (pred_iterator i = pred_begin(BB), e = pred_end(BB);
			i != e; ++i) {
		BasicBlock *Pred = *i;
		if (isBackEdge(Edge(Pred, BB)))
			continue;

		ValueRangeMap &PredVRM = FuncVRMs[Pred];
		ValueRangeMap &BBVRM = FuncVRMs[BB];

		// Copy from its predecessor
		ValueRangeMap VRM(PredVRM.begin(), PredVRM.end());
		// Refine according to the terminator
		visitTerminator(Pred->getTerminator(), BB, VRM);

		// union with other predecessors
		for (ValueRangeMap::iterator j = VRM.begin(), je = VRM.end();
			 j != je; ++j) {
			ValueRangeMap::iterator it = BBVRM.find(j->first);
			if (it != BBVRM.end())
				it->second.safeUnion(j->second);
			else
				BBVRM.insert(*j);
		}
	}

	// Now run through instructions
	for (BasicBlock::iterator i = BB->begin(), e = BB->end();
		 i != e; ++i) {
		changed |= updateRangeFor(&*i);
	}

	return changed;
}

bool KINTRangePass::updateRangeFor(Function &F) {
	// Get Alias Analysis result for current function
	// since AAResultsWrapperPass is a FunctionPass
	this->AARPtr = &getAnalysis<AAResultsWrapperPass>(F).getAAResults();

	bool changed = false;

	FuncVRMs.clear();
	BackEdges.clear();
	FindFunctionBackedges(F, BackEdges);

	for (Function::iterator b = F.begin(), be = F.end(); b != be; ++b)
		changed |= updateRangeFor(&*b);

	this->AARPtr = nullptr; //Reset Alias Analysis result
	return changed;
}

bool KINTRangePass::runOnModule(Module& M) {
	this->CalleesPtr = &getAnalysis<KINTCallGraphPass>().getCalleeMap();
	this->TMPtr = &getAnalysis<KINTTaintPass>().getTaintMap();
	this->TLIPtr = &getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();

	unsigned itr = 0;
	bool changed = true, ret = false;

	while (changed) {
		// if some values converge too slowly, expand them to full-set
		if (++itr > MaxIterations) {
			for (ChangeSet::iterator it = Changes.begin(), ie = Changes.end();
				 it != ie; ++it) {
				LocRangeMap::iterator i = this->IntRanges.find(*it);
				i->second = CRange(i->second.getBitWidth(), true);
			}
		}
		changed = false;
		Changes.clear();
		for (Module::iterator i = M.begin(), e = M.end(); i != e; ++i)
			if (!i->empty())
				changed |= updateRangeFor(*i);
		ret |= changed;
	}

	this->CalleesPtr = nullptr; this->TMPtr = nullptr;
	return ret;
}

bool KINTRangePass::doFinalization(Module& M) {
	LLVMContext &VMCtx = M.getContext();
	for (Module::iterator f = M.begin(), fe = M.end(); f != fe; ++f) {
		Function& F = *f;
		for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
			Instruction& I = *i;
			if (!isa<LoadInst>(I) && !isa<CallInst>(I))
				continue;
			I.setMetadata("intrange", NULL);
			std::string id = getValueId(&I);
			if (id == "")
				continue;
			LocRangeMap &IRM = this->IntRanges;
			LocRangeMap::iterator it = IRM.find(getLocation(&I));
			if (it == IRM.end())
				continue;
			CRange &R = it->second;
			if (R.isEmptySet() || R.isFullSet())
				continue;

			ConstantInt *Lo = ConstantInt::get(VMCtx, R.getLower());
			ConstantInt *Hi = ConstantInt::get(VMCtx, R.getUpper());
			ArrayRef<Metadata*> RL = { ValueAsMetadata::get(Lo), ValueAsMetadata::get(Hi) };
			MDNode *MD = MDNode::get(VMCtx, RL);
			I.setMetadata("intrange", MD);
		}
	}
	return true;
}

char KINTRangePass::ID;

static RegisterPass<KINTRangePass>
X("kint-range", "Compute Value Range", false, true);
