/*
 * KINTGlobal.h
 *
 *  Created on: Jun 21, 2017
 *      Author: chsieh16
 */

#ifndef SRC_KINTGLOBAL_H_
#define SRC_KINTGLOBAL_H_

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>

#include <map>
#include <set>
#include <vector>
#include "CRange.h"

typedef std::vector<std::pair<std::unique_ptr<llvm::Module>, llvm::StringRef> > ModuleList;
typedef llvm::SmallPtrSet<llvm::Function *, 8> FuncSet;
typedef std::map<llvm::StringRef, llvm::Function *> FuncMap;
typedef std::map<std::string, FuncSet> FuncPtrMap;
typedef llvm::DenseMap<llvm::CallInst *, FuncSet> CalleeMap;
typedef std::set<llvm::StringRef> DescSet;

class TaintMap {

public:
	typedef std::map<std::string, std::pair<DescSet, bool> > GlobalMap;
	typedef std::map<llvm::Value *, DescSet> ValueMap;

	GlobalMap GTS;
	ValueMap VTS;

	void add(llvm::Value *V, const DescSet &D) {
		VTS[V].insert(D.begin(), D.end());
	}
	void add(llvm::Value *V, llvm::StringRef D) {
		VTS[V].insert(D);
	}
	DescSet* get(llvm::Value *V) {
		ValueMap::iterator it = VTS.find(V);
		if (it != VTS.end())
			return &it->second;
		return NULL;
	}

	DescSet* get(const std::string &ID) {
		if (ID.empty())
			return NULL;
		GlobalMap::iterator it = GTS.find(ID);
		if (it != GTS.end())
			return &it->second.first;
		return NULL;
	}
	bool add(const std::string &ID, const DescSet &D, bool isSource = false) {
		if (ID.empty())
			return false;
		std::pair<DescSet, bool> &entry = GTS[ID];
		bool isNew = entry.first.empty();
		entry.first.insert(D.begin(), D.end());
		entry.second |= isSource;
		return isNew;
	}
	bool isSource(const std::string &ID) const {
		if (ID.empty())
			return false;
		GlobalMap::const_iterator it = GTS.find(ID);
		if (it == GTS.end())
			return false;
		return it->second.second;
	}
};

class KINTCallGraphPass : public llvm::ModulePass {
public:
	static char ID;

	KINTCallGraphPass(): llvm::ModulePass(ID){}

	virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const {
		AU.setPreservesAll();
	}

	virtual bool doInitialization(llvm::Module& );
	virtual bool doFinalization(llvm::Module& );
	virtual bool runOnModule(llvm::Module& );

	const CalleeMap& getCalleeMap() const { return this->Callees;}

private:
	// collect function pointer assignments in global initializers
	void processInitializers(llvm::Module &, llvm::Constant *, llvm::GlobalValue *);
	bool runOnFunction(llvm::Function &);
	bool mergeFuncSet(FuncSet &S, const std::string &Id);
	bool mergeFuncSet(FuncSet &Dst, const FuncSet &Src);
	bool findFunctions(llvm::Value *, FuncSet &);
	bool findFunctions(llvm::Value *, FuncSet &,
	                   llvm::SmallPtrSet<llvm::Value *, 4>);
private:
	// Map global function name to function definition
	FuncMap Funcs;

	// Map function pointers (IDs) to possible assignments
	FuncPtrMap FuncPtrs;

	// Map a callsite to all potential callees
	CalleeMap Callees;
};

class KINTTaintPass : public llvm::ModulePass {
public:
	static char ID;

	KINTTaintPass(): llvm::ModulePass(ID), CalleesPtr(nullptr) {}
	virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const {
		AU.setPreservesCFG();
		AU.addRequired<KINTCallGraphPass>();
	}

	virtual bool doFinalization(llvm::Module &);
	virtual bool runOnModule(llvm::Module &);

	const TaintMap& getTaintMap() const { return TM;}

private:
	DescSet* getTaint(llvm::Value *);
	bool runOnFunction(llvm::Function &);
	bool checkTaintSource(llvm::Instruction *I);

	const CalleeMap* CalleesPtr;
	// Taints
	TaintMap TM;
};


#endif /* SRC_KINTGLOBAL_H_ */
