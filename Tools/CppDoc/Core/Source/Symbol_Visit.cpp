#include "Symbol_Visit.h"
#include "EvaluateSymbol.h"
#include "Ast_Resolving.h"

namespace symbol_type_resolving
{
	/***********************************************************************
	AdjustThisItemForSymbol: Fix thisItem according to symbol
	***********************************************************************/

	Nullable<ExprTsysItem> AdjustThisItemForSymbol(const ParsingArguments& pa, ExprTsysItem thisItem, Symbol* symbol)
	{
		auto thisType = ApplyExprTsysType(thisItem.tsys, thisItem.type);
		TsysCV thisCv;
		TsysRefType thisRef;
		auto entity = thisType->GetEntity(thisCv, thisRef);

		TypeTsysList visiting;
		SortedList<ITsys*> visited;
		visiting.Add(entity);

		for (vint i = 0; i < visiting.Count(); i++)
		{
			auto currentThis = visiting[i];
			if (visited.Contains(currentThis))
			{
				continue;
			}
			visited.Add(currentThis);

			switch (currentThis->GetType())
			{
			case TsysType::Decl:
			case TsysType::DeclInstant:
				break;
			default:
				throw TypeCheckerException();
			}

			auto thisSymbol = currentThis->GetDecl();
			if (symbol->GetParentScope() == thisSymbol)
			{
				ExprTsysItem baseItem(thisSymbol, thisItem.type, CvRefOf(currentThis, thisCv, thisRef));
				return baseItem;
			}

			auto& ev = symbol_type_resolving::EvaluateClassType(pa, currentThis);
			for (vint j = 0; j < ev.ExtraCount(); j++)
			{
				CopyFrom(visiting, ev.GetExtra(j), true);
			}
		}

		return {};
	}

	/***********************************************************************
	VisitSymbol: Fill a symbol to ExprTsysList
		thisItem:
			it represents typeof(x) in x.name or typeof(*x) in x->name
			it could be null if it is initiated by IdExpr (visitMemberKind == InScope)
	***********************************************************************/

	enum class VisitMemberKind
	{
		MemberAfterType,
		MemberAfterValue,
		InScope,
	};

	void VisitSymbolInternalWithCorrectThisItem(const ParsingArguments& pa, const ExprTsysItem* thisItem, Symbol* symbol, VisitMemberKind visitMemberKind, ExprTsysList& result, bool allowVariadic, bool& hasVariadic, bool& hasNonVariadic)
	{
		Symbol* classScope = nullptr;
		if (auto parent = symbol->GetParentScope())
		{
			if (parent->GetImplDecl_NFb<ClassDeclaration>())
			{
				classScope = parent;
			}
		}

		ITsys* parentDeclType = nullptr;
		ITsys* thisEntity = nullptr;
		if (thisItem)
		{
			thisEntity = GetThisEntity(thisItem->tsys);
			if (thisEntity->GetType() == TsysType::DeclInstant)
			{
				parentDeclType = thisEntity;
			}
		}

		switch (symbol->kind)
		{
		case symbol_component::SymbolKind::Variable:
			{
				bool isVariadic = false;
				auto varDecl = symbol->GetAnyForwardDecl<ForwardVariableDeclaration>();
				auto& evTypes = EvaluateVarSymbol(pa, varDecl.Obj(), parentDeclType, isVariadic);
				bool isStaticSymbol = IsStaticSymbol<ForwardVariableDeclaration>(symbol);
				bool isMember = classScope && !isStaticSymbol;
				if (!thisItem && isMember)
				{
					return;
				}

				for (vint k = 0; k < evTypes.Count(); k++)
				{
					auto tsys = evTypes[k];

					if (isStaticSymbol)
					{
						AddInternal(result, { symbol,ExprTsysType::LValue,tsys });
					}
					else
					{
						switch (visitMemberKind)
						{
						case VisitMemberKind::MemberAfterType:
							{
								if (classScope)
								{
									AddInternal(result, { symbol,ExprTsysType::PRValue,tsys->MemberOf(thisEntity) });
								}
								else
								{
									AddInternal(result, { symbol,ExprTsysType::LValue,tsys });
								}
							}
							break;
						case VisitMemberKind::MemberAfterValue:
							{
								CalculateValueFieldType(thisItem, symbol, tsys, false, result);
							}
							break;
						case VisitMemberKind::InScope:
							{
								AddInternal(result, { symbol,ExprTsysType::LValue,tsys });
							}
							break;
						}
					}
				}
				if (!allowVariadic && isVariadic) throw TypeCheckerException();
				hasNonVariadic = !isVariadic;
				hasVariadic = isVariadic;
			}
			return;
		case symbol_component::SymbolKind::FunctionSymbol:
			{
				auto funcDecl = symbol->GetAnyForwardDecl<ForwardFunctionDeclaration>();
				auto& evTypes = EvaluateFuncSymbol(pa, funcDecl.Obj(), parentDeclType, nullptr);
				bool isStaticSymbol = IsStaticSymbol<ForwardFunctionDeclaration>(symbol);
				bool isMember = classScope && !isStaticSymbol;
				if (!thisItem && isMember)
				{
					return;
				}

				for (vint k = 0; k < evTypes.Count(); k++)
				{
					auto tsys = evTypes[k];

					if (tsys->GetType() == TsysType::GenericFunction)
					{
						auto elementTsys = tsys->GetElement();
						if (isMember && visitMemberKind == VisitMemberKind::MemberAfterType)
						{
							elementTsys = elementTsys->MemberOf(thisEntity)->PtrOf();
						}
						else
						{
							elementTsys = elementTsys->PtrOf();
						}

						const auto& gf = tsys->GetGenericFunction();
						TypeTsysList params;
						vint count = tsys->GetParamCount();
						for (vint l = 0; l < count; l++)
						{
							params.Add(tsys->GetParam(l));
						}
						tsys = elementTsys->GenericFunctionOf(params, gf);
					}
					else
					{
						if (isMember && visitMemberKind == VisitMemberKind::MemberAfterType)
						{
							tsys = tsys->MemberOf(thisEntity)->PtrOf();
						}
						else
						{
							tsys = tsys->PtrOf();
						}
					}

					AddInternal(result, { symbol,ExprTsysType::PRValue,tsys });
				}
				hasNonVariadic = true;
			}
			return;
		case symbol_component::SymbolKind::EnumItem:
			{
				auto tsys = pa.tsys->DeclOf(symbol->GetParentScope());
				AddInternal(result, { symbol,ExprTsysType::PRValue,tsys });
				hasNonVariadic = true;
			}
			return;
		case symbol_component::SymbolKind::ValueAlias:
			{
				auto usingDecl = symbol->GetImplDecl_NFb<ValueAliasDeclaration>();
				auto& evTypes = EvaluateValueAliasSymbol(pa, usingDecl.Obj(), parentDeclType, nullptr);
				AddTempValue(result, evTypes);
				hasNonVariadic = true;
			}
			return;
		case symbol_component::SymbolKind::GenericValueArgument:
			{
				if (symbol->ellipsis)
				{
					if (!allowVariadic)
					{
						throw TypeCheckerException();
					}
					hasVariadic = true;

					auto argumentKey = pa.tsys->DeclOf(symbol);
					ITsys* replacedType = nullptr;
					if (pa.TryGetReplacedGenericArg(argumentKey, replacedType))
					{
						if (!replacedType)
						{
							throw TypeCheckerException();
						}

						switch (replacedType->GetType())
						{
						case TsysType::Init:
							{
								TypeTsysList tsys;
								tsys.SetLessMemoryMode(false);

								Array<ExprTsysList> initArgs(replacedType->GetParamCount());
								for (vint j = 0; j < initArgs.Count(); j++)
								{
									AddTempValue(initArgs[j], EvaluateGenericArgumentSymbol(symbol)->ReplaceGenericArgs(pa));
								}
								CreateUniversalInitializerType(pa, initArgs, result);
							}
							break;
						case TsysType::Any:
							AddType(result, pa.tsys->Any());
							break;
						default:
							throw TypeCheckerException();
						}
					}
					else
					{
						AddType(result, pa.tsys->Any());
					}
				}
				else
				{
					AddTempValue(result, EvaluateGenericArgumentSymbol(symbol)->ReplaceGenericArgs(pa));
					hasNonVariadic = true;
				}
			}
			return;
		}
		throw IllegalExprException();
	}

	void VisitSymbolInternal(const ParsingArguments& pa, const ExprTsysItem* thisItem, Symbol* symbol, VisitMemberKind visitMemberKind, ExprTsysList& result, bool allowVariadic, bool& hasVariadic, bool& hasNonVariadic)
	{
		switch (symbol->GetParentScope()->kind)
		{
		case CLASS_SYMBOL_KIND:
			break;
		default:
			thisItem = nullptr;
		}

		if (thisItem)
		{
			auto adjustedThisItem = AdjustThisItemForSymbol(pa, *thisItem, symbol);
			if (!adjustedThisItem)
			{
				throw TypeCheckerException();
			}

			auto baseItem = adjustedThisItem.Value();
			VisitSymbolInternalWithCorrectThisItem(pa, &baseItem, symbol, visitMemberKind, result, allowVariadic, hasVariadic, hasNonVariadic);
		}
		else
		{
			VisitSymbolInternalWithCorrectThisItem(pa, nullptr, symbol, visitMemberKind, result, allowVariadic, hasVariadic, hasNonVariadic);
		}
	}

	void VisitSymbol(const ParsingArguments& pa, Symbol* symbol, ExprTsysList& result)
	{
		bool hasVariadic = false;
		bool hasNonVariadic = false;
		VisitSymbolInternal(pa, nullptr, symbol, VisitMemberKind::InScope, result, false, hasVariadic, hasNonVariadic);
	}

	void VisitSymbolForScope(const ParsingArguments& pa, const ExprTsysItem* thisItem, Symbol* symbol, ExprTsysList& result)
	{
		bool hasVariadic = false;
		bool hasNonVariadic = false;
		VisitSymbolInternal(pa, thisItem, symbol, VisitMemberKind::MemberAfterType, result, false, hasVariadic, hasNonVariadic);
	}

	void VisitSymbolForField(const ParsingArguments& pa, const ExprTsysItem* thisItem, Symbol* symbol, ExprTsysList& result)
	{
		bool hasVariadic = false;
		bool hasNonVariadic = false;
		VisitSymbolInternal(pa, thisItem, symbol, VisitMemberKind::MemberAfterValue, result, false, hasVariadic, hasNonVariadic);
	}

	/***********************************************************************
	FindMembersByName: Fill all members of a name to ExprTsysList
	***********************************************************************/

	Ptr<Resolving> FindMembersByName(const ParsingArguments& pa, CppName& name, ResolveSymbolResult* totalRar, const ExprTsysItem& parentItem)
	{
		TsysCV cv;
		TsysRefType refType;
		auto entity = parentItem.tsys->GetEntity(cv, refType);
		auto rar = ResolveChildSymbol(pa, entity, name);
		if (totalRar) totalRar->Merge(rar);
		return rar.values;
	}

	/***********************************************************************
	VisitResolvedMember: Fill all resolved member symbol to ExprTsysList
	***********************************************************************/

	void VisitResolvedMemberInternal(const ParsingArguments& pa, const ExprTsysItem* thisItem, Ptr<Resolving> resolving, ExprTsysList& result, bool allowVariadic, bool& hasVariadic, bool& hasNonVariadic)
	{
		ExprTsysList varTypes, funcTypes;
		for (vint i = 0; i < resolving->resolvedSymbols.Count(); i++)
		{
			auto targetTypeList = &result;
			auto symbol = resolving->resolvedSymbols[i];
			
			bool parentScopeIsClass = false;
			if (auto parent = symbol->GetParentScope())
			{
				switch (parent->kind)
				{
				case CLASS_SYMBOL_KIND:
					parentScopeIsClass = true;
					switch (symbol->kind)
					{
					case symbol_component::SymbolKind::Variable:
						if (!IsStaticSymbol<ForwardVariableDeclaration>(symbol))
						{
							targetTypeList = &varTypes;
						}
						break;
					case symbol_component::SymbolKind::FunctionSymbol:
						if (!IsStaticSymbol<ForwardFunctionDeclaration>(symbol))
						{
							targetTypeList = &funcTypes;
						}
						break;
					}
					break;
				}
			}

			auto thisItemForSymbol = parentScopeIsClass ? thisItem : nullptr;
			VisitSymbolInternal(pa, thisItemForSymbol, resolving->resolvedSymbols[i], (thisItemForSymbol ? VisitMemberKind::MemberAfterValue : VisitMemberKind::InScope), *targetTypeList, allowVariadic, hasVariadic, hasNonVariadic);
		}

		if (varTypes.Count() > 0)
		{
			// varTypes has non-static member fields
			AddInternal(result, varTypes);
		}

		if (funcTypes.Count() > 0)
		{
			if (thisItem)
			{
				// funcTypes has non-static member functions
				TsysCV thisCv;
				TsysRefType thisRef;
				thisItem->tsys->GetEntity(thisCv, thisRef);
				FilterFieldsAndBestQualifiedFunctions(thisCv, thisRef, funcTypes);
				AddInternal(result, funcTypes);
			}
			else
			{
				AddInternal(result, funcTypes);
			}
		}
	}

	void VisitResolvedMember(const ParsingArguments& pa, const ExprTsysItem* thisItem, Ptr<Resolving> resolving, ExprTsysList& result, bool& hasVariadic, bool& hasNonVariadic)
	{
		VisitResolvedMemberInternal(pa, thisItem, resolving, result, true, hasVariadic, hasNonVariadic);
	}

	void VisitResolvedMember(const ParsingArguments& pa, const ExprTsysItem* thisItem, Ptr<Resolving> resolving, ExprTsysList& result)
	{
		bool hasVariadic = false;
		bool hasNonVariadic = false;
		VisitResolvedMemberInternal(pa, thisItem, resolving, result, false, hasVariadic, hasNonVariadic);
	}

	/***********************************************************************
	VisitFunctors: Find qualified functors (including functions and operator())
	***********************************************************************/

	void VisitFunctors(const ParsingArguments& pa, const ExprTsysItem& parentItem, const WString& name, ExprTsysList& result)
	{
		TsysCV cv;
		TsysRefType refType;
		parentItem.tsys->GetEntity(cv, refType);

		CppName opName;
		opName.name = name;
		if (auto resolving = FindMembersByName(pa, opName, nullptr, parentItem))
		{
			for (vint j = 0; j < resolving->resolvedSymbols.Count(); j++)
			{
				auto symbol = resolving->resolvedSymbols[j];
				VisitSymbolForField(pa, &parentItem, symbol, result);
			}
			FindQualifiedFunctors(pa, cv, refType, result, false);
		}
	}
}