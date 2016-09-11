#include "Core/Core.h"
#include "Core/Serialization.h"
#include "WebAssembly.h"
#include "Module.h"
#include "Operations.h"

namespace WebAssembly
{
	using namespace Serialization;
	
	enum
	{
		magicNumber=0x6d736100, // "\0asm"
		currentVersion=0xc
	};

	enum class SectionType : uint8
	{
		type = 1,
		import = 2,
		functionDeclarations = 3,
		table = 4,
		memory = 5,
		global = 6,
		export_ = 7,
		start = 8,
		functionDefinitions = 9,
		elem = 10,
		data = 11,
		name = 0x80,
	};

	template<typename Stream>
	void serialize(Stream& stream,ValueType& type)
	{
		serializeNativeValue(stream,type);
	}

	void serialize(InputStream& stream,ResultType& returnType)
	{
		uintptr arity;
		serializeVarUInt1(stream,arity);
		if(arity == 0) { returnType = ResultType::unit; }
		else { serializeNativeValue(stream,returnType); }
	}
	void serialize(OutputStream& stream,ResultType& returnType)
	{
		uintptr arity = returnType == ResultType::unit ? 0 : 1;
		serializeVarUInt1(stream,arity);
		if(arity) { serializeNativeValue(stream,returnType); }
	}
	
	template<typename Stream>
	void serialize(Stream& stream,SizeConstraints& sizeConstraints)
	{
		uintptr flags = sizeConstraints.max != UINT64_MAX ? 1 : 0;
		serializeVarUInt32(stream,flags);
		serializeVarUInt32(stream,sizeConstraints.min);
		if(flags) { serializeVarUInt32(stream,sizeConstraints.max); }
		else if(Stream::isInput) { sizeConstraints.max = UINT64_MAX; }
	}
	
	template<typename Stream>
	void serialize(Stream& stream,TableElementType& elementType)
	{
		serializeNativeValue(stream,elementType);
	}

	template<typename Stream>
	void serialize(Stream& stream,TableType& tableType)
	{
		serialize(stream,tableType.elementType);
		serialize(stream,tableType.size);
	}

	template<typename Stream>
	void serialize(Stream& stream,MemoryType& memoryType)
	{
		serialize(stream,memoryType.size);
	}

	template<typename Stream>
	void serialize(Stream& stream,GlobalType& globalType)
	{
		serialize(stream,globalType.valueType);
		uint8 isMutable = globalType.isMutable ? 1 : 0;
		serializeVarUInt1(stream,isMutable);
		if(Stream::isInput) { globalType.isMutable = isMutable != 0; }
	}
	
	template<typename Stream>
	void serialize(Stream& stream,ObjectKind& kind)
	{
		serializeNativeValue(stream,*(uint8*)&kind);
	}

	template<typename Stream>
	void serialize(Stream& stream,Import& import)
	{
		serialize(stream,import.type.kind);
		switch(import.type.kind)
		{
		case ObjectKind::function: serializeVarUInt32(stream,import.type.functionTypeIndex); break;
		case ObjectKind::table: serialize(stream,import.type.table); break;
		case ObjectKind::memory: serialize(stream,import.type.memory); break;
		case ObjectKind::global: serialize(stream,import.type.global); break;
		default: throw FatalSerializationException("invalid ObjectKind");
		}
		serialize(stream,import.module);
		serialize(stream,import.exportName);
	}
		
	template<typename Stream>
	void serialize(Stream& stream,Export& e)
	{
		serializeVarUInt32(stream,e.index);
		serialize(stream,e.kind);
		serialize(stream,e.name);
	}

	template<typename Stream>
	void serialize(Stream& stream,InitializerExpression& initializer)
	{
		serializeNativeValue(stream,*(uint8*)&initializer.type);
		switch(initializer.type)
		{
		case InitializerExpression::Type::i32_const: serialize(stream,initializer.i32); break;
		case InitializerExpression::Type::i64_const: serialize(stream,initializer.i64); break;
		case InitializerExpression::Type::f32_const: serialize(stream,initializer.f32); break;
		case InitializerExpression::Type::f64_const: serialize(stream,initializer.f64); break;
		case InitializerExpression::Type::get_global: serializeVarUInt32(stream,initializer.globalIndex); break;
		default: throw FatalSerializationException("invalid initializer expression opcode");
		}
		serializeConstant(stream,"expected end opcode",(uint8)Opcode::end);
	}

	template<typename Stream>
	void serialize(Stream& stream,Global& global)
	{
		serialize(stream,global.type);
		serialize(stream,global.initializer);
	}

	template<typename Stream>
	void serialize(Stream& stream,DataSegment& dataSegment)
	{
		serializeConstant<uint8>(stream,"invalid memory index",0);
		serialize(stream,dataSegment.baseOffset);
		serialize(stream,dataSegment.data);
	}

	template<typename Stream>
	void serialize(Stream& stream,TableSegment& tableSegment)
	{
		serializeConstant<uint8>(stream,"invalid table index",0);
		serialize(stream,tableSegment.baseOffset);
		serializeArray(stream,tableSegment.indices,[](Stream& stream,uintptr& functionIndex){serializeVarUInt32(stream,functionIndex);});
	}

	template<typename Stream>
	void serialize(Stream& stream,FunctionDisassemblyInfo& functionDisassemblyInfo)
	{
		serialize(stream,functionDisassemblyInfo.name);
		serialize(stream,functionDisassemblyInfo.localNames);
	}
		
	template<typename Stream>
	void serialize(Stream& stream,ModuleDisassemblyInfo& moduleDisassemblyInfo)
	{
		serialize(stream,moduleDisassemblyInfo.functions);
	}

	template<typename SerializeSection>
	void serializeSection(OutputStream& stream,SectionType type,bool mustSerialize,SerializeSection serializeSection)
	{
		if(mustSerialize)
		{
			serializeNativeValue(stream,type);
			ArrayOutputStream sectionStream;
			serializeSection(sectionStream);
			std::vector<uint8> sectionBytes = sectionStream.getBytes();
			size_t sectionNumBytes = sectionBytes.size();
			serializeVarUInt32(stream,sectionNumBytes);
			stream.serializeBytes(sectionBytes.data(),sectionBytes.size());
		}
	}
	template<typename SerializeSection>
	void serializeSection(InputStream& stream,SectionType expectedType,bool,SerializeSection serializeSection)
	{
		InputStream savedStream = stream;
		if(stream.capacity())
		{
			SectionType type;
			serializeNativeValue(stream,type);
			if(type == expectedType)
			{
				size_t numBytes = 0;
				serializeVarUInt32(stream,numBytes);
				InputStream sectionStream = stream.makeSubstream(numBytes);
				serializeSection(sectionStream);
			}
			else { stream = savedStream; }
		}
	}
	
	struct LocalSet
	{
		uintptr num;
		ValueType type;
	};
	
	template<typename Stream>
	void serialize(Stream& stream,LocalSet& localSet)
	{
		serializeVarUInt32(stream,localSet.num);
		serialize(stream,localSet.type);
	}

	void serializeFunctionBody(OutputStream& sectionStream,Module& module,Function& function)
	{
		ArrayOutputStream bodyStream;

		// Convert the function's local types into LocalSets: runs of locals of the same type.
		LocalSet* localSets = (LocalSet*)alloca(sizeof(LocalSet)*function.nonParameterLocalTypes.size());
		uintptr numLocalSets = 0;
		if(function.nonParameterLocalTypes.size())
		{
			localSets[0].type = ValueType::invalid;
			localSets[0].num = 0;
			for(auto localType : function.nonParameterLocalTypes)
			{
				if(localSets[numLocalSets].type != localType)
				{
					if(localSets[numLocalSets].type != ValueType::invalid) { ++numLocalSets; }
					localSets[numLocalSets].type = localType;
					localSets[numLocalSets].num = 0;
				}
				++localSets[numLocalSets].num;
			}
			if(localSets[numLocalSets].type != ValueType::invalid) { ++numLocalSets; }
		}

		// Serialize the local sets.
		serializeVarUInt32(bodyStream,numLocalSets);
		for(uintptr setIndex = 0;setIndex < numLocalSets;++setIndex) { serialize(bodyStream,localSets[setIndex]); }

		bodyStream.serializeBytes(module.code.data() + function.code.offset,function.code.numBytes);

		std::vector<uint8> bodyBytes = bodyStream.getBytes();
		serialize(sectionStream,bodyBytes);
	}
	
	void serializeFunctionBody(InputStream& sectionStream,Module& module,Function& function)
	{
		size_t numBodyBytes = 0;
		serializeVarUInt32(sectionStream,numBodyBytes);
		const size_t initialCapacity = sectionStream.capacity();
		
		// Deserialize local sets and unpack them into a linear array of local types.
		size_t numLocalSets = 0;
		serializeVarUInt32(sectionStream,numLocalSets);
		for(uintptr setIndex = 0;setIndex < numLocalSets;++setIndex)
		{
			LocalSet localSet;
			serialize(sectionStream,localSet);
			for(uintptr index = 0;index < localSet.num;++index) { function.nonParameterLocalTypes.push_back(localSet.type); }
		}

		const size_t numLocalBytes = initialCapacity - sectionStream.capacity();

		function.code = {module.code.size(),numBodyBytes - numLocalBytes};
		module.code.resize(function.code.offset + function.code.numBytes);
		sectionStream.serializeBytes(module.code.data() + function.code.offset,function.code.numBytes);
	}

	template<typename Stream>
	void serializeModule(Stream& moduleStream,Module& module)
	{
		serializeConstant(moduleStream,"magic number",uint32(magicNumber));
		serializeConstant(moduleStream,"version",uint32(currentVersion));

		serializeSection(moduleStream,SectionType::type,module.types.size()>0,[&module](Stream& sectionStream)
		{
			serializeArray(sectionStream,module.types,[](Stream& stream,const FunctionType*& functionType)
			{
				serializeConstant(stream,"function type tag",uint8(0x40));
				if(Stream::isInput)
				{
					std::vector<ValueType> parameterTypes;
					ResultType returnType;
					serialize(stream,parameterTypes);
					serialize(stream,returnType);
					functionType = FunctionType::get(returnType,parameterTypes);
				}
				else
				{
					serialize(stream,const_cast<std::vector<ValueType>&>(functionType->parameters));
					serialize(stream,const_cast<ResultType&>(functionType->ret));
				}
			});
		});
		serializeSection(moduleStream,SectionType::import,module.imports.size()>0,[&module](Stream& sectionStream)
		{
			serialize(sectionStream,module.imports);
		});
		serializeSection(moduleStream,SectionType::functionDeclarations,module.functionDefs.size()>0,[&module](Stream& sectionStream)
		{
			size_t numFunctions = module.functionDefs.size();
			serializeVarUInt32(sectionStream,numFunctions);
			if(Stream::isInput) { module.functionDefs.resize(numFunctions); }
			for(Function& function : module.functionDefs) { serializeVarUInt32(sectionStream,function.typeIndex); }
		});
		serializeSection(moduleStream,SectionType::table,module.tableDefs.size()>0,[&module](Stream& sectionStream)
		{
			serializeArray(sectionStream,module.tableDefs,[](Stream& stream,TableType& tableType)
			{
				serialize(stream,tableType.elementType);
				serialize(stream,tableType.size);
			});
		});
		serializeSection(moduleStream,SectionType::memory,module.memoryDefs.size()>0,[&module](Stream& sectionStream)
		{
			if(Stream::isInput) { module.memoryDefs.resize(1); }
			serialize(sectionStream,module.memoryDefs[0].size);
		});
		serializeSection(moduleStream,SectionType::global,module.globalDefs.size()>0,[&module](Stream& sectionStream)
		{
			serialize(sectionStream,module.globalDefs);
		});
		serializeSection(moduleStream,SectionType::export_,module.exports.size()>0,[&module](Stream& sectionStream)
		{
			serialize(sectionStream,module.exports);
		});
		serializeSection(moduleStream,SectionType::start,module.startFunctionIndex!=UINTPTR_MAX,[&module](Stream& sectionStream)
		{
			serializeVarUInt32(sectionStream,module.startFunctionIndex);
		});
		serializeSection(moduleStream,SectionType::functionDefinitions,module.functionDefs.size()>0,[&module](Stream& sectionStream)
		{
			size_t numFunctionBodies = module.functionDefs.size();
			serializeVarUInt32(sectionStream,numFunctionBodies);
			if(Stream::isInput && numFunctionBodies != module.functionDefs.size())
				{ throw FatalSerializationException("function and code sections have mismatched function counts"); }
			for(Function& function : module.functionDefs) { serializeFunctionBody(sectionStream,module,function); }
		});
		serializeSection(moduleStream,SectionType::elem,module.tableSegments.size()>0,[&module](Stream& sectionStream)
		{
			serialize(sectionStream,module.tableSegments);
		});
		serializeSection(moduleStream,SectionType::data,module.dataSegments.size()>0,[&module](Stream& sectionStream)
		{
			serialize(sectionStream,module.dataSegments);
		});
		serializeSection(moduleStream,SectionType::name,module.disassemblyInfo.functions.size()>0,[&module](Stream& sectionStream)
		{
			serialize(sectionStream,module.disassemblyInfo);
		});
	}

	void serialize(Serialization::InputStream& stream,Module& module)
	{
		serializeModule(stream,module);
		validate(module);
	}
	void serialize(Serialization::OutputStream& stream,const Module& module)
	{
		serializeModule(stream,const_cast<Module&>(module));
	}
}
