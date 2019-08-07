#include "Ast_Resolving.h"

namespace symbol_type_resolving
{
	/***********************************************************************
	CreateGenericFunctionHeader: Calculate enough information to create a generic function type
	***********************************************************************/

	void CreateGenericFunctionHeader(const ParsingArguments& pa, Ptr<TemplateSpec> spec, TypeTsysList& params, TsysGenericFunction& genericFunction)
	{
		genericFunction.variadicArgumentIndex = -1;
		genericFunction.acceptTypes.Resize(spec->arguments.Count());

		for (vint i = 0; i < spec->arguments.Count(); i++)
		{
			auto argument = spec->arguments[i];
			if ((genericFunction.acceptTypes[i] = (argument.argumentType == CppTemplateArgumentType::Type)))
			{
				params.Add(argument.argumentSymbol->evaluation.Get()[0]);
			}
			else
			{
				params.Add(pa.tsys->DeclOf(argument.argumentSymbol));
			}

			if (argument.ellipsis)
			{
				if (genericFunction.variadicArgumentIndex == -1)
				{
					genericFunction.variadicArgumentIndex = i;
				}
				else
				{
					throw NotConvertableException();
				}
			}
		}
	}

	/***********************************************************************
	ResolveGenericParameters: Calculate generic parameter types by matching arguments to patterens
	***********************************************************************/

	void EnsureGenericFunctionParameterAndArgumentMatched(ITsys* parameter, ITsys* argument);

	void EnsureGenericTypeParameterAndArgumentMatched(ITsys* parameter, ITsys* argument)
	{
		switch (parameter->GetType())
		{
		case TsysType::GenericArg:
			if (argument->GetType() == TsysType::GenericFunction)
			{
				throw NotConvertableException();
			}
			break;
		case TsysType::GenericFunction:
			if (argument->GetType() != TsysType::GenericFunction)
			{
				throw NotConvertableException();
			}
			EnsureGenericFunctionParameterAndArgumentMatched(parameter, argument);
			break;
		default:
			// until class specialization begins to develop, this should always not happen
			throw NotConvertableException();
		}
	}

	void EnsureGenericFunctionParameterAndArgumentMatched(ITsys* parameter, ITsys* argument)
	{
		if (parameter->GetParamCount() != argument->GetParamCount())
		{
			throw NotConvertableException();
		}
		for (vint i = 0; i < parameter->GetParamCount(); i++)
		{
			auto nestedParameter = parameter->GetParam(i);
			auto nestedArgument = argument->GetParam(i);

			bool acceptType = parameter->GetGenericFunction().acceptTypes[i];
			bool isType = argument->GetGenericFunction().acceptTypes[i];
			if (acceptType != isType)
			{
				throw NotConvertableException();
			}

			if (acceptType)
			{
				EnsureGenericTypeParameterAndArgumentMatched(nestedParameter, nestedArgument);
			}
		}
	}

	Ptr<TemplateSpec> GetTemplateSpec(const TsysGenericFunction& genericFuncInfo)
	{
		if (genericFuncInfo.declSymbol)
		{
			if (auto typeAliasDecl = genericFuncInfo.declSymbol->GetAnyForwardDecl<TypeAliasDeclaration>())
			{
				return typeAliasDecl->templateSpec;
			}
			else if (auto valueAliasDecl = genericFuncInfo.declSymbol->GetAnyForwardDecl<ValueAliasDeclaration>())
			{
				return valueAliasDecl->templateSpec;
			}
		}
		return nullptr;
	}

	void GetArgumentCountRange(ITsys* genericFunction, Ptr<TemplateSpec> spec, const TsysGenericFunction& genericFuncInfo, vint& minCount, vint& maxCount)
	{
		maxCount = genericFuncInfo.variadicArgumentIndex == -1 ? genericFunction->GetParamCount() : -1;

		vint defaultCount = 0;
		if (spec)
		{
			for (vint i = spec->arguments.Count() - 1; i >= 0; i--)
			{
				auto argument = spec->arguments[i];
				switch (argument.argumentType)
				{
				case CppTemplateArgumentType::HighLevelType:
				case CppTemplateArgumentType::Type:
					if (!argument.type) break;
					defaultCount++;
					break;
				case CppTemplateArgumentType::Value:
					if (!argument.expr) break;
					defaultCount++;
					break;
				}
			}
		}

		if (genericFuncInfo.variadicArgumentIndex == -1)
		{
			minCount = genericFunction->GetParamCount() - defaultCount;
			maxCount = genericFunction->GetParamCount();
		}
		else
		{
			minCount = genericFunction->GetParamCount() - (defaultCount == 0 ? 1 : defaultCount);
			maxCount = -1;
		}
	}

	enum class GenericParameterAssignmentKind
	{
		DefaultValue,
		OneArgument,
		EmptyVta,
		MultipleVta,
		Any,
	};

	struct GenericParameterAssignment
	{
		GenericParameterAssignmentKind		kind;
		vint								index;
		vint								count;

		GenericParameterAssignment() = default;
		GenericParameterAssignment(GenericParameterAssignmentKind _kind, vint _index, vint _count)
			:kind(_kind)
			, index(_index)
			, count(_count)
		{
		}

		static GenericParameterAssignment DefaultValue()
		{
			return { GenericParameterAssignmentKind::DefaultValue,-1,-1 };
		}

		static GenericParameterAssignment OneArgument(vint index)
		{
			return { GenericParameterAssignmentKind::OneArgument,index,-1 };
		}

		static GenericParameterAssignment EmptyVta()
		{
			return { GenericParameterAssignmentKind::EmptyVta,-1,-1 };
		}

		static GenericParameterAssignment MultipleVta(vint index, vint count)
		{
			return { GenericParameterAssignmentKind::MultipleVta,index,count };
		}

		static GenericParameterAssignment Any()
		{
			return { GenericParameterAssignmentKind::Any,-1,-1 };
		}
	};

	using GpaList = List<GenericParameterAssignment>;

	void CalculateGpa(GpaList& gpaMappings, ITsys* genericFunction, const TsysGenericFunction& genericFuncInfo, vint inputArgumentCount)
	{
		if (genericFuncInfo.variadicArgumentIndex == -1)
		{
			for (vint i = 0; i < genericFunction->GetParamCount(); i++)
			{
				if (i < inputArgumentCount)
				{
					gpaMappings.Add(GenericParameterAssignment::OneArgument(i));
				}
				else
				{
					gpaMappings.Add(GenericParameterAssignment::DefaultValue());
				}
			}
		}
		else
		{
			vint delta = inputArgumentCount - genericFunction->GetParamCount();
			for (vint i = 0; i < genericFuncInfo.variadicArgumentIndex; i++)
			{
				gpaMappings.Add(GenericParameterAssignment::OneArgument(i));
			}
			gpaMappings.Add(GenericParameterAssignment::MultipleVta(genericFuncInfo.variadicArgumentIndex, delta + 1));
			for (vint i = genericFuncInfo.variadicArgumentIndex + 1; i < genericFunction->GetParamCount(); i++)
			{
				gpaMappings.Add(GenericParameterAssignment::OneArgument(i + delta));
			}
		}
	}

	void ResolveGenericParameters(const ParsingArguments& pa, ITsys* genericFunction, Array<ExprTsysItem>& argumentTypes, Array<bool>& isTypes, vint offset, GenericArgContext* newGaContext)
	{
		if (genericFunction->GetType() != TsysType::GenericFunction)
		{
			throw NotConvertableException();
		}

		const auto& genericFuncInfo = genericFunction->GetGenericFunction();
		auto spec = GetTemplateSpec(genericFuncInfo);

		vint minCount = -1;
		vint maxCount = -1;
		GetArgumentCountRange(genericFunction, spec, genericFuncInfo, minCount, maxCount);

		vint inputArgumentCount = argumentTypes.Count() - offset;
		if (inputArgumentCount < minCount || (maxCount != -1 && inputArgumentCount > maxCount))
		{
			throw NotConvertableException();
		}

		GpaList gpaMappings;
		CalculateGpa(gpaMappings, genericFunction, genericFuncInfo, inputArgumentCount);

		for (vint i = 0; i < genericFunction->GetParamCount(); i++)
		{
			auto gpa = gpaMappings[i];
			auto pattern = genericFunction->GetParam(i);
			bool acceptType = genericFuncInfo.acceptTypes[i];

			switch (gpa.kind)
			{
			case GenericParameterAssignmentKind::DefaultValue:
				{
					if (acceptType)
					{
						if (spec->arguments[i].argumentType == CppTemplateArgumentType::Value)
						{
							throw NotConvertableException();
						}
						TypeTsysList argTypes;
						TypeToTsysNoVta(pa.WithContext(genericFuncInfo.declSymbol), spec->arguments[i].type, argTypes, newGaContext);

						for (vint j = 0; j < argTypes.Count(); j++)
						{
							newGaContext->arguments.Add(pattern, argTypes[j]);
						}
					}
					else
					{
						if (spec->arguments[i].argumentType != CppTemplateArgumentType::Value)
						{
							throw NotConvertableException();
						}
						newGaContext->arguments.Add(pattern, nullptr);
					}
				}
				break;
			case GenericParameterAssignmentKind::OneArgument:
				{
					if (acceptType != isTypes[gpa.index + offset])
					{
						throw NotConvertableException();
					}

					if (acceptType)
					{
						auto item = argumentTypes[gpa.index + offset];
						EnsureGenericTypeParameterAndArgumentMatched(pattern, item.tsys);
						newGaContext->arguments.Add(pattern, item.tsys);
					}
					else
					{
						newGaContext->arguments.Add(pattern, nullptr);
					}
				}
				break;
			case GenericParameterAssignmentKind::EmptyVta:
				{
					Array<ExprTsysItem> items;
					auto init = pa.tsys->InitOf(items);
					newGaContext->arguments.Add(pattern, init);
				}
				break;
			case GenericParameterAssignmentKind::MultipleVta:
				{
					for (vint j = 0; j < gpa.count; j++)
					{
						if (acceptType != isTypes[gpa.index + j + offset])
						{
							throw NotConvertableException();
						}
					}

					Array<ExprTsysItem> items(gpa.count);

					for (vint j = 0; j < gpa.count; j++)
					{
						if (acceptType)
						{
							auto item = argumentTypes[gpa.index + j + offset];
							EnsureGenericTypeParameterAndArgumentMatched(pattern, item.tsys);
							items[j] = item;
						}
						else
						{
							items[j] = { nullptr,ExprTsysType::PRValue,nullptr };
						}
					}
					auto init = pa.tsys->InitOf(items);
					newGaContext->arguments.Add(pattern, init);
				}
				break;
			}
		}
	}
}