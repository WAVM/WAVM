#include "Core/Core.h"
#include "Core/Platform.h"
#include "Intrinsics.h"
#include "RuntimePrivate.h"

#include <limits.h>
#include <iostream>

namespace Runtime
{
	DEFINE_INTRINSIC_FUNCTION1(spectest,spectest_print,print,unit,i32,a)
	{
		std::cout << a << " : i32" << std::endl;
	}

	DEFINE_INTRINSIC_FUNCTION1(spectest,spectest_print,print,unit,i64,a)
	{
		std::cout << a << " : i64" << std::endl;
	}

	DEFINE_INTRINSIC_FUNCTION1(spectest,spectest_print,print,unit,f32,a)
	{
		std::cout << a << " : f32" << std::endl;
	}
	
	DEFINE_INTRINSIC_FUNCTION1(spectest,spectest_print,print,unit,f64,a)
	{
		std::cout << a << " : f64" << std::endl;
	}

	DEFINE_INTRINSIC_FUNCTION2(spectest,spectest_print,print,unit,i32,a,f32,b)
	{
		std::cout << a << " : i32, " << b << " : f32" << std::endl;
	}

	DEFINE_INTRINSIC_FUNCTION2(spectest,spectest_print,print,unit,i64,a,f64,b)
	{
		std::cout << a << " : i64, " << b << " : f64" << std::endl;
	}

	DEFINE_INTRINSIC_VARIABLE(spectest,spectest_globalI32,global,i32,false,666)
	DEFINE_INTRINSIC_VARIABLE(spectest,spectest_globalI64,global,i64,false,0)
	DEFINE_INTRINSIC_VARIABLE(spectest,spectest_globalF32,global,f32,false,0.0f)
	DEFINE_INTRINSIC_VARIABLE(spectest,spectest_globalF64,global,f64,false,0.0)

	DEFINE_INTRINSIC_TABLE(spectest,spectest_table,table,TableType(TableElementType::anyfunc,SizeConstraints {10,20}))
	DEFINE_INTRINSIC_MEMORY(spectest,spectest_memory,memory,MemoryType(SizeConstraints {1,2}))

	void initSpecTestIntrinsics()
	{
	}
}
