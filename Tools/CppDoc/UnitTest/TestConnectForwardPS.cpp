#include "Util.h"

TEST_FILE
{
	TEST_CATEGORY(L"Functions")
	{
		auto input = LR"(
template<typename T, typename U, typename... Ts>	void F										(T*, U(*...)(Ts&));
template<typename T, typename U, typename... Ts>	void F										(T, U(*...)(Ts));
template<typename T, typename U, typename... Ts>	void G										(T*, U(*...)(Ts&));
template<typename T, typename U, typename... Ts>	void G										(T, U(*...)(Ts));

template<>											void F<bool, bool, float, double>			(bool*, bool(*)(float&), bool(*)(double&));
template<>											void F<void, void, char, wchar_t, bool>		(void*, void(*)(char&), void(*)(wchar_t&), void(*)(bool&));
template<>											void F<char, char>							(char*);

template<>											void F<bool*, bool, float&, double&>		(bool*, bool(*)(float&), bool(*)(double&));
template<>											void F<void*, void, char&, wchar_t&, bool&>	(void*, void(*)(char&), void(*)(wchar_t&), void(*)(bool&));
template<>											void F<char*, char>							(char*);

template<>											void G<bool, bool, float, double>			(bool*, bool(*)(float&), bool(*)(double&));
template<>											void G<void, void, char, wchar_t, bool>		(void*, void(*)(char&), void(*)(wchar_t&), void(*)(bool&));
template<>											void G<char, char>							(char*);

template<>											void G<bool*, bool, float&, double&>		(bool*, bool(*)(float&), bool(*)(double&));
template<>											void G<void*, void, char&, wchar_t&, bool&>	(void*, void(*)(char&), void(*)(wchar_t&), void(*)(bool&));
template<>											void G<char*, char>							(char*);

template<typename U, typename V, typename... Us>	void F										(U*, V(*...)(Us&)){}
template<typename U, typename V, typename... Us>	void F										(U, V(*...)(Us)){}
template<typename U, typename V, typename... Us>	void G										(U*, V(*...)(Us&)){}
template<typename U, typename V, typename... Us>	void G										(U, V(*...)(Us)){}

template<>											void F<bool, bool, float, double>			(bool*, bool(*)(float&), bool(*)(double&)){}
template<>											void F<void, void, char, wchar_t, bool>		(void*, void(*)(char&), void(*)(wchar_t&), void(*)(bool&)){}
template<>											void F<char, char>							(char*){}

template<>											void F<bool*, bool, float&, double&>		(bool*, bool(*)(float&), bool(*)(double&)){}
template<>											void F<void*, void, char&, wchar_t&, bool&>	(void*, void(*)(char&), void(*)(wchar_t&), void(*)(bool&)){}
template<>											void F<char*, char>							(char*){}

template<>											void G<bool, bool, float, double>			(bool*, bool(*)(float&), bool(*)(double&)){}
template<>											void G<void, void, char, wchar_t, bool>		(void*, void(*)(char&), void(*)(wchar_t&), void(*)(bool&)){}
template<>											void G<char, char>							(char*){}

template<>											void G<bool*, bool, float&, double&>		(bool*, bool(*)(float&), bool(*)(double&)){}
template<>											void G<void*, void, char&, wchar_t&, bool&>	(void*, void(*)(char&), void(*)(wchar_t&), void(*)(bool&)){}
template<>											void G<char*, char>							(char*){}
)";

		COMPILE_PROGRAM(program, pa, input);

		List<Ptr<ForwardFunctionDeclaration>> ffs[2];
		List<Ptr<FunctionDeclaration>> fs[2];
		for (vint i = 0; i < program->decls.Count(); i++)
		{
			auto decl = program->decls[i];
			if (auto fdecl = decl.Cast<FunctionDeclaration>())
			{
				if (fdecl->name.name == L"F") fs[0].Add(fdecl);
				if (fdecl->name.name == L"G") fs[1].Add(fdecl);
			}
			else if (auto ffdecl = decl.Cast<ForwardFunctionDeclaration>())
			{
				if (ffdecl->name.name == L"F") ffs[0].Add(ffdecl);
				if (ffdecl->name.name == L"G") ffs[1].Add(ffdecl);
			}
		}

		TEST_CASE_ASSERT(ffs[0].Count() == 8);
		TEST_CASE_ASSERT(ffs[1].Count() == 8);
		TEST_CASE_ASSERT(fs[0].Count() == 8);
		TEST_CASE_ASSERT(fs[1].Count() == 8);
		for (vint i = 0; i < 2; i++)
		{
			TEST_CATEGORY(WString(L"Test function: ") + (i == 0 ? L"F" : L"G"))
			{
				TEST_CATEGORY(L"Check connections")
				{
					for (vint j = 0; j < 8; j++)
					{
						auto symbol = ffs[i][j]->symbol->GetFunctionSymbol_Fb();
						TEST_CASE_ASSERT(symbol == fs[i][j]->symbol->GetFunctionSymbol_Fb());
					}
				});

				for (vint j = 0; j < 2; j++)
				{
					TEST_CATEGORY(L"Check partial specialization relationship for group: " + itow(j))
					{
						auto primary = ffs[i][j]->symbol->GetFunctionSymbol_Fb();
						for (vint k = 0; k < 4; k++)
						{
							if (k == 0)
							{
								TEST_CASE_ASSERT(primary->IsPSPrimary_NF());
								TEST_CASE_ASSERT(primary->GetPSPrimaryDescendants_NF().Count() == 3);
								TEST_CASE_ASSERT(primary->GetPSChildren_NF().Count() == 3);
							}
							else
							{
								auto symbol = ffs[i][1 + j * 3 + k]->symbol->GetFunctionSymbol_Fb();
								TEST_CASE_ASSERT(symbol->GetPSPrimary_NF() == primary);
								TEST_CASE_ASSERT(symbol->GetPSParents_NF().Count() == 1);
								TEST_CASE_ASSERT(symbol->GetPSParents_NF()[0] == primary);
							}
						}
					});
				}
			});
		}
	});

	TEST_CATEGORY(L"Members")
	{
		auto input = LR"(
namespace ns
{
	struct A
	{
		template<int X, typename T>
		struct B
		{
			struct C
			{
				template<typename U, int Y>
				struct D
				{
					static const bool field;
					void Method(T, U);
					template<typename V> void Method(T, U*, V, V*);
					template<typename V> void Method(T*, U, V*, V);
				};

				template<int Y, typename U1, typename U2, typename U3>
				struct D<U1(*)(U2, U3), Y>
				{
					static const bool field;
					void Method(T, U1);
					template<typename V> void Method(T, U2*, V, V*);
					template<typename V> void Method(T*, U3, V*, V);
				};
			};
		};

		template<typename T1, typename T2, typename T3, int X>
		struct B<X, T1(*)(T2, T3)>
		{
			struct C
			{
				template<typename U, int Y>
				struct D
				{
					static const bool field;
					void Method(T1, U);
					template<typename V> void Method(T2, U*, V, V*);
					template<typename V> void Method(T3*, U, V*, V);
				};

				template<int Y, typename U1, typename U2, typename U3>
				struct D<U1(*)(U2, U3), Y>
				{
					static const bool field;
					void Method(T1, U1);
					template<typename V> void Method(T2, U2*, V, V*);
					template<typename V> void Method(T3*, U3, V*, V);
				};
			};
		};
	};
}

namespace ns
{
	template<int _1, typename X>								template<typename Y, int _2>														bool A::B<_1, X>::C::D<Y, _2>::field = false;
	template<int _1, typename X>								template<typename Y, int _2>														void A::B<_1, X>::C::D<Y, _2>::Method(X, Y){}
	template<int _1, typename X>								template<typename Y, int _2>								template<typename Z>	void A::B<_1, X>::C::D<Y, _2>::Method(X, Y*, Z, Z*){}
	template<int _1, typename X>								template<typename Y, int _2>								template<typename Z>	void A::B<_1, X>::C::D<Y, _2>::Method(X*, Y, Z*, Z){}

	template<int _1, typename X>								template<int _2, typename Y1, typename Y2, typename Y3>								bool A::B<_1, X>::C::D<Y1(*)(Y2, Y3), _2>::field = false;
	template<int _1, typename X>								template<int _2, typename Y1, typename Y2, typename Y3>								void A::B<_1, X>::C::D<Y1(*)(Y2, Y3), _2>::Method(X, Y){}
	template<int _1, typename X>								template<int _2, typename Y1, typename Y2, typename Y3>		template<typename Z>	void A::B<_1, X>::C::D<Y1(*)(Y2, Y3), _2>::Method(X, Y*, Z, Z*){}
	template<int _1, typename X>								template<int _2, typename Y1, typename Y2, typename Y3>		template<typename Z>	void A::B<_1, X>::C::D<Y1(*)(Y2, Y3), _2>::Method(X*, Y, Z*, Z){}

	template<typename X1, typename X2, typename X3, int _1>		template<typename Y, int _2>														bool A::B<_1, X1(*)(X2, X3)>::C::D<Y, _2>::field = false;
	template<typename X1, typename X2, typename X3, int _1>		template<typename Y, int _2>														void A::B<_1, X1(*)(X2, X3)>::C::D<Y, _2>::Method(X1, Y){}
	template<typename X1, typename X2, typename X3, int _1>		template<typename Y, int _2>								template<typename Z>	void A::B<_1, X1(*)(X2, X3)>::C::D<Y, _2>::Method(X2, Y*, Z, Z*){}
	template<typename X1, typename X2, typename X3, int _1>		template<typename Y, int _2>								template<typename Z>	void A::B<_1, X1(*)(X2, X3)>::C::D<Y, _2>::Method(X3*, Y, Z*, Z){}

	template<typename X1, typename X2, typename X3, int _1>		template<int _2, typename Y1, typename Y2, typename Y3>								bool A::B<_1, X1(*)(X2, X3)>::C::D<Y1(*)(Y2, Y3), _2>::field = false;
	template<typename X1, typename X2, typename X3, int _1>		template<int _2, typename Y1, typename Y2, typename Y3>								void A::B<_1, X1(*)(X2, X3)>::C::D<Y1(*)(Y2, Y3), _2>::Method(X1, Y){}
	template<typename X1, typename X2, typename X3, int _1>		template<int _2, typename Y1, typename Y2, typename Y3>		template<typename Z>	void A::B<_1, X1(*)(X2, X3)>::C::D<Y1(*)(Y2, Y3), _2>::Method(X2, Y*, Z, Z*){}
	template<typename X1, typename X2, typename X3, int _1>		template<int _2, typename Y1, typename Y2, typename Y3>		template<typename Z>	void A::B<_1, X1(*)(X2, X3)>::C::D<Y1(*)(Y2, Y3), _2>::Method(X3*, Y, Z*, Z){}
}
)";
		COMPILE_PROGRAM(program, pa, input);

		TEST_CATEGORY(L"Checking connections")
		{
			using Item = Tuple<CppClassAccessor, Ptr<Declaration>>;
			List<Ptr<Declaration>> inClassMembers;

			auto& inClassMembersUnfiltered1 = pa.root
				->TryGetChildren_NFb(L"ns")->Get(0)
				->TryGetChildren_NFb(L"A")->Get(0)
				->TryGetChildren_NFb(L"B")->Get(0)
				->TryGetChildren_NFb(L"C")->Get(0)
				->TryGetChildren_NFb(L"D")->Get(0)
				->GetImplDecl_NFb<ClassDeclaration>()->decls;

			auto& inClassMembersUnfiltered2 = pa.root
				->TryGetChildren_NFb(L"ns")->Get(0)
				->TryGetChildren_NFb(L"A")->Get(0)
				->TryGetChildren_NFb(L"B")->Get(0)
				->TryGetChildren_NFb(L"C")->Get(0)
				->TryGetChildren_NFb(L"D@<[U1]([U2], [U3]) *, *>")->Get(0)
				->GetImplDecl_NFb<ClassDeclaration>()->decls;

			auto& inClassMembersUnfiltered3 = pa.root
				->TryGetChildren_NFb(L"ns")->Get(0)
				->TryGetChildren_NFb(L"A")->Get(0)
				->TryGetChildren_NFb(L"B")->Get(0)
				->TryGetChildren_NFb(L"C")->Get(0)
				->TryGetChildren_NFb(L"D")->Get(0)
				->GetImplDecl_NFb<ClassDeclaration>()->decls;

			auto& inClassMembersUnfiltered4 = pa.root
				->TryGetChildren_NFb(L"ns")->Get(0)
				->TryGetChildren_NFb(L"A")->Get(0)
				->TryGetChildren_NFb(L"B@<*, [T1]([T2], [T3]) *>")->Get(0)
				->TryGetChildren_NFb(L"C")->Get(0)
				->TryGetChildren_NFb(L"D@<[U1]([U2], [U3]) *, *>")->Get(0)
				->GetImplDecl_NFb<ClassDeclaration>()->decls;

#define FILTER_CONDITION .Where([](Item item) {return !item.f1->implicitlyGeneratedMember; }).Select([](Item item) { return item.f1; })
			CopyFrom(inClassMembers, From(inClassMembersUnfiltered1) FILTER_CONDITION, true);
			CopyFrom(inClassMembers, From(inClassMembersUnfiltered2) FILTER_CONDITION, true);
			CopyFrom(inClassMembers, From(inClassMembersUnfiltered3) FILTER_CONDITION, true);
			CopyFrom(inClassMembers, From(inClassMembersUnfiltered4) FILTER_CONDITION, true);
#undef FILTER_CONDITION
			TEST_CASE_ASSERT(inClassMembers.Count() == 16);

			auto& outClassMembers = pa.root
				->TryGetChildren_NFb(L"ns")->Get(0)
				->GetForwardDecls_N()[1].Cast<NamespaceDeclaration>()->decls;
			TEST_CASE_ASSERT(outClassMembers.Count() == 16);

			for (vint c = 0; c < 4; c++)
			{
				TEST_CASE(L"Category " + itow(c))
				{
					for (vint m = 0; m < 4; m++)
					{
						vint i = c * 4 + m;
						auto inClassDecl = inClassMembers[i];
						auto outClassDecl = outClassMembers[i];

						if (i == 0)
						{
							auto symbol = inClassDecl->symbol;
							TEST_ASSERT(symbol->kind == symbol_component::SymbolKind::Variable);

							TEST_ASSERT(symbol->GetImplDecl_NFb() == outClassDecl);

							TEST_ASSERT(symbol->GetForwardDecls_N().Count() == 1);
							TEST_ASSERT(symbol->GetForwardDecls_N()[0] == inClassDecl);
						}
						else
						{
							auto symbol = inClassDecl->symbol->GetFunctionSymbol_Fb();
							TEST_ASSERT(symbol->kind == symbol_component::SymbolKind::FunctionSymbol);

							TEST_ASSERT(symbol->GetImplSymbols_F().Count() == 1);
							TEST_ASSERT(symbol->GetImplSymbols_F()[0]->GetImplDecl_NFb() == outClassDecl);

							TEST_ASSERT(symbol->GetForwardSymbols_F().Count() == 1);
							TEST_ASSERT(symbol->GetForwardSymbols_F()[0]->GetForwardDecl_Fb() == inClassDecl);
						}
					}
				});
			}
		});
	});

	TEST_CATEGORY(L"Methods")
	{
		// TODO:
	});
}