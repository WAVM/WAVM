#include "Core/Core.h"
#include "Inline/Serialization.h"
#include "WASM.h"
#include "IR/Module.h"
#include "IR/Operators.h"
#include "IR/Types.h"
#include "IR/Validate.h"

namespace IR
{
	template<typename Stream>
	void serialize(Stream& stream,ValueType& type)
	{
		int8 encodedValueType = -(int8)type;
		serializeVarInt7(stream,encodedValueType);
		if(Stream::isInput) { type = (ValueType)-encodedValueType; }
	}

	static void serialize(InputStream& stream,ResultType& resultType)
	{
		uintp arity;
		serializeVarUInt1(stream,arity);
		if(arity == 0) { resultType = ResultType::none; }
		else
		{
			int8 encodedValueType = 0;
			serializeVarInt7(stream,encodedValueType);
			resultType = (ResultType)-encodedValueType;
		}
	}
	static void serialize(OutputStream& stream,ResultType& returnType)
	{
		uintp arity = returnType == ResultType::none ? 0 : 1;
		serializeVarUInt1(stream,arity);
		if(arity)
		{
			int8 encodedValueType = -(int8)returnType;
			serializeVarInt7(stream,encodedValueType);
		}
	}
	
	template<typename Stream>
	void serialize(Stream& stream,SizeConstraints& sizeConstraints)
	{
		uintp flags = sizeConstraints.max != UINT64_MAX ? 1 : 0;
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
	void serialize(Stream& stream,Export& e)
	{
		serialize(stream,e.name);
		serialize(stream,e.kind);
		serializeVarUInt32(stream,e.index);
	}

	template<typename Stream>
	void serialize(Stream& stream,InitializerExpression& initializer)
	{
		serializeNativeValue(stream,*(uint8*)&initializer.type);
		switch(initializer.type)
		{
		case InitializerExpression::Type::i32_const: serializeVarInt32(stream,initializer.i32); break;
		case InitializerExpression::Type::i64_const: serializeVarInt64(stream,initializer.i64); break;
		case InitializerExpression::Type::f32_const: serialize(stream,initializer.f32); break;
		case InitializerExpression::Type::f64_const: serialize(stream,initializer.f64); break;
		case InitializerExpression::Type::get_global: serializeVarUInt32(stream,initializer.globalIndex); break;
		default: throw FatalSerializationException("invalid initializer expression opcode");
		}
		serializeConstant(stream,"expected end opcode",(uint8)Opcode::end);
	}
	
	template<typename Stream>
	void serialize(Stream& stream,TableDef& tableDef)
	{
		serialize(stream,tableDef.type);
	}

	template<typename Stream>
	void serialize(Stream& stream,MemoryDef& memoryDef)
	{
		serialize(stream,memoryDef.type);
	}

	template<typename Stream>
	void serialize(Stream& stream,GlobalDef& globalDef)
	{
		serialize(stream,globalDef.type);
		serialize(stream,globalDef.initializer);
	}

	template<typename Stream>
	void serialize(Stream& stream,DataSegment& dataSegment)
	{
		serializeVarUInt32(stream,dataSegment.memoryIndex);
		serialize(stream,dataSegment.baseOffset);
		serialize(stream,dataSegment.data);
	}

	template<typename Stream>
	void serialize(Stream& stream,TableSegment& tableSegment)
	{
		serializeVarUInt32(stream,tableSegment.tableIndex);
		serialize(stream,tableSegment.baseOffset);
		serializeArray(stream,tableSegment.indices,[](Stream& stream,uintp& functionIndex){serializeVarUInt32(stream,functionIndex);});
	}
}

namespace WASM
{
	using namespace IR;
	using namespace Serialization;
	
	enum
	{
		magicNumber=0x6d736100, // "\0asm"
		currentVersion=1
	};

	enum class SectionType : uint8
	{
		unknown = 0,
		user = 0,
		type = 1,
		import = 2,
		functionDeclarations = 3,
		table = 4,
		memory = 5,
		global = 6,
		export_ = 7,
		start = 8,
		elem = 9,
		functionDefinitions = 10,
		data = 11
	};
	
	template<typename SerializeSection>
	void serializeSection(OutputStream& stream,SectionType type,SerializeSection serializeSectionBody)
	{
		serializeNativeValue(stream,type);
		ArrayOutputStream sectionStream;
		serializeSectionBody(sectionStream);
		std::vector<uint8> sectionBytes = sectionStream.getBytes();
		size_t sectionNumBytes = sectionBytes.size();
		serializeVarUInt32(stream,sectionNumBytes);
		serializeBytes(stream,sectionBytes.data(),sectionBytes.size());
	}
	template<typename SerializeSection>
	void serializeSection(InputStream& stream,SectionType expectedType,SerializeSection serializeSectionBody)
	{
		assert((SectionType)*stream.peek(sizeof(SectionType)) == expectedType);
		stream.advance(sizeof(SectionType));
		size_t numSectionBytes = 0;
		serializeVarUInt32(stream,numSectionBytes);
		MemoryInputStream sectionStream(stream.advance(numSectionBytes),numSectionBytes);
		serializeSectionBody(sectionStream);
		if(sectionStream.capacity()) { throw FatalSerializationException("section contained more data than expected"); }
	}
	
	void serialize(OutputStream& stream,UserSection& userSection)
	{
		serializeConstant(stream,"expected user section (section ID 0)",(uint8)SectionType::user);
		ArrayOutputStream sectionStream;
		serialize(sectionStream,userSection.name);
		serializeBytes(sectionStream,userSection.data.data(),userSection.data.size());
		std::vector<uint8> sectionBytes = sectionStream.getBytes();
		serialize(stream,sectionBytes);
	}
	
	void serialize(InputStream& stream,UserSection& userSection)
	{
		serializeConstant(stream,"expected user section (section ID 0)",(uint8)SectionType::user);
		size_t numSectionBytes = 0;
		serializeVarUInt32(stream,numSectionBytes);
		
		MemoryInputStream sectionStream(stream.advance(numSectionBytes),numSectionBytes);
		serialize(sectionStream,userSection.name);
		userSection.data.resize(sectionStream.capacity());
		serializeBytes(sectionStream,userSection.data.data(),userSection.data.size());
		assert(!sectionStream.capacity());
	}

	struct LocalSet
	{
		uintp num;
		ValueType type;
	};
	
	template<typename Stream>
	void serialize(Stream& stream,LocalSet& localSet)
	{
		serializeVarUInt32(stream,localSet.num);
		serialize(stream,localSet.type);
	}

	void serializeFunctionBody(OutputStream& sectionStream,Module& module,FunctionDef& functionDef)
	{
		ArrayOutputStream bodyStream;

		// Convert the function's local types into LocalSets: runs of locals of the same type.
		LocalSet* localSets = (LocalSet*)alloca(sizeof(LocalSet)*functionDef.nonParameterLocalTypes.size());
		uintp numLocalSets = 0;
		if(functionDef.nonParameterLocalTypes.size())
		{
			localSets[0].type = ValueType::any;
			localSets[0].num = 0;
			for(auto localType : functionDef.nonParameterLocalTypes)
			{
				if(localSets[numLocalSets].type != localType)
				{
					if(localSets[numLocalSets].type != ValueType::any) { ++numLocalSets; }
					localSets[numLocalSets].type = localType;
					localSets[numLocalSets].num = 0;
				}
				++localSets[numLocalSets].num;
			}
			if(localSets[numLocalSets].type != ValueType::any) { ++numLocalSets; }
		}

		// Serialize the local sets.
		serializeVarUInt32(bodyStream,numLocalSets);
		for(uintp setIndex = 0;setIndex < numLocalSets;++setIndex) { serialize(bodyStream,localSets[setIndex]); }

		serializeBytes(bodyStream,module.code.data() + functionDef.code.offset,functionDef.code.numBytes);

		std::vector<uint8> bodyBytes = bodyStream.getBytes();
		serialize(sectionStream,bodyBytes);
	}
	
	void serializeFunctionBody(InputStream& sectionStream,Module& module,FunctionDef& functionDef)
	{
		size_t numBodyBytes = 0;
		serializeVarUInt32(sectionStream,numBodyBytes);

		MemoryInputStream bodyStream(sectionStream.advance(numBodyBytes),numBodyBytes);
		
		// Deserialize local sets and unpack them into a linear array of local types.
		size_t numLocalSets = 0;
		serializeVarUInt32(bodyStream,numLocalSets);
		for(uintp setIndex = 0;setIndex < numLocalSets;++setIndex)
		{
			LocalSet localSet;
			serialize(bodyStream,localSet);
			for(uintp index = 0;index < localSet.num;++index) { functionDef.nonParameterLocalTypes.push_back(localSet.type); }
		}

		const size_t numCodeBytes = bodyStream.capacity();
		functionDef.code = {module.code.size(),numCodeBytes};
		module.code.resize(functionDef.code.offset + functionDef.code.numBytes);
		serializeBytes(bodyStream,module.code.data() + functionDef.code.offset,functionDef.code.numBytes);
	}
	
	template<typename Stream>
	void serializeTypeSection(Stream& moduleStream,Module& module)
	{
		serializeSection(moduleStream,SectionType::type,[&module](Stream& sectionStream)
		{
			serializeArray(sectionStream,module.types,[](Stream& stream,const FunctionType*& functionType)
			{
				serializeConstant(stream,"function type tag",uint8(0x60));
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
	}
	template<typename Stream>
	void serializeImportSection(Stream& moduleStream,Module& module)
	{
		serializeSection(moduleStream,SectionType::import,[&module](Stream& sectionStream)
		{
			size_t size = module.functions.imports.size()
				+ module.tables.imports.size()
				+ module.memories.imports.size()
				+ module.globals.imports.size();
			serializeVarUInt32(sectionStream,size);
			if(Stream::isInput)
			{
				for(uintp index = 0;index < size;++index)
				{
					std::string moduleName;
					std::string exportName;
					ObjectKind kind = ObjectKind::invalid;
					serialize(sectionStream,moduleName);
					serialize(sectionStream,exportName);
					serialize(sectionStream,kind);
					switch(kind)
					{
					case ObjectKind::function:
					{
						uintp functionTypeIndex = 0;
						serializeVarUInt32(sectionStream,functionTypeIndex);
						if(functionTypeIndex >= module.types.size())
						{
							throw FatalSerializationException("invalid import function type index");
						}
						module.functions.imports.push_back({{functionTypeIndex},std::move(moduleName),std::move(exportName)});
						break;
					}
					case ObjectKind::table:
					{
						TableType tableType;
						serialize(sectionStream,tableType);
						module.tables.imports.push_back({tableType,std::move(moduleName),std::move(exportName)});
						break;
					}
					case ObjectKind::memory:
					{
						MemoryType memoryType;
						serialize(sectionStream,memoryType);
						module.memories.imports.push_back({memoryType,std::move(moduleName),std::move(exportName)});
						break;
					}
					case ObjectKind::global:
					{
						GlobalType globalType;
						serialize(sectionStream,globalType);
						module.globals.imports.push_back({globalType,std::move(moduleName),std::move(exportName)});
						break;
					}
					default: throw FatalSerializationException("invalid ObjectKind");
					}
				}
			}
			else
			{
				for(auto& functionImport : module.functions.imports)
				{
					serialize(sectionStream,functionImport.moduleName);
					serialize(sectionStream,functionImport.exportName);
					ObjectKind kind = ObjectKind::function;
					serialize(sectionStream,kind);
					serializeVarUInt32(sectionStream,functionImport.type.index);
				}
				for(auto& tableImport : module.tables.imports)
				{
					serialize(sectionStream,tableImport.moduleName);
					serialize(sectionStream,tableImport.exportName);
					ObjectKind kind = ObjectKind::table;
					serialize(sectionStream,kind);
					serialize(sectionStream,tableImport.type);
				}
				for(auto& memoryImport : module.memories.imports)
				{
					serialize(sectionStream,memoryImport.moduleName);
					serialize(sectionStream,memoryImport.exportName);
					ObjectKind kind = ObjectKind::memory;
					serialize(sectionStream,kind);
					serialize(sectionStream,memoryImport.type);
				}
				for(auto& globalImport : module.globals.imports)
				{
					serialize(sectionStream,globalImport.moduleName);
					serialize(sectionStream,globalImport.exportName);
					ObjectKind kind = ObjectKind::global;
					serialize(sectionStream,kind);
					serialize(sectionStream,globalImport.type);
				}
			}
		});
	}

	template<typename Stream>
	void serializeFunctionSection(Stream& moduleStream,Module& module)
	{
		serializeSection(moduleStream,SectionType::functionDeclarations,[&module](Stream& sectionStream)
		{
			size_t numFunctions = module.functions.defs.size();
			serializeVarUInt32(sectionStream,numFunctions);
			if(Stream::isInput)
			{
				// Grow the vector one element at a time:
				// try to get a serialization exception before making a huge allocation for malformed input.
				module.functions.defs.clear();
				for(uintp functionIndex = 0;functionIndex < numFunctions;++functionIndex)
				{
					uintp functionTypeIndex = 0;
					serializeVarUInt32(sectionStream,functionTypeIndex);
					if(functionTypeIndex >= module.types.size()) { throw FatalSerializationException("invalid function type index"); }
					module.functions.defs.push_back({{functionTypeIndex},{},CodeRef()});
				}
				module.functions.defs.shrink_to_fit();
			}
			else
			{
				for(FunctionDef& function : module.functions.defs)
				{
					serializeVarUInt32(sectionStream,function.type.index);
				}
			}
		});
	}

	template<typename Stream>
	void serializeTableSection(Stream& moduleStream,Module& module)
	{
		serializeSection(moduleStream,SectionType::table,[&module](Stream& sectionStream)
		{
			serialize(sectionStream,module.tables.defs);
		});
	}

	template<typename Stream>
	void serializeMemorySection(Stream& moduleStream,Module& module)
	{
		serializeSection(moduleStream,SectionType::memory,[&module](Stream& sectionStream)
		{
			serialize(sectionStream,module.memories.defs);
		});
	}

	template<typename Stream>
	void serializeGlobalSection(Stream& moduleStream,Module& module)
	{
		serializeSection(moduleStream,SectionType::global,[&module](Stream& sectionStream)
		{
			serialize(sectionStream,module.globals.defs);
		});
	}

	template<typename Stream>
	void serializeExportSection(Stream& moduleStream,Module& module)
	{
		serializeSection(moduleStream,SectionType::export_,[&module](Stream& sectionStream)
		{
			serialize(sectionStream,module.exports);
		});
	}

	template<typename Stream>
	void serializeStartSection(Stream& moduleStream,Module& module)
	{
		serializeSection(moduleStream,SectionType::start,[&module](Stream& sectionStream)
		{
			serializeVarUInt32(sectionStream,module.startFunctionIndex);
		});
	}

	template<typename Stream>
	void serializeElementSection(Stream& moduleStream,Module& module)
	{
		serializeSection(moduleStream,SectionType::elem,[&module](Stream& sectionStream)
		{
			serialize(sectionStream,module.tableSegments);
		});
	}

	template<typename Stream>
	void serializeCodeSection(Stream& moduleStream,Module& module)
	{
		serializeSection(moduleStream,SectionType::functionDefinitions,[&module](Stream& sectionStream)
		{
			size_t numFunctionBodies = module.functions.defs.size();
			serializeVarUInt32(sectionStream,numFunctionBodies);
			if(Stream::isInput && numFunctionBodies != module.functions.defs.size())
				{ throw FatalSerializationException("function and code sections have mismatched function counts"); }
			for(FunctionDef& functionDef : module.functions.defs) { serializeFunctionBody(sectionStream,module,functionDef); }
		});
	}

	template<typename Stream>
	void serializeDataSection(Stream& moduleStream,Module& module)
	{
		serializeSection(moduleStream,SectionType::data,[&module](Stream& sectionStream)
		{
			serialize(sectionStream,module.dataSegments);
		});
	}

	void serializeModule(OutputStream& moduleStream,Module& module)
	{
		serializeConstant(moduleStream,"magic number",uint32(magicNumber));
		serializeConstant(moduleStream,"version",uint32(currentVersion));

		if(module.types.size() > 0) { serializeTypeSection(moduleStream,module); }
		if(module.functions.imports.size() > 0
		|| module.tables.imports.size() > 0
		|| module.memories.imports.size() > 0
		|| module.globals.imports.size() > 0) { serializeImportSection(moduleStream,module); }
		if(module.functions.defs.size() > 0) { serializeFunctionSection(moduleStream,module); }
		if(module.tables.defs.size() > 0) { serializeTableSection(moduleStream,module); }
		if(module.memories.defs.size() > 0) { serializeMemorySection(moduleStream,module); }
		if(module.globals.defs.size() > 0) { serializeGlobalSection(moduleStream,module); }
		if(module.exports.size() > 0) { serializeExportSection(moduleStream,module); }
		if(module.startFunctionIndex != UINTPTR_MAX) { serializeStartSection(moduleStream,module); }
		if(module.tableSegments.size() > 0) { serializeElementSection(moduleStream,module); }
		if(module.functions.defs.size() > 0) { serializeCodeSection(moduleStream,module); }
		if(module.dataSegments.size() > 0) { serializeDataSection(moduleStream,module); }

		for(auto& userSection : module.userSections) { serialize(moduleStream,userSection); }
	}
	void serializeModule(InputStream& moduleStream,Module& module)
	{
		serializeConstant(moduleStream,"magic number",uint32(magicNumber));
		serializeConstant(moduleStream,"version",uint32(currentVersion));

		SectionType lastKnownSectionType = SectionType::unknown;
		while(moduleStream.capacity())
		{
			const SectionType sectionType = *(SectionType*)moduleStream.peek(sizeof(SectionType));
			if(sectionType != SectionType::user)
			{
				if(sectionType > lastKnownSectionType) { lastKnownSectionType = sectionType; }
				else { throw FatalSerializationException("incorrect order for known section"); }
			}
			switch(sectionType)
			{
			case SectionType::type: serializeTypeSection(moduleStream,module); break;
			case SectionType::import: serializeImportSection(moduleStream,module); break;
			case SectionType::functionDeclarations: serializeFunctionSection(moduleStream,module); break;
			case SectionType::table: serializeTableSection(moduleStream,module); break;
			case SectionType::memory: serializeMemorySection(moduleStream,module); break;
			case SectionType::global: serializeGlobalSection(moduleStream,module); break;
			case SectionType::export_: serializeExportSection(moduleStream,module); break;
			case SectionType::start: serializeStartSection(moduleStream,module); break;
			case SectionType::elem: serializeElementSection(moduleStream,module); break;
			case SectionType::functionDefinitions: serializeCodeSection(moduleStream,module); break;
			case SectionType::data: serializeDataSection(moduleStream,module); break;
			case SectionType::user:
			{
				UserSection& userSection = *module.userSections.insert(module.userSections.end(),UserSection());
				serialize(moduleStream,userSection);
				break;
			}
			default: throw FatalSerializationException("unknown section ID");
			};
		};
	}

	void serialize(Serialization::InputStream& stream,Module& module)
	{
		serializeModule(stream,module);
		IR::validateDefinitions(module);
		IR::validateCode(module);
	}
	void serialize(Serialization::OutputStream& stream,const Module& module)
	{
		serializeModule(stream,const_cast<Module&>(module));
	}
}
