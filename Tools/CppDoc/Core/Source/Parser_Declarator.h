#ifndef VCZH_DOCUMENT_CPPDOC_PARSER_DECLARATOR
#define VCZH_DOCUMENT_CPPDOC_PARSER_DECLARATOR

#include "Lexer.h"
#include "Ast.h"

class ClassDeclaration;

#define PARSING_DECLARATOR_ARGUMENTS(PREFIX, DELIMITER)												\
	ClassDeclaration*								PREFIX##classOfMemberInside		DELIMITER		\
	Ptr<Type>										PREFIX##classOfMemberOutside	DELIMITER		\
	List<Ptr<TemplateSpec>>*						PREFIX##specsOfMemberOutside	DELIMITER		\
	Ptr<Symbol>										PREFIX##scopeSymbolToReuse		DELIMITER		\
	bool											PREFIX##forParameter			DELIMITER		\
	DeclaratorRestriction							PREFIX##dr						DELIMITER		\
	InitializerRestriction							PREFIX##ir						DELIMITER		\
	bool											PREFIX##allowBitField			DELIMITER		\
	bool											PREFIX##allowEllipsis			DELIMITER		\
	bool											PREFIX##allowComma				DELIMITER		\
	bool											PREFIX##allowGtInInitializer	DELIMITER		\
	bool											PREFIX##allowSpecializationSpec	DELIMITER		\
	bool											PREFIX##allowClassMemberCache					\

#define PARSING_DECLARATOR_COPY(PREFIX)										\
	classOfMemberInside(PREFIX##classOfMemberInside)						\
	, classOfMemberOutside(PREFIX##classOfMemberOutside)					\
	, specsOfMemberOutside(PREFIX##specsOfMemberOutside)					\
	, scopeSymbolToReuse(PREFIX##scopeSymbolToReuse)						\
	, forParameter(PREFIX##forParameter)									\
	, dr(PREFIX##dr)														\
	, ir(PREFIX##ir)														\
	, allowBitField(PREFIX##allowBitField)									\
	, allowEllipsis(PREFIX##allowEllipsis)									\
	, allowComma(PREFIX##allowComma)										\
	, allowGtInInitializer(PREFIX##allowGtInInitializer)					\
	, allowSpecializationSpec(PREFIX##allowSpecializationSpec)				\
	, allowClassMemberCache(PREFIX##allowClassMemberCache)					\

enum class DeclaratorRestriction
{
	Zero,
	Optional,
	One,
	Many,
};

enum class InitializerRestriction
{
	Zero,
	Optional,
};

// Parser_Declarator.cpp
struct ParsingDeclaratorArguments
{
	PARSING_DECLARATOR_ARGUMENTS(, ;);

#define __ ,
	ParsingDeclaratorArguments(PARSING_DECLARATOR_ARGUMENTS(_, __))
		: PARSING_DECLARATOR_COPY(_)
	{
	}
#undef __
};

#define PDA_HEADER(NAME) inline ParsingDeclaratorArguments	pda_##NAME
//																					inside		outside		specs		symbol		param			declarator-count					initializer-count					bitfield		ellipsis		comma		=a>b	spec	cache
PDA_HEADER(Type)			()										{	return {	nullptr,	{},			nullptr,	nullptr,	false,			DeclaratorRestriction::Zero,		InitializerRestriction::Zero,		false,			false,			false,		false,	false,	false	}; } // Type
PDA_HEADER(VarType)			()										{	return {	nullptr,	{},			nullptr,	nullptr,	false,			DeclaratorRestriction::Optional,	InitializerRestriction::Zero,		false,			false,			false,		false,	false,	false	}; } // Type or Variable without Initializer
PDA_HEADER(VarInit)			()										{	return {	nullptr,	{},			nullptr,	nullptr,	false,			DeclaratorRestriction::One,			InitializerRestriction::Optional,	false,			false,			false,		true,	false,	false	}; } // Variable with Initializer
PDA_HEADER(VarNoInit)		()										{	return {	nullptr,	{},			nullptr,	nullptr,	false,			DeclaratorRestriction::One,			InitializerRestriction::Zero,		false,			false,			false,		false,	false,	false	}; } // Variable without Initializer
PDA_HEADER(Param)			(bool forParameter)						{	return {	nullptr,	{},			nullptr,	nullptr,	forParameter,	DeclaratorRestriction::Optional,	InitializerRestriction::Optional,	false,			forParameter,	false,		true,	false,	false	}; } // Parameter
PDA_HEADER(TemplateParam)	()										{	return {	nullptr,	{},			nullptr,	nullptr,	false,			DeclaratorRestriction::Optional,	InitializerRestriction::Optional,	false,			true,			false,		false,	false,	false	}; } // Parameter
PDA_HEADER(Decls)			(bool allowBitField, bool allowComma)	{	return {	nullptr,	{},			nullptr,	nullptr,	false,			DeclaratorRestriction::Many,		InitializerRestriction::Optional,	allowBitField,	false,			allowComma,	true,	true,	true	}; } // Declarations
PDA_HEADER(ForceFP)			()										{	return {	nullptr,	{},			nullptr,	nullptr,	false,			DeclaratorRestriction::One,			InitializerRestriction::Zero,		false,			false,			false,		true,	true,	false	}; } // Declarations
PDA_HEADER(Typedefs)		()										{	return {	nullptr,	{},			nullptr,	nullptr,	false,			DeclaratorRestriction::Many,		InitializerRestriction::Zero,		false,			false,			false,		false,	false,	false	}; } // Declarations after typedef keyword
#undef PDA_HEADER

namespace symbol_component
{
	struct ClassMemberCache
	{
		/*
		for void A::B::C::F(){}, it will be [C, B, A]
		*/
		List<ITsys*>				containerClassTypes;

		/*
		template headers for each member in containerClassTypes
		any item could be null when the mapped class type is not a template class
		*/
		List<Ptr<TemplateSpec>>		containerClassSpecs;

		/*
		the single remaining template header that is unused, could be null for non-template member
		*/
		Ptr<TemplateSpec>			declSpec;

		/*
		true for members defined inside a class, so that type arguments reachable in classSymbols are reachable here
		false for outside
		*/
		bool						symbolDefinedInsideClass;

		/*
		The scope to jump to after all symbols in classSymbols are scanned
		for members defined inside a class, this is the parent scope of the most outside class
		for members defined outside a class, this is the scope where the member is defined
		*/
		Symbol*						parentScope;

		// applicable where this cache is in a function
		ITsys*						thisType = nullptr;		// Parser_Declaration_FuncVar_Func.cpp
		Symbol*						funcSymbol = nullptr;	// Symbol.cpp
		Ptr<FunctionDeclaration>	funcDecl;				// Symbol.cpp
	};
}

extern bool											IsInTemplateHeader(const ParsingArguments& pa);
extern Ptr<symbol_component::ClassMemberCache>		CreatePartialClassMemberCache(const ParsingArguments& pa, Symbol* classSymbol, Ptr<CppTokenCursor>& cursor);
extern Ptr<symbol_component::ClassMemberCache>		CreatePartialClassMemberCache(const ParsingArguments& pa, Ptr<Type> classType, List<Ptr<TemplateSpec>>* specs, Ptr<CppTokenCursor>& cursor);

extern Ptr<Type>									ParseTypeBeforeDeclarator(const ParsingArguments& pa, Ptr<Type> baselineType, Ptr<CppTokenCursor>& cursor);
extern void											ParseMemberDeclarator(const ParsingArguments& pa, const ParsingDeclaratorArguments& pda, Ptr<CppTokenCursor>& cursor, List<Ptr<Declarator>>& declarators);
extern void											ParseNonMemberDeclarator(const ParsingArguments& pa, const ParsingDeclaratorArguments& pda, Ptr<Type> type, Ptr<CppTokenCursor>& cursor, List<Ptr<Declarator>>& declarators);
extern void											ParseNonMemberDeclarator(const ParsingArguments& pa, const ParsingDeclaratorArguments& pda, Ptr<CppTokenCursor>& cursor, List<Ptr<Declarator>>& declarators);
extern Ptr<Declarator>								ParseNonMemberDeclarator(const ParsingArguments& pa, const ParsingDeclaratorArguments& pda, Ptr<CppTokenCursor>& cursor);

extern Ptr<Type>									ParseType(const ParsingArguments& pa, Ptr<CppTokenCursor>& cursor);
extern Ptr<FunctionType>							ParseFunctionType(const ParsingArguments& pa, Ptr<Type> returnType, Ptr<CppTokenCursor>& cursor);

#endif