#include "Util.h"

namespace INPUT__TestParsePSFunction_Functions
{
	TEST_DECL(
		void R();

		template<typename T>
		T R(T);

		template<typename T, typename U, typename... Ts>
		auto R(T, U u, Ts... ts) { return R(u, ts...); }

		template<typename T, typename U, typename... Ts>
		auto F(T, U, Ts*...) -> decltype(R(Ts()...));

		template<>
		void F<char, wchar_t>(char, wchar_t);

		template<>
		float F<char, wchar_t, float>(char, wchar_t, float*);

		template<>
		double F<char, wchar_t, float, double>(char, wchar_t, float*, double*);

		template<>
		bool F<char, wchar_t, float, double, bool>(char, wchar_t, float*, double*, bool*);

		float* pf = nullptr;
		double* pd = nullptr;
		bool* pb = nullptr;
	);
}

TEST_FILE
{
	TEST_CATEGORY(L"Partial specialization relationship")
	{
		using namespace INPUT__TestParsePSFunction_Functions;
		COMPILE_PROGRAM(program, pa, input);

		Symbol* primary = nullptr;
		for (vint i = 0; i < program->decls.Count(); i++)
		{
			if(auto decl=program->decls[i].Cast<ForwardFunctionDeclaration>())
			{
				if (decl->name.name == L"F")
				{
					if (primary)
					{
						TEST_CASE_ASSERT(decl->symbol->GetPSPrimary_NF() == primary);
					}
					else
					{
						auto primary = decl->symbol;
						TEST_CASE_ASSERT(primary->IsPSPrimary_NF());
						TEST_CASE_ASSERT(primary->GetPSPrimaryDescendants_NF().Count() == 4);
					}
				}
			}
		}
	});

	TEST_CATEGORY(L"Instantiate template functions")
	{
		using namespace INPUT__TestParsePSFunction_Functions;
		COMPILE_PROGRAM(program, pa, input);

		ASSERT_OVERLOADING_VERBOSE(
			(&F<char, wchar_t>),
			L"((& F<char, wchar_t>))",
			L"void __cdecl(char, wchar_t) *",
			void(*)(char, wchar_t)
		);

		ASSERT_OVERLOADING_VERBOSE(
			(&F<char, wchar_t, float>),
			L"((& F<char, wchar_t, float>))",
			L"float __cdecl(char, wchar_t, float *) *",
			float(*)(char, wchar_t, float*)
		);

		ASSERT_OVERLOADING_VERBOSE(
			(&F<char, wchar_t, float, double>),
			L"((& F<char, wchar_t, float, double>))",
			L"double __cdecl(char, wchar_t, float *, double *) *",
			double(*)(char, wchar_t, float*, double*)
		);

		ASSERT_OVERLOADING_VERBOSE(
			(&F<char, wchar_t, float, double, bool>),
			L"((& F<char, wchar_t, float, double, bool>))",
			L"bool __cdecl(char, wchar_t, float *, double *, bool *) *",
			bool(*)(char, wchar_t, float*, double*, bool*)
		);
	});

	TEST_CATEGORY(L"Calling template functions")
	{
		using namespace INPUT__TestParsePSFunction_Functions;
		COMPILE_PROGRAM(program, pa, input);

		ASSERT_OVERLOADING_SIMPLE(F('c', L'c'),				void	);
		ASSERT_OVERLOADING_SIMPLE(F('c', L'c', pf),			float	);
		ASSERT_OVERLOADING_SIMPLE(F('c', L'c', pf, pd),		double	);
		ASSERT_OVERLOADING_SIMPLE(F('c', L'c', pf, pd, pb),	bool	);
	});
}