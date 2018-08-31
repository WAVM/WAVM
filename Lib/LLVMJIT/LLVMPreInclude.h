#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4267)
#pragma warning(disable : 4800)
#pragma warning(disable : 4291)
#pragma warning(disable : 4244)
#pragma warning(disable : 4351)
#pragma warning(disable : 4065)
#pragma warning(disable : 4624)

// conversion from 'int' to 'unsigned int', signed/unsigned mismatch
#pragma warning(disable : 4245)

// unary minus operator applied to unsigned type, result is still unsigned
#pragma warning(disable : 4146)

// declaration of 'x' hides class member
#pragma warning(disable : 4458)

// default constructor could not be generated
#pragma warning(disable : 4510)

// struct can never be instantiated - user defined constructor required
#pragma warning(disable : 4610)

// structure was padded due to alignment specifier
#pragma warning(disable : 4324)

// unreachable code
#pragma warning(disable : 4702)
#endif
