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
		user = 0,
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
		name = 0x80
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
		if(arity == 0) { returnType = ResultType::none; }
		else { serializeNativeValue(stream,returnType); }
	}
	void serialize(OutputStream& stream,ResultType& returnType)
	{
		uintptr arity = returnType == ResultType::none ? 0 : 1;
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
		serializeArray(stream,tableSegment.indices,[](Stream& stream,uintptr& functionIndex){serializeVarUInt32(stream,functionIndex);});
	}

	template<typename SerializeSection>
	void serializeSection(OutputStream& stream,SectionType type,bool mustSerialize,SerializeSection serializeSectionBody)
	{
		if(mustSerialize)
		{
			serializeNativeValue(stream,type);
			ArrayOutputStream sectionStream;
			serializeSectionBody(sectionStream);
			std::vector<uint8> sectionBytes = sectionStream.getBytes();
			size_t sectionNumBytes = sectionBytes.size();
			serializeVarUInt32(stream,sectionNumBytes);
			serializeBytes(stream,sectionBytes.data(),sectionBytes.size());
		}
	}
	template<typename SerializeSection>
	void serializeSection(InputStream& stream,SectionType expectedType,bool,SerializeSection serializeSectionBody)
	{
		if(stream.capacity())
		{
			SectionType type = (SectionType)*stream.peek(sizeof(SectionType));
			if(type == expectedType)
			{
				stream.advance(sizeof(SectionType));
				size_t numSectionBytes = 0;
				serializeVarUInt32(stream,numSectionBytes);
				MemoryInputStream sectionStream(stream.advance(numSectionBytes),numSectionBytes);
				serializeSectionBody(sectionStream);
				if(sectionStream.capacity()) { throw FatalSerializationException("section contained more data than expected"); }
			}
		}
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
		for(uintptr setIndex = 0;setIndex < numLocalSets;++setIndex)
		{
			LocalSet localSet;
			serialize(bodyStream,localSet);
			for(uintptr index = 0;index < localSet.num;++index) { function.nonParameterLocalTypes.push_back(localSet.type); }
		}

		const size_t numCodeBytes = bodyStream.capacity();
		function.code = {module.code.size(),numCodeBytes};
		module.code.resize(function.code.offset + function.code.numBytes);
		serializeBytes(bodyStream,module.code.data() + function.code.offset,function.code.numBytes);
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
			serializeArray(sectionStream,module.tableDefs,[](Stream& elementStream,TableType& tableType)
			{
				serialize(elementStream,tableType.elementType);
				serialize(elementStream,tableType.size);
			});
		});
		serializeSection(moduleStream,SectionType::memory,module.memoryDefs.size()>0,[&module](Stream& sectionStream)
		{
			serializeArray(sectionStream,module.memoryDefs,[](Stream& elementStream,MemoryType& memoryType)
			{
				serialize(elementStream,memoryType.size);
			});
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

		uintptr userSectionIndex = 0;
		while((Stream::isInput && moduleStream.capacity()) || userSectionIndex < module.userSections.size())
		{
			UserSection& userSection = Stream::isInput ? *module.userSections.insert(module.userSections.end(),UserSection()) : module.userSections[userSectionIndex];
			serialize(moduleStream,userSection);
			++userSectionIndex;
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
		const uintptr numImportedFunctions = outNames.functions.size();
		const uintptr numImportedTables = outNames.tables.size();
		const uintptr numImportedMemories = outNames.memories.size();
		const uintptr numImportedGlobals = outNames.globals.size();
		for(uintptr typeIndex = 0;typeIndex < module.types.size();++typeIndex) { outNames.types.push_back(""); }
		for(uintptr tableDefIndex = 0;tableDefIndex < module.tableDefs.size();++tableDefIndex) { outNames.tables.push_back(""); }
		for(uintptr memoryDefIndex = 0;memoryDefIndex < module.memoryDefs.size();++memoryDefIndex) { outNames.memories.push_back(""); }
		for(uintptr globalDefIndex = 0;globalDefIndex < module.globalDefs.size();++globalDefIndex) { outNames.globals.push_back(""); }
		for(uintptr functionDefIndex = 0;functionDefIndex < module.functionDefs.size();++functionDefIndex)
		{
			const Function& functionDef = module.functionDefs[functionDefIndex];
			const FunctionType* functionType = module.types[functionDef.typeIndex];
			outNames.functions.push_back("");
			DisassemblyNames::FunctionDef functionDefNames;
			for(uintptr parameterIndex = 0;parameterIndex < functionType->parameters.size();++parameterIndex) { functionDefNames.locals.push_back(""); }
			for(uintptr localIndex = 0;localIndex < functionDef.nonParameterLocalTypes.size();++localIndex) { functionDefNames.locals.push_back(""); }
			outNames.functionDefs.push_back(std::move(functionDefNames));
		}

		// Deserialize the name section, if it is present.
		uintptr userSectionIndex = 0;
		if(findUserSection(module,"name",userSectionIndex))
		{
			try
			{
				const UserSection& nameSection = module.userSections[userSectionIndex];
				MemoryInputStream stream(nameSection.data.data(),nameSection.data.size());
			
				size_t numFunctionNames = 0;
				serializeVarUInt32(stream,numFunctionNames);
				numFunctionNames = std::min(numFunctionNames,module.functionDefs.size());

				for(uintptr functionDefIndex = 0;functionDefIndex < numFunctionNames;++functionDefIndex)
				{
					const uintptr functionIndex = numImportedFunctions + functionDefIndex;
					DisassemblyNames::FunctionDef& functionDefNames = outNames.functionDefs[functionDefIndex];

					serialize(stream,outNames.functions[functionIndex]);

					size_t numLocalNames = 0;
					serializeVarUInt32(stream,numLocalNames);
					numLocalNames = std::min(numLocalNames,functionDefNames.locals.size());

					for(uintptr localIndex = 0;localIndex < numLocalNames;++localIndex)
					{ serialize(stream,functionDefNames.locals[localIndex]); }
				}
			}
			catch(FatalSerializationException exception)
			{
				Log::printf(Log::Category::debug,"FatalSerializationException while deserializing WASM user name section: %s",exception.message.c_str());
			}
		}
	}

	void setDisassemblyNames(Module& module,const DisassemblyNames& names)
	{
		// Replace an existing name section if one is present, or create a new section.
		uintptr userSectionIndex = 0;
		if(!findUserSection(module,"name",userSectionIndex))
		{
			userSectionIndex = module.userSections.size();
			module.userSections.push_back({"name",{}});
		}

		ArrayOutputStream stream;
		
		size_t numFunctionNames = std::max(names.functions.size(),names.functionDefs.size());
		serializeVarUInt32(stream,numFunctionNames);

		assert(names.functions.size() > names.functionDefs.size());
		const uintptr baseFunctionDefIndex = names.functions.size() - names.functionDefs.size();

		for(uintptr functionDefIndex = 0;functionDefIndex < numFunctionNames;++functionDefIndex)
		{
			const uintptr functionIndex = baseFunctionDefIndex + functionDefIndex;
			std::string functionName = functionIndex < names.functions.size() ? names.functions[functionIndex] : "";
			serialize(stream,functionName);

			size_t numLocalNames = functionDefIndex < names.functionDefs.size() ? names.functionDefs[functionDefIndex].locals.size() : 0;
			serializeVarUInt32(stream,numLocalNames);
			for(uintptr localIndex = 0;localIndex < numLocalNames;++localIndex)
			{
				std::string localName = names.functionDefs[functionDefIndex].locals[localIndex];
				serialize(stream,localName);
			}
		}

		module.userSections[userSectionIndex].data = stream.getBytes();
	}
}
