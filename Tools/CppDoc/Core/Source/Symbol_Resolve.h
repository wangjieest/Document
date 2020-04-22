#ifndef VCZH_DOCUMENT_CPPDOC_SYMBOL_RESOLVE
#define VCZH_DOCUMENT_CPPDOC_SYMBOL_RESOLVE

#include "Ast.h"

/***********************************************************************
Resolving Functions
***********************************************************************/

struct ResolveSymbolResult
{
	Ptr<Resolving>									values;
	Ptr<Resolving>									types;

	void											Merge(Ptr<Resolving>& to, Ptr<Resolving> from);
	void											Merge(const ResolveSymbolResult& rar);
};

extern ResolveSymbolResult							ResolveSymbolInContext(const ParsingArguments& pa, CppName& name, bool cStyleTypeReference);
extern ResolveSymbolResult							ResolveChildSymbol(const ParsingArguments& pa, ITsys* tsysDecl, CppName& name);
extern ResolveSymbolResult							ResolveChildSymbol(const ParsingArguments& pa, Ptr<Type> classType, CppName& name);
extern ResolveSymbolResult							ResolveDirectChildSymbol(const ParsingArguments& pa, CppName& name);

#endif