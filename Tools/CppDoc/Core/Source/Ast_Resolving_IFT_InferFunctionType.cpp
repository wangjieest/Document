#include "Ast_Resolving_IFT.h"

namespace symbol_type_resolving
{
	/***********************************************************************
	TemplateArgumentPatternToSymbol:	Get the symbol from a type representing a template argument
	***********************************************************************/

	Symbol* TemplateArgumentPatternToSymbol(ITsys* tsys)
	{
		switch (tsys->GetType())
		{
		case TsysType::GenericArg:
			return tsys->GetGenericArg().argSymbol;
		default:
			throw TypeCheckerException();
		}
	}

	/***********************************************************************
	SetInferredResult:	Set a inferred type for a template argument, and check if it is compatible with previous result
	***********************************************************************/

	void SetInferredResult(TemplateArgumentContext& taContext, ITsys* pattern, ITsys* type)
	{
		vint index = taContext.arguments.Keys().IndexOf(pattern);
		if (index == -1)
		{
			// if this argument is not inferred, use the result
			taContext.arguments.Add(pattern, type);
		}
		else if (type->GetType() != TsysType::Any)
		{
			// if this argument is inferred, it requires the same result if both of them are not any_t
			auto inferred = taContext.arguments.Values()[index];
			if (inferred->GetType() == TsysType::Any)
			{
				taContext.arguments.Set(pattern, type);
			}
			else if (type != inferred)
			{
				throw TypeCheckerException();
			}
		}
	}

	/***********************************************************************
	InferTemplateArgument:	Perform type inferencing for a template argument
	***********************************************************************/

	void InferTemplateArgument(
		const ParsingArguments& pa,
		Ptr<Type> argumentType,
		bool isVariadic,
		ITsys* assignedTsys,
		TemplateArgumentContext& taContext,
		const SortedList<Symbol*>& freeTypeSymbols,
		bool exactMatchForParameters
	)
	{
		SortedList<Type*> involvedTypes;
		CollectFreeTypes(argumentType, isVariadic, freeTypeSymbols, involvedTypes);

		// get all affected arguments
		List<ITsys*> vas;
		List<ITsys*> nvas;
		for (vint j = 0; j < involvedTypes.Count(); j++)
		{
			if (auto idType = dynamic_cast<IdType*>(involvedTypes[j]))
			{
				auto patternSymbol = idType->resolving->resolvedSymbols[0];
				auto pattern = EvaluateGenericArgumentSymbol(patternSymbol);
				if (patternSymbol->ellipsis)
				{
					vas.Add(pattern);
				}
				else
				{
					nvas.Add(pattern);
				}
			}
		}

		// infer all affected types to any_t, result will be overrided if more precise types are inferred
		for (vint j = 0; j < vas.Count(); j++)
		{
			SetInferredResult(taContext, vas[j], pa.tsys->Any());
		}
		for (vint j = 0; j < nvas.Count(); j++)
		{
			SetInferredResult(taContext, nvas[j], pa.tsys->Any());
		}

		if (assignedTsys->GetType() != TsysType::Any)
		{
			if (isVariadic)
			{
				// for variadic parameter
				vint count = assignedTsys->GetParamCount();
				if (count == 0)
				{
					// if the assigned argument is an empty list, infer all variadic arguments to empty
					Array<ExprTsysItem> params;
					auto init = pa.tsys->InitOf(params);
					for (vint j = 0; j < vas.Count(); j++)
					{
						SetInferredResult(taContext, vas[j], init);
					}
				}
				else
				{
					// if the assigned argument is a non-empty list
					Dictionary<ITsys*, Ptr<Array<ExprTsysItem>>> variadicResults;
					for (vint j = 0; j < vas.Count(); j++)
					{
						variadicResults.Add(vas[j], MakePtr<Array<ExprTsysItem>>(count));
					}

					// run each item in the list
					for (vint j = 0; j < count; j++)
					{
						auto assignedTsysItem = ApplyExprTsysType(assignedTsys->GetParam(j), assignedTsys->GetInit().headers[j].type);
						TemplateArgumentContext variadicContext;
						InferTemplateArgument(pa, argumentType, assignedTsysItem, taContext, variadicContext, freeTypeSymbols, involvedTypes, exactMatchForParameters);
						for (vint k = 0; k < variadicContext.arguments.Count(); k++)
						{
							auto key = variadicContext.arguments.Keys()[k];
							auto value = variadicContext.arguments.Values()[k];
							auto result = variadicResults[key];
							result->Set(j, { nullptr,ExprTsysType::PRValue,value });
						}
					}

					// aggregate them
					for (vint j = 0; j < vas.Count(); j++)
					{
						auto pattern = vas[j];
						auto& params = *variadicResults[pattern].Obj();
						auto init = pa.tsys->InitOf(params);
						SetInferredResult(taContext, pattern, init);
					}
				}
			}
			else
			{
				// for non-variadic parameter, run the assigned argument
				TemplateArgumentContext unusedVariadicContext;
				InferTemplateArgument(pa, argumentType, assignedTsys, taContext, unusedVariadicContext, freeTypeSymbols, involvedTypes, exactMatchForParameters);
				if (unusedVariadicContext.arguments.Count() > 0)
				{
					throw TypeCheckerException();
				}
			}
		}
	}

	/***********************************************************************
	InferTemplateArgumentsForGenericType:	Perform type inferencing for template class offered arguments
	***********************************************************************/

	void InferTemplateArgumentsForGenericType(
		const ParsingArguments& pa,
		GenericType* genericType,
		List<ITsys*>& parameterAssignment,
		TemplateArgumentContext& taContext,
		const SortedList<Symbol*>& freeTypeSymbols
	)
	{
		for (vint i = 0; i < genericType->arguments.Count(); i++)
		{
			auto argument = genericType->arguments[i];
			// if this is a value argument, skip it
			if (argument.item.type)
			{
				auto assignedTsys = parameterAssignment[i];
				InferTemplateArgument(pa, argument.item.type, argument.isVariadic, assignedTsys, taContext, freeTypeSymbols, true);
			}
		}
	}

	/***********************************************************************
	InferTemplateArgumentsForFunctionType:	Perform type inferencing for template function offered arguments
	***********************************************************************/

	void InferTemplateArgumentsForFunctionType(
		const ParsingArguments& pa,
		FunctionType* functionType,
		List<ITsys*>& parameterAssignment,
		TemplateArgumentContext& taContext,
		const SortedList<Symbol*>& freeTypeSymbols,
		bool exactMatchForParameters
	)
	{
		// don't care about arguments for ellipsis
		for (vint i = 0; i < functionType->parameters.Count(); i++)
		{
			// if default value is used, skip it
			if (auto assignedTsys = parameterAssignment[i])
			{
				// see if any variadic value arguments can be determined
				// variadic value argument only care about the number of values
				auto parameter = functionType->parameters[i];
				InferTemplateArgument(pa, parameter.item->type, parameter.isVariadic, assignedTsys, taContext, freeTypeSymbols, exactMatchForParameters);
			}
		}
	}

	/***********************************************************************
	InferFunctionType:	Perform type inferencing for template function using both offered template and function arguments
						Ts(*)(X<Ts...>)... or Ts<X<Ts<Y>...>... is not supported, because of nested Ts...
	***********************************************************************/

	Nullable<ExprTsysItem> InferFunctionType(const ParsingArguments& pa, ExprTsysItem functionItem, Array<ExprTsysItem>& argTypes, SortedList<vint>& boundedAnys)
	{
		switch (functionItem.tsys->GetType())
		{
		case TsysType::Function:
			return functionItem;
		case TsysType::LRef:
		case TsysType::RRef:
		case TsysType::CV:
		case TsysType::Ptr:
			return InferFunctionType(pa, { functionItem,functionItem.tsys->GetElement() }, argTypes, boundedAnys);
		case TsysType::GenericFunction:
			if (auto symbol = functionItem.symbol)
			{
				if (auto decl = symbol->GetAnyForwardDecl<ForwardFunctionDeclaration>())
				{
					if (auto functionType = GetTypeWithoutMemberAndCC(decl->type).Cast<FunctionType>())
					{
						try
						{
							auto gfi = functionItem.tsys->GetGenericFunction();
							if (gfi.filledArguments != 0) throw TypeCheckerException();

							List<ITsys*>						parameterAssignment;
							TemplateArgumentContext				taContext;
							SortedList<Symbol*>					freeTypeSymbols;

							auto inferPa = pa.AdjustForDecl(gfi.declSymbol, gfi.parentDeclType, false);

							// cannot pass Ptr<FunctionType> to this function since the last filled argument could be variadic
							// known variadic function argument should be treated as separated arguments
							// ParsingArguments need to be adjusted so that we can evaluate each parameter type
							ResolveFunctionParameters(pa, parameterAssignment, functionType.Obj(), argTypes, boundedAnys);

							// fill freeTypeSymbols with all template arguments
							// fill freeTypeAssignments with only assigned template arguments
							for (vint i = 0; i < gfi.spec->arguments.Count(); i++)
							{
								auto argument = gfi.spec->arguments[i];
								auto pattern = GetTemplateArgumentKey(argument, pa.tsys.Obj());
								auto patternSymbol = TemplateArgumentPatternToSymbol(pattern);
								freeTypeSymbols.Add(patternSymbol);
							}

							// type inferencing
							InferTemplateArgumentsForFunctionType(inferPa, functionType.Obj(), parameterAssignment, taContext, freeTypeSymbols, false);

							// fill template value arguments
							for (vint i = 0; i < gfi.spec->arguments.Count(); i++)
							{
								auto argument = gfi.spec->arguments[i];
								if (argument.argumentType == CppTemplateArgumentType::Value)
								{
									auto pattern = GetTemplateArgumentKey(argument, pa.tsys.Obj());
									auto patternSymbol = TemplateArgumentPatternToSymbol(pattern);
									// TODO: not implemented
									// InferTemplateArgumentsForFunctionType need to handle this
									throw 0;
									/*
									vint index = freeTypeAssignments.Keys().IndexOf(patternSymbol);
									if (index == -1)
									{
										throw TypeCheckerException();
									}
									else if (argument.ellipsis)
									{
										// consistent with ResolveGenericParameters
										vint assignment = freeTypeAssignments.Values()[index];
										if (assignment == FREE_TYPE_ANY)
										{
											taContext.arguments.Add(pattern, pa.tsys->Any());
										}
										else
										{
											Array<ExprTsysItem> items(assignment);
											for (vint j = 0; j < assignment; j++)
											{
												items[j] = { nullptr,ExprTsysType::PRValue,nullptr };
											}
											auto init = pa.tsys->InitOf(items);
											taContext.arguments.Add(pattern, init);
										}
									}
									else
									{
										taContext.arguments.Add(pattern, nullptr);
									}
									*/
								}
							}

							taContext.symbolToApply = gfi.declSymbol;
							auto& tsys = EvaluateFuncSymbol(inferPa, decl.Obj(), inferPa.parentDeclType, &taContext);
							if (tsys.Count() == 0)
							{
								return {};
							}
							else if (tsys.Count() == 1)
							{
								return { { functionItem,tsys[0] } };
							}
							else
							{
								// unable to handle multiple types
								throw IllegalExprException();
							}
						}
						catch (const TypeCheckerException&)
						{
							return {};
						}
					}
				}
			}
		default:
			if (functionItem.tsys->IsUnknownType())
			{
				return functionItem;
			}
			return {};
		}
	}
}