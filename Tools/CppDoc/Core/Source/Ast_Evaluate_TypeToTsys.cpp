#include "Ast_Resolving_ExpandPotentialVta.h"

using namespace symbol_type_resolving;
using namespace symbol_totsys_impl;

/***********************************************************************
TypeToTsys
  PrimitiveType				: literal
  ReferenceType				: unbounded
  ArrayType					: unbounded
  CallingConventionType		: unbounded		*
  FunctionType				: variant
  MemberType				: unbounded
  DeclType					: unbounded
  DecorateType				: unbounded
  RootType					: literal		*
  IdType					: identifier
  ChildType					: unbounded
  GenericType				: variant		+
***********************************************************************/

class TypeToTsysVisitor : public Object, public virtual ITypeVisitor
{
public:
	TypeTsysList&				result;
	bool						isVta = false;

	const ParsingArguments&		pa;
	TypeTsysList*				returnTypes;
	GenericArgContext*			gaContext = nullptr;
	bool						memberOf = false;
	TsysCallingConvention		cc = TsysCallingConvention::None;

	TypeToTsysVisitor(const ParsingArguments& _pa, TypeTsysList& _result, TypeTsysList* _returnTypes, GenericArgContext* _gaContext, bool _memberOf, TsysCallingConvention _cc)
		:pa(_pa)
		, result(_result)
		, returnTypes(_returnTypes)
		, gaContext(_gaContext)
		, cc(_cc)
		, memberOf(_memberOf)
	{
	}

	void AddResult(ITsys* tsys)
	{
		AddTsysToResult(result, tsys);
	}

	//////////////////////////////////////////////////////////////////////////////////////
	// PrimitiveType
	//////////////////////////////////////////////////////////////////////////////////////

	void Visit(PrimitiveType* self)override
	{
		AddResult(ProcessPrimitiveType(pa, self));
	}

	//////////////////////////////////////////////////////////////////////////////////////
	// ReferenceType
	//////////////////////////////////////////////////////////////////////////////////////

	void Visit(ReferenceType* self)override
	{
		TypeTsysList items1;
		bool isVta1 = false;
		TypeToTsysInternal(pa, self->type, items1, gaContext, isVta1);
		isVta = ExpandPotentialVta(pa, result, [=](ExprTsysItem arg1)
		{
			return ProcessReferenceType(pa, self, arg1);
		}, Input(items1, isVta1));
	}

	//////////////////////////////////////////////////////////////////////////////////////
	// ArrayType
	//////////////////////////////////////////////////////////////////////////////////////

	void Visit(ArrayType* self)override
	{
		TypeTsysList items1;
		bool isVta1 = false;
		TypeToTsysInternal(pa, self->type, items1, gaContext, isVta1);
		isVta = ExpandPotentialVta(pa, result, [=](ExprTsysItem arg1)
		{
			return ProcessArrayType(pa, self, arg1);
		}, Input(items1, isVta1));
	}

	//////////////////////////////////////////////////////////////////////////////////////
	// CallingConventionType
	//////////////////////////////////////////////////////////////////////////////////////

	void Visit(CallingConventionType* self)override
	{
		auto oldCc = cc;
		cc = self->callingConvention;

		self->type->Accept(this);
		cc = oldCc;
	}

	//////////////////////////////////////////////////////////////////////////////////////
	// FunctionType
	//////////////////////////////////////////////////////////////////////////////////////

	void Visit(FunctionType* self)override
	{
		VariantInput<ITsys*> variantInput(self->parameters.Count() + 1, pa, gaContext);
		if (returnTypes)
		{
			variantInput.ApplyTypes(0, *returnTypes);
		}
		else if (self->decoratorReturnType)
		{
			variantInput.ApplySingle(0, self->decoratorReturnType);
		}
		else if (self->returnType)
		{
			variantInput.ApplySingle(0, self->returnType);
		}
		else
		{
			TypeTsysList types;
			types.Add(pa.tsys->Void());
			variantInput.ApplyTypes(0, types);
		}
		variantInput.ApplyVariadicList(1, self->parameters);

		isVta = variantInput.Expand(&self->parameters, result,
			[=](ExprTsysList& processResult, Array<ExprTsysItem>& args, SortedList<vint>& boundedAnys)
			{
				ProcessFunctionType(pa, processResult, self, cc, memberOf, args, boundedAnys);
			});
	}

	//////////////////////////////////////////////////////////////////////////////////////
	// MemberType
	//////////////////////////////////////////////////////////////////////////////////////

	void Visit(MemberType* self)override
	{
		TypeTsysList types, classTypes;
		bool typesVta = false;
		bool classTypesVta = false;
		TypeToTsysInternal(pa, self->type, types, gaContext, typesVta, true, cc);
		TypeToTsysInternal(pa, self->classType, classTypes, gaContext, classTypesVta);
		isVta = ExpandPotentialVta(pa, result, [=](ExprTsysItem argType, ExprTsysItem argClass)
		{
			return ProcessMemberType(pa, self, argType, argClass);
		}, Input(types, typesVta), Input(classTypes, classTypesVta));
	}

	//////////////////////////////////////////////////////////////////////////////////////
	// DeclType
	//////////////////////////////////////////////////////////////////////////////////////

	void Visit(DeclType* self)override
	{
		if (self->expr)
		{
			ExprTsysList types;
			bool typesVta = false;
			ExprToTsysInternal(pa, self->expr, types, typesVta, gaContext);
			isVta = ExpandPotentialVta(pa, result, [=](ExprTsysItem arg1)
			{
				return ProcessDeclType(pa, self, arg1);
			}, Input(types, typesVta));
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////
	// DecorateType
	//////////////////////////////////////////////////////////////////////////////////////

	void Visit(DecorateType* self)override
	{
		TypeTsysList items1;
		bool isVta1 = false;
		TypeToTsysInternal(pa, self->type, items1, gaContext, isVta1);
		isVta = ExpandPotentialVta(pa, result, [=](ExprTsysItem arg1)
		{
			return ProcessDecorateType(pa, self, arg1);
		}, Input(items1, isVta1));
	}

	//////////////////////////////////////////////////////////////////////////////////////
	// RootType
	//////////////////////////////////////////////////////////////////////////////////////

	void Visit(RootType* self)override
	{
		throw NotConvertableException();
	}

	//////////////////////////////////////////////////////////////////////////////////////
	// IdType
	//////////////////////////////////////////////////////////////////////////////////////

	void Visit(IdType* self)override
	{
		CreateIdReferenceType(pa, gaContext, self->resolving, false, true, result, isVta);
	}

	//////////////////////////////////////////////////////////////////////////////////////
	// ChildType
	//////////////////////////////////////////////////////////////////////////////////////

	void Visit(ChildType* self)override
	{
		if (!IsResolvedToType(self->classType))
		{
			CreateIdReferenceType(pa, gaContext, self->resolving, true, false, result, isVta);
			return;
		}

		TypeTsysList classTypes;
		bool classVta = false;
		TypeToTsysInternal(pa, self->classType, classTypes, gaContext, classVta);

		isVta = ExpandPotentialVtaMultiResult(pa, result, [=](ExprTsysList& processResult, ExprTsysItem argClass)
		{
			ProcessChildType(pa, gaContext, self, argClass, processResult);
		}, Input(classTypes, classVta));
	}

	//////////////////////////////////////////////////////////////////////////////////////
	// GenericType
	//////////////////////////////////////////////////////////////////////////////////////

	void Visit(GenericType* self)override
	{
		vint count = self->arguments.Count() + 1;
		Array<ExprTsysList> argItems(count);
		Array<bool> isTypes(count);
		Array<bool> isVtas(count);

		{
			TypeTsysList tsyses;
			TypeToTsysInternal(pa, self->type, tsyses, gaContext, isVtas[0]);
			AddTemp(argItems[0], tsyses);
		}
		ResolveGenericArguments(pa, self->arguments, argItems, isTypes, isVtas, 1, gaContext);

		bool hasBoundedVta = false;
		bool hasUnboundedVta = false;
		vint unboundedVtaCount = -1;
		isVta = CheckVta(self->arguments, argItems, isVtas, 1, hasBoundedVta, hasUnboundedVta, unboundedVtaCount);

		ExpandPotentialVtaList(pa, result, argItems, isVtas, hasBoundedVta, unboundedVtaCount,
			[&](ExprTsysList& processResult, Array<ExprTsysItem>& args, SortedList<vint>& boundedAnys)
			{
				if (boundedAnys.Count() > 0)
				{
					// TODO: Implement variadic template argument passing
					throw NotConvertableException();
				}

				auto genericFunction = args[0].tsys;
				if (genericFunction->GetType() == TsysType::GenericFunction)
				{
					auto declSymbol = genericFunction->GetGenericFunction().declSymbol;
					if (!declSymbol)
					{
						throw NotConvertableException();
					}

					EvaluateSymbolContext esContext;
					if (!ResolveGenericParameters(pa, genericFunction, args, isTypes, 1, &esContext.gaContext))
					{
						throw NotConvertableException();
					}

					switch (declSymbol->kind)
					{
					case symbol_component::SymbolKind::GenericTypeArgument:
						genericFunction->GetElement()->ReplaceGenericArgs(esContext.gaContext, esContext.evaluatedTypes);
						break;
					case symbol_component::SymbolKind::TypeAlias:
						{
							auto decl = declSymbol->definition.Cast<TypeAliasDeclaration>();
							if (!decl->templateSpec) throw NotConvertableException();
							EvaluateSymbol(pa, decl.Obj(), &esContext);
						}
						break;
					default:
						throw NotConvertableException();
					}

					for (vint j = 0; j < esContext.evaluatedTypes.Count(); j++)
					{
						AddExprTsysItemToResult(processResult, GetExprTsysItem(esContext.evaluatedTypes[j]));
					}
				}
				else if (genericFunction->GetType() == TsysType::Any)
				{
					AddExprTsysItemToResult(processResult, GetExprTsysItem(pa.tsys->Any()));
				}
				else
				{
					throw NotConvertableException();
				}
			});
	}
};

// Convert type AST to type system object

void TypeToTsysInternal(const ParsingArguments& pa, Type* t, TypeTsysList& tsys, GenericArgContext* gaContext, bool& isVta, bool memberOf, TsysCallingConvention cc)
{
	if (!t) throw NotConvertableException();
	TypeToTsysVisitor visitor(pa, tsys, nullptr, gaContext, memberOf, cc);
	t->Accept(&visitor);
	isVta = visitor.isVta;
}
void TypeToTsysInternal(const ParsingArguments& pa, Ptr<Type> t, TypeTsysList& tsys, GenericArgContext* gaContext, bool& isVta, bool memberOf, TsysCallingConvention cc)
{
	TypeToTsysInternal(pa, t.Obj(), tsys, gaContext, isVta, memberOf, cc);
}

void TypeToTsysNoVta(const ParsingArguments& pa, Type* t, TypeTsysList& tsys, GenericArgContext* gaContext, bool memberOf, TsysCallingConvention cc)
{
	bool isVta = false;
	TypeToTsysInternal(pa, t, tsys, gaContext, isVta, memberOf, cc);
	if (isVta)
	{
		throw NotConvertableException();
	}
}

void TypeToTsysNoVta(const ParsingArguments& pa, Ptr<Type> t, TypeTsysList& tsys, GenericArgContext* gaContext, bool memberOf, TsysCallingConvention cc)
{
	TypeToTsysNoVta(pa, t.Obj(), tsys, gaContext, memberOf, cc);
}

void TypeToTsysAndReplaceFunctionReturnType(const ParsingArguments& pa, Ptr<Type> t, TypeTsysList& returnTypes, TypeTsysList& tsys, GenericArgContext* gaContext, bool memberOf)
{
	if (!t) throw NotConvertableException();
	TypeToTsysVisitor visitor(pa, tsys, &returnTypes, gaContext, memberOf, TsysCallingConvention::None);
	t->Accept(&visitor);
	if (visitor.isVta)
	{
		throw NotConvertableException();
	}
}