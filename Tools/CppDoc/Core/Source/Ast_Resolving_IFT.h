#ifndef VCZH_DOCUMENT_CPPDOC_AST_RESOLVING_IFT
#define VCZH_DOCUMENT_CPPDOC_AST_RESOLVING_IFT

#include "Ast_Resolving.h"
#include "Ast_Expr.h"

namespace infer_function_type
{
	extern Symbol*							TemplateArgumentPatternToSymbol(ITsys* tsys);

	extern void								CollectFreeTypes(
												const ParsingArguments& pa,
												bool includeParentDeclArguments,
												Ptr<Type> type,
												Ptr<Expr> expr,
												bool insideVariant,
												const SortedList<Symbol*>& freeTypeSymbols,
												SortedList<Type*>& involvedTypes,
												SortedList<Expr*>& involvedExprs
											);

	extern void								InferTemplateArgument(
												const ParsingArguments& pa,
												Ptr<Type> typeToInfer,
												Ptr<Expr> exprToInfer,
												ITsys* offeredType,
												TemplateArgumentContext& taContext,
												TemplateArgumentContext& variadicContext,
												const SortedList<Symbol*>& freeTypeSymbols,
												const SortedList<Type*>& involvedTypes,
												const SortedList<Expr*>& involvedExprs,
												bool exactMatchForParameters,
												ITsys** lastAssignedVta,
												SortedList<ITsys*>& hardcodedPatterns
											);

	extern void								SetInferredResult(
												const ParsingArguments& pa,
												TemplateArgumentContext& taContext,
												ITsys* pattern,
												ITsys* type,
												ITsys** lastAssignedVta,
												SortedList<ITsys*>& hardcodedPatterns
											);

	extern void								InferTemplateArgumentsForFunctionType(
												const ParsingArguments& pa,
												FunctionType* functionType,
												TypeTsysList& parameterAssignment,
												TemplateArgumentContext& taContext,
												TemplateArgumentContext& variadicContext,
												const SortedList<Symbol*>& freeTypeSymbols,
												bool exactMatchForParameters,
												ITsys** lastAssignedVta,
												SortedList<ITsys*>& hardcodedPatterns
											);

	extern void								InferTemplateArgumentsForGenericType(
												const ParsingArguments& pa,
												GenericType* genericType,
												TypeTsysList& parameterAssignment,
												TemplateArgumentContext& taContext,
												TemplateArgumentContext& variadicContext,
												const SortedList<Symbol*>& freeTypeSymbols,
												ITsys** lastAssignedVta,
												SortedList<ITsys*>& hardcodedPatterns
											);

	extern void								InferTemplateArgumentsForSpecializationSpec(
												const ParsingArguments& pa,
												SpecializationSpec* spec,
												TypeTsysList& parameterAssignment,
												TemplateArgumentContext& taContext,
												TemplateArgumentContext& variadicContext,
												const SortedList<Symbol*>& freeTypeSymbols,
												ITsys** lastAssignedVta,
												SortedList<ITsys*>& hardcodedPatterns
											);


	extern void								InferFunctionType(
												const ParsingArguments& pa,
												ExprTsysList& inferredFunctionTypes,
												ExprTsysItem funcType,
												Array<ExprTsysItem>& argTypes,
												SortedList<vint>& boundedAnys
											);

	extern Ptr<TemplateArgumentContext>		InferPartialSpecialization(
												const ParsingArguments& pa,
												Symbol* declSymbol,
												ITsys* parentDeclType,
												Ptr<TemplateSpec> templateSpec,
												Ptr<SpecializationSpec> specializationSpec,
												Ptr<TemplateSpec> primaryTemplateSpec,
												TemplateArgumentContext* argumentsToApply
											);

	extern bool								IsValuableTaContextWithMatchedPSChildren(
												TemplateArgumentContext* taContext
											);

	template<typename TCallback>
	void CollectInvolvedVariadicArguments(const ParsingArguments& pa, const SortedList<Type*>& involvedTypes, const SortedList<Expr*>& involvedExprs, TCallback&& callback)
	{
		for (vint j = 0; j < involvedTypes.Count(); j++)
		{
			if (auto idType = dynamic_cast<IdType*>(involvedTypes[j]))
			{
				auto patternSymbol = idType->resolving->resolvedSymbols[0];
				auto pattern = symbol_type_resolving::GetTemplateArgumentKey(patternSymbol, pa.tsys.Obj());
				if (patternSymbol->ellipsis)
				{
					callback(patternSymbol, pattern);
				}
			}
		}

		for (vint j = 0; j < involvedExprs.Count(); j++)
		{
			if (auto idExpr = dynamic_cast<IdExpr*>(involvedExprs[j]))
			{
				auto patternSymbol = idExpr->resolving->resolvedSymbols[0];
				auto pattern = symbol_type_resolving::GetTemplateArgumentKey(patternSymbol, pa.tsys.Obj());
				if (patternSymbol->ellipsis)
				{
					callback(patternSymbol, pattern);
				}
			}
		}
	}

	template<typename TDecl>
	bool InferPartialSpecializationPrimaryInternal(
		const ParsingArguments& pa,
		Dictionary<Symbol*, Ptr<TemplateArgumentContext>>& result,
		Symbol* declSymbol,
		ITsys* parentDeclType,
		Ptr<TemplateSpec> primaryTemplateSpec,
		TemplateArgumentContext* argumentsToApply,
		SortedList<Symbol*>& accessed
	)
	{
		if (accessed.Contains(declSymbol))
		{
			return result.Keys().Contains(declSymbol);
		}
		else
		{
			accessed.Add(declSymbol);

			auto decl = declSymbol->GetAnyForwardDecl<TDecl>();
			Ptr<TemplateArgumentContext> taContext;
			if (decl->specializationSpec)
			{
				taContext = InferPartialSpecialization(pa, declSymbol, parentDeclType, decl->templateSpec, decl->specializationSpec, primaryTemplateSpec, argumentsToApply);
				if (!taContext)
				{
					return false;
				}
			}

			auto& children = declSymbol->GetPSChildren_NF();
			vint counter = 0;
			for (vint i = 0; i < children.Count(); i++)
			{
				if (InferPartialSpecializationPrimaryInternal<TDecl>(pa, result, children[i], parentDeclType, primaryTemplateSpec, argumentsToApply, accessed))
				{
					counter++;
				}
			}

			if (taContext)
			{
				if (counter > 0)
				{
					if (IsValuableTaContextWithMatchedPSChildren(taContext.Obj()))
					{
						result.Add(declSymbol, taContext);
					}
				}
				else
				{
					result.Add(declSymbol, taContext);
				}
			}
			return true;
		}
	}

	template<typename TDecl>
	void InferPartialSpecializationPrimary(
		const ParsingArguments& pa,
		Dictionary<Symbol*, Ptr<TemplateArgumentContext>>& result,
		Symbol* primarySymbol,
		ITsys* parentDeclType,
		TemplateArgumentContext* argumentsToApply
	)
	{
		SortedList<Symbol*> accessed;
		auto decl = primarySymbol->GetAnyForwardDecl<TDecl>();
		InferPartialSpecializationPrimaryInternal<TDecl>(pa, result, primarySymbol, parentDeclType, decl->templateSpec, argumentsToApply, accessed);
	}
}

#endif