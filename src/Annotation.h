#pragma once

#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Metadata.h>
#include <llvm/Support/Path.h>
#include <string>
#include <llvm/Support/Debug.h>

#define MD_TaintSrc   "taint_src"
#define MD_Taint      "taint"
#define MD_Sink       "sink"
#define MD_ID         "id"

class AnnotationPass : public llvm::FunctionPass {
public:
	static char ID;
	AnnotationPass() : FunctionPass(ID) { }

	virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const {
		AU.setPreservesCFG();
	}
	virtual bool runOnFunction(llvm::Function &);
	virtual bool doInitialization(llvm::Module &);
};


static inline bool isFunctionPointer(llvm::Type *Ty) {
	llvm::PointerType *PTy = llvm::dyn_cast<llvm::PointerType>(Ty);
	return PTy && PTy->getElementType()->isFunctionTy();
}

static inline std::string getScopeName(const llvm::GlobalValue *GV) {
	if (llvm::GlobalValue::isExternalLinkage(GV->getLinkage()))
		return GV->getName();
	else {
		llvm::StringRef moduleName = llvm::sys::path::stem(
			GV->getParent()->getModuleIdentifier());
		return "_" + moduleName.str() + "." + GV->getName().str();
	}
}

// prefix anonymous struct name with module name
static inline std::string getScopeName(llvm::StructType *Ty, llvm::Module *M) {
	if (Ty->getStructName().startswith("struct.anon")) {
		llvm::StringRef rest = Ty->getStructName().substr(6);
		llvm::StringRef moduleName = llvm::sys::path::stem(
			M->getModuleIdentifier());
		return "struct._" + moduleName.str() + rest.str();
	}
	return Ty->getStructName().str();
}

static inline llvm::StringRef getLoadStoreId(const llvm::Instruction *I) {
	if (llvm::MDNode *MD = I->getMetadata(MD_ID))
		return llvm::dyn_cast<llvm::MDString>(MD->getOperand(0))->getString();
	return llvm::StringRef();
}

static inline std::string
getStructId(llvm::Type *Ty, llvm::Module *M, unsigned offset) {
	llvm::StructType *STy = llvm::dyn_cast<llvm::StructType>(Ty);
	if (!STy || STy->isLiteral())
		return "";
	return getScopeName(STy, M) + "." + llvm::Twine(offset).str();
}

static inline std::string getVarId(const llvm::GlobalValue *GV) {
	return "var." + getScopeName(GV);
}

static inline std::string getArgId(const llvm::Argument *A) {
	return "arg." + getScopeName(A->getParent()) + "."
			+ llvm::Twine(A->getArgNo()).str();
}

static inline std::string getArgId(llvm::Function *F, unsigned no) {
	return "arg." + getScopeName(F) + "." + llvm::Twine(no).str();
}

static inline std::string getRetId(llvm::Function *F) {
	return "ret." + getScopeName(F);
}

static inline std::string getValueId(const llvm::Value *V);
static inline std::string getRetId(const llvm::CallInst *CI) {
	if (llvm::Function *CF = CI->getCalledFunction())
		return getRetId(CF);
	else {
		std::string sID = getValueId(CI->getCalledValue());
		if (sID != "")
			return "ret." + sID;
	}
	return "";
}

static inline std::string getValueId(const llvm::Value *V) {
	if(const llvm::GlobalValue *GV = llvm::dyn_cast<const llvm::GlobalValue>(V))
		return getVarId(GV);
	else if (const llvm::Argument *A = llvm::dyn_cast<const llvm::Argument>(V))
		return getArgId(A);
	else if (const llvm::CallInst *CI = llvm::dyn_cast<const llvm::CallInst>(V)) {
		if (llvm::Function *F = CI->getCalledFunction())
			if (F->getName().startswith("kint_arg.i"))
				return getLoadStoreId(CI);
		return getRetId(CI);
	} else if (llvm::isa<llvm::LoadInst>(V) || llvm::isa<llvm::StoreInst>(V))
		return getLoadStoreId(llvm::dyn_cast<const llvm::Instruction>(V));
	return "";
}

