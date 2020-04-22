#include "AP_CalculateGpa.h"

using namespace symbol_type_resolving;

namespace assign_parameters
{
	/***********************************************************************
	ResolveGenericParameters: Fill TemplateArgumentContext by matching type/value arguments to template arguments
	***********************************************************************/

	void ResolveGenericParameters(
		const ParsingArguments& invokerPa,			// context
		TemplateArgumentContext& newTaContext,		// TAC to store type arguemnt to offered argument map, vta argument will be grouped to Init or Any
		ITsys* genericFunction,						// generic type header
		Array<ExprTsysItem>& argumentTypes,			// (index of unpacked)		offered argument (unpacked), starts from argumentTypes[offset]
		Array<bool>& isTypes,						// (index of packed)		( isTypes[j + offset] == true )			means the j-th offered arguments (packed) is a type instead of a value
		Array<vint>& argSource,						// (unpacked -> packed)		( argSource[i + offset] == j + offset )	means argumentTypes[i + offset] is from whole or part of the j-th packed offered argument
		SortedList<vint>& boundedAnys,				// (value of unpacked)		( boundedAnys[x] == i + offset )		means argumentTypes[i + offset] is any_t, and it means unknown variadic arguments, instead of an unknown type
		vint offset,								// means first offset types are not template arguments or offered arguments, they stored other things
		bool allowPartialApply,						// true means it is legal to not offer enough amount of arguments
		vint& partialAppliedArguments				// the number of applied template arguments, -1 if all arguments are filled
	)
	{
		if (genericFunction->GetType() != TsysType::GenericFunction)
		{
			throw TypeCheckerException();
		}

		const auto& genericFuncInfo = genericFunction->GetGenericFunction();
		auto spec = genericFuncInfo.spec;
		vint inputArgumentCount = argumentTypes.Count() - offset;

		// calculate how to assign offered arguments to template arguments
		// gpaMappings will contains decisions for every template arguments
		// if there are not enough offered arguments, only the first few templates are assigned decisions
		GpaList gpaMappings;
		Array<vint> knownPackSizes;
		CalculateGpa(gpaMappings, inputArgumentCount, boundedAnys, offset, allowPartialApply, spec->arguments.Count(), knownPackSizes,
			[&spec](vint index) { return spec->arguments[index].ellipsis; },
			[&spec](vint index) { auto argument = spec->arguments[index]; return argument.argumentType == CppTemplateArgumentType::Value ? argument.expr : argument.type; }
			);

		auto pa = invokerPa.AdjustForDecl(genericFuncInfo.declSymbol).AppendSingleLevelArgs(newTaContext);
		for (vint i = 0; i < genericFunction->GetParamCount(); i++)
		{
			auto gpa = gpaMappings[i];
			auto pattern = genericFunction->GetParam(i);
			bool acceptType = spec->arguments[i].argumentType != CppTemplateArgumentType::Value;

			switch (gpa.kind)
			{
			case GenericParameterAssignmentKind::DefaultValue:
				{
					// if a default value is expected to fill this template argument
					if (acceptType)
					{
						if (spec->arguments[i].argumentType == CppTemplateArgumentType::Value)
						{
							throw TypeCheckerException();
						}
						TypeTsysList argTypes;
						TypeToTsysNoVta(pa.WithScope(genericFuncInfo.declSymbol), spec->arguments[i].type, argTypes);

						for (vint j = 0; j < argTypes.Count(); j++)
						{
							newTaContext.arguments.Add(pattern, argTypes[j]);
						}
					}
					else
					{
						if (spec->arguments[i].argumentType != CppTemplateArgumentType::Value)
						{
							throw TypeCheckerException();
						}
						newTaContext.arguments.Add(pattern, nullptr);
					}
				}
				break;
			case GenericParameterAssignmentKind::OneArgument:
				{
					// if an offered argument is to fill this template argument
					if (acceptType != isTypes[argSource[gpa.index]])
					{
						throw TypeCheckerException();
					}

					if (acceptType)
					{
						auto item = argumentTypes[gpa.index];
						EnsureGenericTypeParameterAndArgumentMatched(pattern, item.tsys);
						newTaContext.arguments.Add(pattern, item.tsys);
					}
					else
					{
						newTaContext.arguments.Add(pattern, nullptr);
					}
				}
				break;
			case GenericParameterAssignmentKind::EmptyVta:
				{
					// if an empty pack of offered arguments is to fill this variadic template argument
					Array<ExprTsysItem> items;
					auto init = pa.tsys->InitOf(items);
					newTaContext.arguments.Add(pattern, init);
				}
				break;
			case GenericParameterAssignmentKind::MultipleVta:
				{
					// if a pack of offered arguments is to fill this variadic template argument
					for (vint j = 0; j < gpa.count; j++)
					{
						if (acceptType != isTypes[argSource[gpa.index + j]])
						{
							throw TypeCheckerException();
						}
					}

					Array<ExprTsysItem> items(gpa.count);

					for (vint j = 0; j < gpa.count; j++)
					{
						if (acceptType)
						{
							auto item = argumentTypes[gpa.index + j];
							EnsureGenericTypeParameterAndArgumentMatched(pattern, item.tsys);
							items[j] = item;
						}
						else
						{
							items[j] = { nullptr,ExprTsysType::PRValue,nullptr };
						}
					}
					auto init = pa.tsys->InitOf(items);
					newTaContext.arguments.Add(pattern, init);
				}
				break;
			case GenericParameterAssignmentKind::Any:
				{
					// if any is to fill this (maybe variadic) template argument
					newTaContext.arguments.Add(pattern, pa.tsys->Any());
				}
				break;
			case GenericParameterAssignmentKind::Unfilled:
				{
					// if this template argument is not filled
					// fill them will template argument themselves
					partialAppliedArguments = i;
					for (vint j = i; j < genericFunction->GetParamCount(); j++)
					{
						auto unappliedPattern = genericFunction->GetParam(j);
						auto unappliedValue = spec->arguments[j].ellipsis ? pa.tsys->Any() : acceptType ? unappliedPattern : nullptr;
						newTaContext.arguments.Add(unappliedPattern, unappliedValue);
					}
					return;
				}
				break;
			}
		}
		partialAppliedArguments = -1;
	}
}