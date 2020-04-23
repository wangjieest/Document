#include "Symbol_Resolve.h"
#include "EvaluateSymbol.h"
#include "Ast_Type.h"

/***********************************************************************
SearchPolicy
***********************************************************************/

enum class SearchPolicy
{
	_NoFeature = 0,
	_ClassNameAsType = 1,
	_AllowTemplateArgument = 2,
	_AccessParentScope = 4,
	_AccessClassBaseType = 8,

	// when in a scope S, search for <NAME> that is accessible in S
	InContext
		= _ClassNameAsType
		| _AllowTemplateArgument
		| _AccessParentScope
		| _AccessClassBaseType
		,

	// when in any scope, search for Something::<NAME>, if scope is a class, never search in any base class
	ScopedChild_NoBaseType
		= _NoFeature
		,

	// when in any scope, search for Something::<NAME>
	ScopedChild
		= _AccessClassBaseType
		,

	// when in class C, search for <NAME>, which is a member of C
	ClassMember_FromInside
		= _ClassNameAsType
		| _AllowTemplateArgument
		| _AccessClassBaseType
		,

	// when not in class C, search for <NAME>, which is a member of C
	ClassMember_FromOutside
		= _ClassNameAsType
		| _AccessClassBaseType
		,
};

constexpr bool operator& (SearchPolicy a, SearchPolicy b)
{
	return ((vint)a & (vint)b) > 0;
}

/***********************************************************************
IsPotentialTypeDeclVisitor
***********************************************************************/

void ResolveSymbolResult::Merge(Ptr<Resolving>& to, Ptr<Resolving> from)
{
	if (!from) return;

	if (!to)
	{
		to = MakePtr<Resolving>();
	}

	for (vint i = 0; i < from->resolvedSymbols.Count(); i++)
	{
		auto symbol = from->resolvedSymbols[i];
		if (!to->resolvedSymbols.Contains(symbol))
		{
			to->resolvedSymbols.Add(symbol);
		}
	}
}

void ResolveSymbolResult::Merge(const ResolveSymbolResult& rar)
{
	Merge(types, rar.types);
	Merge(values, rar.values);
}

/***********************************************************************
ResolveSymbolArguments
***********************************************************************/

struct ResolveSymbolArguments
{
	ResolveSymbolResult			result;
	bool						cStyleTypeReference = false;
	bool						found = false;
	CppName&					name;
	SortedList<Symbol*>			searchedScopes;
	SortedList<ITsys*>			searchedTypes;

	ResolveSymbolArguments(CppName& _name)
		:name(_name)
	{
	}
};

void ResolveChildSymbolInternal(const ParsingArguments& pa, Ptr<Type> classType, SearchPolicy policy, ResolveSymbolArguments& rsa);

/***********************************************************************
AddSymbolToResolve
***********************************************************************/

void AddSymbolToResolve(Ptr<Resolving>& resolving, Symbol* symbol)
{
	if (!resolving)
	{
		resolving = new Resolving;
	}

	if (!resolving->resolvedSymbols.Contains(symbol))
	{
		resolving->resolvedSymbols.Add(symbol);
	}
}

/***********************************************************************
PickResolvedSymbols
***********************************************************************/

void PickResolvedSymbols(const List<Ptr<Symbol>>* pSymbols, bool allowTemplateArgument, ResolveSymbolArguments& rsa)
{
	bool hasCStyleType = false;
	bool hasOthers = false;
	for (vint i = 0; i < pSymbols->Count(); i++)
	{
		auto symbol = pSymbols->Get(i).Obj();
		switch (symbol->kind)
		{
		case symbol_component::SymbolKind::Enum:
		case symbol_component::SymbolKind::Class:
		case symbol_component::SymbolKind::Struct:
		case symbol_component::SymbolKind::Union:
			hasCStyleType = true;
			break;
		default:
			hasOthers = true;
		}
	}

	bool acceptCStyleType = false;
	bool acceptOthers = false;

	if (hasCStyleType && hasOthers)
	{
		acceptCStyleType = rsa.cStyleTypeReference;
		acceptOthers = !rsa.cStyleTypeReference;
	}
	else if (hasCStyleType)
	{
		acceptCStyleType = true;
	}
	else if (hasOthers)
	{
		acceptOthers = !rsa.cStyleTypeReference;
	}
	else
	{
		throw L"This could not happen, because pSymbols should be nullptr in this case.";
	}

	if (acceptCStyleType)
	{
		for (vint i = 0; i < pSymbols->Count(); i++)
		{
			auto symbol = pSymbols->Get(i).Obj();
			switch (symbol->kind)
			{
				// type symbols
			case CSTYLE_TYPE_SYMBOL_KIND:
				rsa.found = true;
				AddSymbolToResolve(rsa.result.types, symbol);
				break;
			}
		}
	}

	if (acceptOthers)
	{
		for (vint i = 0; i < pSymbols->Count(); i++)
		{
			auto symbol = pSymbols->Get(i).Obj();
			switch (symbol->kind)
			{
				// type symbols
			case symbol_component::SymbolKind::TypeAlias:
			case symbol_component::SymbolKind::Namespace:
				rsa.found = true;
				AddSymbolToResolve(rsa.result.types, symbol);
				break;

				// value symbols
			case symbol_component::SymbolKind::EnumItem:
			case symbol_component::SymbolKind::FunctionSymbol:
			case symbol_component::SymbolKind::Variable:
			case symbol_component::SymbolKind::ValueAlias:
				rsa.found = true;
				AddSymbolToResolve(rsa.result.values, symbol);
				break;

				// template type argument
			case symbol_component::SymbolKind::GenericTypeArgument:
				if (allowTemplateArgument)
				{
					rsa.found = true;
					AddSymbolToResolve(rsa.result.types, symbol);
				}
				break;

				// template value argument
			case symbol_component::SymbolKind::GenericValueArgument:
				if (allowTemplateArgument)
				{
					rsa.found = true;
					AddSymbolToResolve(rsa.result.values, symbol);
				}
				break;
			}
		}
	}
}

/***********************************************************************
ResolveSymbolInTypeInternal
***********************************************************************/

void ResolveSymbolInTypeInternal(const ParsingArguments& pa, ITsys* tsys, SearchPolicy policy, ResolveSymbolArguments& rsa)
{
	TsysCV cv;
	TsysRefType ref;
	auto entity = tsys->GetEntity(cv, ref);

	if (rsa.searchedTypes.Contains(entity))
	{
		return;
	}
	else
	{
		rsa.searchedTypes.Add(entity);
	}

	if (entity->GetType() == TsysType::Decl || entity->GetType() == TsysType::DeclInstant)
	{
		auto scope = entity->GetDecl();
		if (auto classDecl = scope->GetImplDecl_NFb<ClassDeclaration>())
		{
			if (classDecl->name.name == rsa.name.name)
			{
				if (policy & SearchPolicy::_ClassNameAsType)
				{
					// A::A could never be the type A
					// But searching A inside A will get the type A
					rsa.found = true;
					AddSymbolToResolve(rsa.result.types, classDecl->symbol);
				}
			}

			if (auto pSymbols = scope->TryGetChildren_NFb(rsa.name.name))
			{
				PickResolvedSymbols(pSymbols, (policy & SearchPolicy::_AllowTemplateArgument), rsa);
			}

			if (rsa.found) return;

			if (policy & SearchPolicy::_AccessClassBaseType)
			{
				auto& ev = symbol_type_resolving::EvaluateClassType(pa, tsys);
				for (vint i = 0; i < ev.ExtraCount(); i++)
				{
					auto& extra = ev.GetExtra(i);
					for (vint j = 0; j < extra.Count(); j++)
					{
						auto basePolicy
							= policy == SearchPolicy::ScopedChild || policy == SearchPolicy::InContext
							? SearchPolicy::ScopedChild
							: SearchPolicy::ClassMember_FromOutside
							;
						ResolveSymbolInTypeInternal(pa, extra[j], basePolicy, rsa);
					}
				}
			}
		}
		else if (auto enumDecl = scope->GetImplDecl_NFb<EnumDeclaration>())
		{
			if (auto pSymbols = scope->TryGetChildren_NFb(rsa.name.name))
			{
				PickResolvedSymbols(pSymbols, (policy & SearchPolicy::_AllowTemplateArgument), rsa);
			}
		}
	}
}

/***********************************************************************
ResolveSymbolInternal
***********************************************************************/

void ResolveSymbolInternal(const ParsingArguments& pa, SearchPolicy policy, ResolveSymbolArguments& rsa)
{
	auto scope = pa.scopeSymbol;
	if (rsa.searchedScopes.Contains(scope))
	{
		return;
	}
	else
	{
		rsa.searchedScopes.Add(scope);
	}

	while (scope)
	{
		auto currentClassDecl = scope->GetImplDecl_NFb<ClassDeclaration>();
		if (scope->GetAnyForwardDecl<ForwardClassDeclaration>() || scope->GetAnyForwardDecl<ForwardEnumDeclaration>())
		{
			throw L"This could not happen, because type scopes should be skipped by ClassMemberCache.";
		}
		else
		{
			if (auto pSymbols = scope->TryGetChildren_NFb(rsa.name.name))
			{
				PickResolvedSymbols(pSymbols, (policy & SearchPolicy::_AllowTemplateArgument), rsa);
			}

			if (scope->usingNss.Count() > 0)
			{
				for (vint i = 0; i < scope->usingNss.Count(); i++)
				{
					auto usingNs = scope->usingNss[i];
					auto newPa = pa.WithScope(usingNs);
					ResolveSymbolInternal(newPa, SearchPolicy::ScopedChild, rsa);
				}
			}

			if (rsa.found) break;

			if (auto cache = scope->GetClassMemberCache_NFb())
			{
				auto childPolicy
					= cache->symbolDefinedInsideClass
					? SearchPolicy::ClassMember_FromInside
					: SearchPolicy::ClassMember_FromOutside
					;
				for (vint i = 0; i < cache->containerClassTypes.Count(); i++)
				{
					auto classType = cache->containerClassTypes[i];
					ResolveSymbolInTypeInternal(pa, classType, childPolicy, rsa);
				}
			}

			if (rsa.found) break;
		}

		if (policy & SearchPolicy::_AccessParentScope)
		{
			// classMemberCache does not exist in class scope
			if (auto cache = scope->GetClassMemberCache_NFb())
			{
				scope = cache->parentScope;
			}
			else
			{
				scope = scope->GetParentScope();
			}
		}
		else
		{
			scope = nullptr;
		}
	}
}

/***********************************************************************
ResolveChildSymbolInternal
***********************************************************************/

class ResolveChildSymbolTypeVisitor : public Object, public virtual ITypeVisitor
{
public:
	const ParsingArguments&		pa;
	SearchPolicy				policy;
	ResolveSymbolArguments&		rsa;

	ResolveChildSymbolTypeVisitor(const ParsingArguments& _pa, SearchPolicy _policy, ResolveSymbolArguments& _rsa)
		:pa(_pa)
		, policy(_policy)
		, rsa(_rsa)
	{
	}

	void ResolveChildSymbolOfType(Type* type)
	{
		TypeTsysList tsyses;
		bool isVta = false;
		TypeToTsysInternal(pa, type, tsyses, isVta);
		for (vint i = 0; i < tsyses.Count(); i++)
		{
			auto tsys = tsyses[i];
			if (tsys->GetType() == TsysType::Init)
			{
				if (isVta)
				{
					for (vint j = 0; j < tsys->GetParamCount(); j++)
					{
						ResolveSymbolInTypeInternal(pa, tsys->GetParam(j), policy, rsa);
					}
				}
			}
			else
			{
				ResolveSymbolInTypeInternal(pa, tsys, policy, rsa);
			}
		}
	}

	void Visit(PrimitiveType* self)override
	{
	}

	void Visit(ReferenceType* self)override
	{
	}

	void Visit(ArrayType* self)override
	{
	}

	void Visit(CallingConventionType* self)override
	{
		self->type->Accept(this);
	}

	void Visit(FunctionType* self)override
	{
	}

	void Visit(MemberType* self)override
	{
	}

	void Visit(DeclType* self)override
	{
		if (self->expr)
		{
			ResolveChildSymbolOfType(self);
		}
	}

	void Visit(DecorateType* self)override
	{
		self->type->Accept(this);
	}

	void Visit(RootType* self)override
	{
		ResolveSymbolInternal(pa.WithScope(pa.root.Obj()), SearchPolicy::ScopedChild, rsa);
	}

	void Visit(IdType* self)override
	{
		ResolveChildSymbolOfType(self);
	}

	void Visit(ChildType* self)override
	{
		ResolveChildSymbolOfType(self);
	}

	void Visit(GenericType* self)override
	{
		self->type->Accept(this);
	}
};

void ResolveChildSymbolInternal(const ParsingArguments& pa, Ptr<Type> classType, SearchPolicy policy, ResolveSymbolArguments& rsa)
{
	ResolveChildSymbolTypeVisitor visitor(pa, policy, rsa);
	classType->Accept(&visitor);
}

/***********************************************************************
ResolveSymbolInContext
***********************************************************************/

ResolveSymbolResult ResolveSymbolInContext(const ParsingArguments& pa, CppName& name, bool cStyleTypeReference)
{
	ResolveSymbolArguments rsa(name);
	rsa.cStyleTypeReference = cStyleTypeReference;
	ResolveSymbolInternal(pa, SearchPolicy::InContext, rsa);
	return rsa.result;
}

/***********************************************************************
ResolveChildSymbol
***********************************************************************/

ResolveSymbolResult ResolveChildSymbol(const ParsingArguments& pa, ITsys* tsysDecl, CppName& name, bool searchInBaseTypes)
{
	ResolveSymbolArguments rsa(name);
	if (searchInBaseTypes)
	{
		ResolveSymbolInTypeInternal(pa, tsysDecl, SearchPolicy::ScopedChild, rsa);
	}
	else
	{
		ResolveSymbolInTypeInternal(pa, tsysDecl, SearchPolicy::ScopedChild_NoBaseType, rsa);
	}
	return rsa.result;
}

ResolveSymbolResult ResolveChildSymbol(const ParsingArguments& pa, Ptr<Type> classType, CppName& name)
{
	ResolveSymbolArguments rsa(name);
	ResolveChildSymbolInternal(pa, classType, SearchPolicy::ScopedChild, rsa);
	return rsa.result;
}