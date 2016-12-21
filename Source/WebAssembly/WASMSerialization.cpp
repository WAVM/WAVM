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
		currentVersion=0xd
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

	template<typename Stream>
	void serialize(Stream& stream,ValueType& type)
	{
		int8 encodedValueType = -(int8)type;
		serializeVarInt7(stream,encodedValueType);
		if(Stream::isInput) { type = (ValueType)-encodedValueType; }
	}

	void serialize(InputStream& stream,ResultType& resultType)
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
	void serialize(OutputStream& stream,ResultType& returnType)
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
	void serialize(Stream& stream,Import& import)
	{
		serialize(stream,import.module);
		serialize(stream,import.exportName);
		serialize(stream,import.type.kind);
		switch(import.type.kind)
		{
		case ObjectKind::function: serializeVarUInt32(stream,import.type.functionTypeIndex); break;
		case ObjectKind::table: serialize(stream,import.type.table); break;
		case ObjectKind::memory: serialize(stream,import.type.memory); break;
		case ObjectKind::global: serialize(stream,import.type.global); break;
		default: throw FatalSerializationException("invalid ObjectKind");
		}
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
	void serialize(Stream& stream,Global& global)
	{
		serialize(stream,global.type);
		serialize(stream,global.initializer);
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

	void serializeFunctionBody(OutputStream& sectionStream,Module& module,Function& function)
	{
		ArrayOutputStream bodyStream;

		// Convert the function's local types into LocalSets: runs of locals of the same type.
		LocalSet* localSets = (LocalSet*)alloca(sizeof(LocalSet)*function.nonParameterLocalTypes.size());
		uintp numLocalSets = 0;
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
		for(uintp setIndex = 0;setIndex < numLocalSets;++setIndex) { serialize(bodyStream,localSets[setIndex]); }

		serializeBytes(bodyStream,module.code.data() + function.code.offset,function.code.numBytes);

		std::vector<uint8> bodyBytes = bodyStream.getBytes();
		serialize(sectionStream,bodyBytes);
	}
	
	void serializeFunctionBody(InputStream& sectionStream,Module& module,Function& function)
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
			for(uintp index = 0;index < localSet.num;++index) { function.nonParameterLocalTypes.push_back(localSet.type); }
		}

		const size_t numCodeBytes = bodyStream.capacity();
		function.code = {module.code.size(),numCodeBytes};
		module.code.resize(function.code.offset + function.code.numBytes);
		serializeBytes(bodyStream,module.code.data() + function.code.offset,function.code.numBytes);
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
			serialize(sectionStream,module.imports);
		});
	}

	template<typename Stream>
	void serializeFunctionSection(Stream& moduleStream,Module& module)
	{
		serializeSection(moduleStream,SectionType::functionDeclarations,[&module](Stream& sectionStream)
		{
			size_t numFunctions = module.functionDefs.size();
			serializeVarUInt32(sectionStream,numFunctions);
			if(Stream::isInput)
			{
				// Grow the vector one element at a time:
				// try to get a serialization exception before making a huge allocation for malformed input.
				module.functionDefs.clear();
				for(uintp functionIndex = 0;functionIndex < numFunctions;++functionIndex)
				{
					module.functionDefs.push_back(Function());
					serializeVarUInt32(sectionStream,module.functionDefs.back().typeIndex);
				}
				module.functionDefs.shrink_to_fit();
			}
			else
			{
				for(Function& function : module.functionDefs) { serializeVarUInt32(sectionStream,function.typeIndex); }
			}
		});
	}

	template<typename Stream>
	void serializeTableSection(Stream& moduleStream,Module& module)
	{
		serializeSection(moduleStream,SectionType::table,[&module](Stream& sectionStream)
		{
			serializeArray(sectionStream,module.tableDefs,[](Stream& elementStream,TableType& tableType)
			{
				serialize(elementStream,tableType.elementType);
				serialize(elementStream,tableType.size);
			});
		});
	}

	template<typename Stream>
	void serializeMemorySection(Stream& moduleStream,Module& module)
	{
		serializeSection(moduleStream,SectionType::memory,[&module](Stream& sectionStream)
		{
			serializeArray(sectionStream,module.memoryDefs,[](Stream& elementStream,MemoryType& memoryType)
			{
				serialize(elementStream,memoryType.size);
			});
		});
	}

	template<typename Stream>
	void serializeGlobalSection(Stream& moduleStream,Module& module)
	{
		serializeSection(moduleStream,SectionType::global,[&module](Stream& sectionStream)
		{
			serialize(sectionStream,module.globalDefs);
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
			size_t numFunctionBodies = module.functionDefs.size();
			serializeVarUInt32(sectionStream,numFunctionBodies);
			if(Stream::isInput && numFunctionBodies != module.functionDefs.size())
				{ throw FatalSerializationException("function and code sections have mismatched function counts"); }
			for(Function& function : module.functionDefs) { serializeFunctionBody(sectionStream,module,function); }
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
		if(module.imports.size() > 0) { serializeImportSection(moduleStream,module); }
		if(module.functionDefs.size() > 0) { serializeFunctionSection(moduleStream,module); }
		if(module.tableDefs.size() > 0) { serializeTableSection(moduleStream,module); }
		if(module.memoryDefs.size() > 0) { serializeMemorySection(moduleStream,module); }
		if(module.globalDefs.size() > 0) { serializeGlobalSection(moduleStream,module); }
		if(module.exports.size() > 0) { serializeExportSection(moduleStream,module); }
		if(module.startFunctionIndex != UINTPTR_MAX) { serializeStartSection(moduleStream,module); }
		if(module.tableSegments.size() > 0) { serializeElementSection(moduleStream,module); }
		if(module.functionDefs.size() > 0) { serializeCodeSection(moduleStream,module); }
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
		validate(module);
	}
	void serialize(Serialization::OutputStream& stream,const Module& module)
	{
		serializeModule(stream,const_cast<Module&>(module));
	}

	void getDisassemblyNames(const Module& module,DisassemblyNames& outNames)
	{
		// Fill in the output with the correct number of blank names.
		for(auto import : module.imports)
		{
			switch(import.type.kind)
			{
			case ObjectKind::function: outNames.functions.push_back(""); break;
			case ObjectKind::table: outNames.tables.push_back(""); break;
			case ObjectKind::memory: outNames.memories.push_back(""); break;
			case ObjectKind::global: outNames.globals.push_back(""); break;
			default: Core::unreachable();
			};
		}
		const uintp numImportedFunctions = outNames.functions.size();
		outNames.types.insert(outNames.types.end(),module.types.size(),"");
		outNames.tables.insert(outNames.tables.end(),module.tableDefs.size(),"");
		outNames.memories.insert(outNames.memories.end(),module.memoryDefs.size(),"");
		outNames.globals.insert(outNames.globals.end(),module.globalDefs.size(),"");
		outNames.functions.insert(outNames.functions.end(),module.functionDefs.size(),"");
		for(uintp functionDefIndex = 0;functionDefIndex < module.functionDefs.size();++functionDefIndex)
		{
			const Function& functionDef = module.functionDefs[functionDefIndex];
			const FunctionType* functionType = module.types[functionDef.typeIndex];
			DisassemblyNames::FunctionDef functionDefNames;
			functionDefNames.locals.insert(functionDefNames.locals.begin(),functionType->parameters.size() + functionDef.nonParameterLocalTypes.size(),"");
			outNames.functionDefs.push_back(std::move(functionDefNames));
		}

		// Deserialize the name section, if it is present.
		uintp userSectionIndex = 0;
		if(findUserSection(module,"name",userSectionIndex))
		{
			try
			{
				const UserSection& nameSection = module.userSections[userSectionIndex];
				MemoryInputStream stream(nameSection.data.data(),nameSection.data.size());
			
				size_t numFunctionNames = 0;
				serializeVarUInt32(stream,numFunctionNames);
				numFunctionNames = std::min(numFunctionNames,module.functionDefs.size());

				for(uintp functionDefIndex = 0;functionDefIndex < numFunctionNames;++functionDefIndex)
				{
					const uintp functionIndex = numImportedFunctions + functionDefIndex;
					DisassemblyNames::FunctionDef& functionDefNames = outNames.functionDefs[functionDefIndex];

					serialize(stream,outNames.functions[functionIndex]);

					size_t numLocalNames = 0;
					serializeVarUInt32(stream,numLocalNames);
					numLocalNames = std::min(numLocalNames,functionDefNames.locals.size());

					for(uintp localIndex = 0;localIndex < numLocalNames;++localIndex)
					{ serialize(stream,functionDefNames.locals[localIndex]); }
				}
			}
			catch(FatalSerializationException exception)
			{
				Log::printf(Log::Category::debug,"FatalSerializationException while deserializing WASM user name section: %s\n",exception.message.c_str());
			}
		}
	}

	void setDisassemblyNames(Module& module,const DisassemblyNames& names)
	{
		// Replace an existing name section if one is present, or create a new section.
		uintp userSectionIndex = 0;
		if(!findUserSection(module,"name",userSectionIndex))
		{
			userSectionIndex = module.userSections.size();
			module.userSections.push_back({"name",{}});
		}

		ArrayOutputStream stream;
		
		size_t numFunctionNames = std::max(names.functions.size(),names.functionDefs.size());
		serializeVarUInt32(stream,numFunctionNames);

		assert(names.functions.size() >= names.functionDefs.size());
		const uintp baseFunctionDefIndex = names.functions.size() - names.functionDefs.size();

		for(uintp functionDefIndex = 0;functionDefIndex < numFunctionNames;++functionDefIndex)
		{
			const uintp functionIndex = baseFunctionDefIndex + functionDefIndex;
			std::string functionName = functionIndex < names.functions.size() ? names.functions[functionIndex] : "";
			serialize(stream,functionName);

			size_t numLocalNames = functionDefIndex < names.functionDefs.size() ? names.functionDefs[functionDefIndex].locals.size() : 0;
			serializeVarUInt32(stream,numLocalNames);
			for(uintp localIndex = 0;localIndex < numLocalNames;++localIndex)
			{
				std::string localName = names.functionDefs[functionDefIndex].locals[localIndex];
				serialize(stream,localName);
			}
		}

		module.userSections[userSectionIndex].data = stream.getBytes();
	}
}
