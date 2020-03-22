#include "Ast_Resolving_AP_CalculateGpa.h"

using namespace symbol_type_resolving;
using namespace infer_function_type;

namespace assign_parameters
{
	/***********************************************************************
	ResolveFunctionParameters: Calculate function parameter types by matching arguments to patterens
	ResolveGenericTypeParameters: Calculate generic parameter types by matching arguments to patterens
	***********************************************************************/

	void ResolveGenericTypeParameters(
		const ParsingArguments& invokerPa,				// context
		TypeTsysList& parameterAssignment,				// store function argument to offered argument map, nullptr indicates the default value is applied
		const TemplateArgumentContext& knownArguments,	// all assigned template arguments
		const SortedList<Symbol*>& argumentSymbols,		// symbols of all template arguments
		GenericType* genericType,						// argument information
		Array<ExprTsysItem>& argumentTypes,				// (index of unpacked)		offered argument (unpacked)
		SortedList<vint>& boundedAnys					// (value of unpacked)		for each offered argument that is any_t, and it means unknown variadic arguments, instead of an unknown type
	)
	{
		auto adjustedPa = AdjustPaForCollecting(invokerPa);

		vint genericParameterCount = genericType->arguments.Count();
		vint passedParameterCount = genericParameterCount + 1;
		Array<vint> knownPackSizes(passedParameterCount);
		for (vint i = 0; i < knownPackSizes.Count(); i++)
		{
			knownPackSizes[i] = -1;
		}

		// calculate all variadic function arguments that with known pack size
		for (vint i = 0; i < genericType->arguments.Count(); i++)
		{
			auto& argument = genericType->arguments[i];
			if (argument.isVariadic)
			{
				SortedList<Type*> involvedTypes;
				SortedList<Expr*> involvedExprs;
				CollectFreeTypes(adjustedPa, true, argument.item.type, argument.item.expr, false, argumentSymbols, involvedTypes, involvedExprs);
				knownPackSizes[i] = CalculateParameterPackSize(adjustedPa, knownArguments, involvedTypes, involvedExprs);
			}
			else
			{
				knownPackSizes[i] = 1;
			}
		}

		// calculate how to assign offered arguments to generic type arguments
		// gpaMappings will contains decisions for every template arguments
		// set allowPartialApply to true because, arguments of genericType could be incomplete, but argumentTypes are always complete because it comes from a ITsys*
		GpaList gpaMappings;
		CalculateGpa(gpaMappings, argumentTypes.Count(), boundedAnys, 0, true, passedParameterCount, knownPackSizes,
			[genericType](vint index) { return index == genericType->arguments.Count() ? true : genericType->arguments[index].isVariadic; },
			[](vint index) { return false; }
		);
		AssignParameterAssignment(adjustedPa, genericParameterCount, parameterAssignment, gpaMappings, argumentTypes);
	}
}