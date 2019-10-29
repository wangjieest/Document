#include "Ast_Evaluate_ExpandPotentialVta.h"

using namespace symbol_type_resolving;

namespace symbol_totsys_impl
{
	//////////////////////////////////////////////////////////////////////////////////////
	// ProcessGenericNode
	//////////////////////////////////////////////////////////////////////////////////////
	
	template<typename TProcess>
	void ProcessGenericType(const ParsingArguments& pa, ExprTsysList& result, Array<bool>& isTypes, Array<ExprTsysItem>& args, Array<vint>& argSource, SortedList<vint>& boundedAnys, TProcess&& process)
	{
		auto genericFunction = args[0].tsys;
		if (
			genericFunction->GetType() == TsysType::Ptr &&
			genericFunction->GetElement()->GetType() == TsysType::GenericFunction &&
			genericFunction->GetElement()->GetElement()->GetType() == TsysType::Function
			)
		{
			genericFunction = genericFunction->GetElement();
		}

		if (genericFunction->GetType() == TsysType::GenericFunction)
		{
			auto declSymbol = genericFunction->GetGenericFunction().declSymbol;
			if (!declSymbol)
			{
				throw NotConvertableException();
			}

			TemplateArgumentContext taContext;
			taContext.symbolToApply = genericFunction->GetGenericFunction().declSymbol;
			ResolveGenericParameters(pa, taContext, genericFunction, args, isTypes, argSource, boundedAnys, 1);
			process(genericFunction, declSymbol, &taContext);
		}
		else if (genericFunction->GetType() == TsysType::Any)
		{
			AddExprTsysItemToResult(result, GetExprTsysItem(pa.tsys->Any()));
		}
		else
		{
			throw NotConvertableException();
		}
	}

	void UseTsys(ExprTsysList& result, ITsys* tsys)
	{
		AddExprTsysItemToResult(result, GetExprTsysItem(tsys));
	}

	void UseTypeTsysList(ExprTsysList& result, TypeTsysList& tsys)
	{
		for (vint j = 0; j < tsys.Count(); j++)
		{
			UseTsys(result, tsys[j]);
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////
	// ProcessGenericType
	//////////////////////////////////////////////////////////////////////////////////////
	
	void ProcessGenericType(const ParsingArguments& pa, ExprTsysList& result, GenericType* self, Array<bool>& isTypes, Array<ExprTsysItem>& args, Array<vint>& argSource, SortedList<vint>& boundedAnys)
	{
		ProcessGenericType(pa, result, isTypes, args, argSource, boundedAnys, [&pa, &result](ITsys* genericFunction, Symbol* declSymbol, TemplateArgumentContext* argumentsToApply)
		{
			auto parentDeclType = genericFunction->GetGenericFunction().parentDeclType;
			switch (declSymbol->kind)
			{
			case CLASS_SYMBOL_KIND:
				{
					auto decl = declSymbol->GetAnyForwardDecl<ForwardClassDeclaration>();
					if (!decl->templateSpec) throw NotConvertableException();
					auto& tsys = EvaluateForwardClassSymbol(pa, decl.Obj(), parentDeclType, argumentsToApply);
					UseTypeTsysList(result, tsys);
				}
				break;
			case symbol_component::SymbolKind::TypeAlias:
				{
					auto decl = declSymbol->GetImplDecl_NFb<TypeAliasDeclaration>();
					if (!decl->templateSpec) throw NotConvertableException();
					auto& tsys = EvaluateTypeAliasSymbol(pa, decl.Obj(), parentDeclType, argumentsToApply);
					UseTypeTsysList(result, tsys);
				}
				break;
			case symbol_component::SymbolKind::GenericTypeArgument:
				{
					auto tsys = genericFunction->GetElement()->ReplaceGenericArgs(pa.AppendSingleLevelArgs(*argumentsToApply));
					UseTsys(result, tsys);
				}
				break;
			default:
				throw NotConvertableException();
			}
		});
	}

	//////////////////////////////////////////////////////////////////////////////////////
	// ProcessGenericExpr
	//////////////////////////////////////////////////////////////////////////////////////

	void ProcessGenericExpr(const ParsingArguments& pa, ExprTsysList& result, GenericExpr* self, Array<bool>& isTypes, Array<ExprTsysItem>& args, Array<vint>& argSource, SortedList<vint>& boundedAnys)
	{
		ProcessGenericType(pa, result, isTypes, args, argSource, boundedAnys, [&pa, &result](ITsys* genericFunction, Symbol* declSymbol, TemplateArgumentContext* argumentsToApply)
		{
			auto parentDeclType = genericFunction->GetGenericFunction().parentDeclType;
			switch (declSymbol->kind)
			{
			case symbol_component::SymbolKind::FunctionBodySymbol:
				{
					auto decl = declSymbol->GetAnyForwardDecl<ForwardFunctionDeclaration>();
					if (!decl->templateSpec) throw NotConvertableException();
					auto& tsys = EvaluateFuncSymbol(pa, decl.Obj(), parentDeclType, argumentsToApply);

					for (vint i = 0; i < tsys.Count(); i++)
					{
						UseTsys(result, tsys[i]->PtrOf());
					}
				}
				break;
			case symbol_component::SymbolKind::ValueAlias:
				{
					auto decl = declSymbol->GetImplDecl_NFb<ValueAliasDeclaration>();
					if (!decl->templateSpec) throw NotConvertableException();
					auto& tsys = EvaluateValueAliasSymbol(pa, decl.Obj(), parentDeclType, argumentsToApply);
					UseTypeTsysList(result, tsys);
				}
				break;
			default:
				throw NotConvertableException();
			}
		});
	}
}