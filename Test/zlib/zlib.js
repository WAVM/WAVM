var Module;
if (!Module) Module = (typeof Module !== "undefined" ? Module : null) || {};
var moduleOverrides = {};
for (var key in Module) {
 if (Module.hasOwnProperty(key)) {
  moduleOverrides[key] = Module[key];
 }
}
var ENVIRONMENT_IS_WEB = typeof window === "object";
var ENVIRONMENT_IS_WORKER = typeof importScripts === "function";
var ENVIRONMENT_IS_NODE = typeof process === "object" && typeof require === "function" && !ENVIRONMENT_IS_WEB && !ENVIRONMENT_IS_WORKER;
var ENVIRONMENT_IS_SHELL = !ENVIRONMENT_IS_WEB && !ENVIRONMENT_IS_NODE && !ENVIRONMENT_IS_WORKER;
if (ENVIRONMENT_IS_NODE) {
 if (!Module["print"]) Module["print"] = function print(x) {
  process["stdout"].write(x + "\n");
 };
 if (!Module["printErr"]) Module["printErr"] = function printErr(x) {
  process["stderr"].write(x + "\n");
 };
 var nodeFS = require("fs");
 var nodePath = require("path");
 Module["read"] = function read(filename, binary) {
  filename = nodePath["normalize"](filename);
  var ret = nodeFS["readFileSync"](filename);
  if (!ret && filename != nodePath["resolve"](filename)) {
   filename = path.join(__dirname, "..", "src", filename);
   ret = nodeFS["readFileSync"](filename);
  }
  if (ret && !binary) ret = ret.toString();
  return ret;
 };
 Module["readBinary"] = function readBinary(filename) {
  var ret = Module["read"](filename, true);
  if (!ret.buffer) {
   ret = new Uint8Array(ret);
  }
  assert(ret.buffer);
  return ret;
 };
 Module["load"] = function load(f) {
  globalEval(read(f));
 };
 if (!Module["thisProgram"]) {
  if (process["argv"].length > 1) {
   Module["thisProgram"] = process["argv"][1].replace(/\\/g, "/");
  } else {
   Module["thisProgram"] = "unknown-program";
  }
 }
 Module["arguments"] = process["argv"].slice(2);
 if (typeof module !== "undefined") {
  module["exports"] = Module;
 }
 process["on"]("uncaughtException", (function(ex) {
  if (!(ex instanceof ExitStatus)) {
   throw ex;
  }
 }));
 Module["inspect"] = (function() {
  return "[Emscripten Module object]";
 });
} else if (ENVIRONMENT_IS_SHELL) {
 if (!Module["print"]) Module["print"] = print;
 if (typeof printErr != "undefined") Module["printErr"] = printErr;
 if (typeof read != "undefined") {
  Module["read"] = read;
 } else {
  Module["read"] = function read() {
   throw "no read() available (jsc?)";
  };
 }
 Module["readBinary"] = function readBinary(f) {
  if (typeof readbuffer === "function") {
   return new Uint8Array(readbuffer(f));
  }
  var data = read(f, "binary");
  assert(typeof data === "object");
  return data;
 };
 if (typeof scriptArgs != "undefined") {
  Module["arguments"] = scriptArgs;
 } else if (typeof arguments != "undefined") {
  Module["arguments"] = arguments;
 }
} else if (ENVIRONMENT_IS_WEB || ENVIRONMENT_IS_WORKER) {
 Module["read"] = function read(url) {
  var xhr = new XMLHttpRequest;
  xhr.open("GET", url, false);
  xhr.send(null);
  return xhr.responseText;
 };
 if (typeof arguments != "undefined") {
  Module["arguments"] = arguments;
 }
 if (typeof console !== "undefined") {
  if (!Module["print"]) Module["print"] = function print(x) {
   console.log(x);
  };
  if (!Module["printErr"]) Module["printErr"] = function printErr(x) {
   console.log(x);
  };
 } else {
  var TRY_USE_DUMP = false;
  if (!Module["print"]) Module["print"] = TRY_USE_DUMP && typeof dump !== "undefined" ? (function(x) {
   dump(x);
  }) : (function(x) {});
 }
 if (ENVIRONMENT_IS_WORKER) {
  Module["load"] = importScripts;
 }
 if (typeof Module["setWindowTitle"] === "undefined") {
  Module["setWindowTitle"] = (function(title) {
   document.title = title;
  });
 }
} else {
 throw "Unknown runtime environment. Where are we?";
}
function globalEval(x) {
 eval.call(null, x);
}
if (!Module["load"] && Module["read"]) {
 Module["load"] = function load(f) {
  globalEval(Module["read"](f));
 };
}
if (!Module["print"]) {
 Module["print"] = (function() {});
}
if (!Module["printErr"]) {
 Module["printErr"] = Module["print"];
}
if (!Module["arguments"]) {
 Module["arguments"] = [];
}
if (!Module["thisProgram"]) {
 Module["thisProgram"] = "./this.program";
}
Module.print = Module["print"];
Module.printErr = Module["printErr"];
Module["preRun"] = [];
Module["postRun"] = [];
for (var key in moduleOverrides) {
 if (moduleOverrides.hasOwnProperty(key)) {
  Module[key] = moduleOverrides[key];
 }
}
var Runtime = {
 setTempRet0: (function(value) {
  tempRet0 = value;
 }),
 getTempRet0: (function() {
  return tempRet0;
 }),
 stackSave: (function() {
  return STACKTOP;
 }),
 stackRestore: (function(stackTop) {
  STACKTOP = stackTop;
 }),
 getNativeTypeSize: (function(type) {
  switch (type) {
  case "i1":
  case "i8":
   return 1;
  case "i16":
   return 2;
  case "i32":
   return 4;
  case "i64":
   return 8;
  case "float":
   return 4;
  case "double":
   return 8;
  default:
   {
    if (type[type.length - 1] === "*") {
     return Runtime.QUANTUM_SIZE;
    } else if (type[0] === "i") {
     var bits = parseInt(type.substr(1));
     assert(bits % 8 === 0);
     return bits / 8;
    } else {
     return 0;
    }
   }
  }
 }),
 getNativeFieldSize: (function(type) {
  return Math.max(Runtime.getNativeTypeSize(type), Runtime.QUANTUM_SIZE);
 }),
 STACK_ALIGN: 16,
 prepVararg: (function(ptr, type) {
  if (type === "double" || type === "i64") {
   if (ptr & 7) {
    assert((ptr & 7) === 4);
    ptr += 4;
   }
  } else {
   assert((ptr & 3) === 0);
  }
  return ptr;
 }),
 getAlignSize: (function(type, size, vararg) {
  if (!vararg && (type == "i64" || type == "double")) return 8;
  if (!type) return Math.min(size, 8);
  return Math.min(size || (type ? Runtime.getNativeFieldSize(type) : 0), Runtime.QUANTUM_SIZE);
 }),
 dynCall: (function(sig, ptr, args) {
  if (args && args.length) {
   if (!args.splice) args = Array.prototype.slice.call(args);
   args.splice(0, 0, ptr);
   return Module["dynCall_" + sig].apply(null, args);
  } else {
   return Module["dynCall_" + sig].call(null, ptr);
  }
 }),
 functionPointers: [],
 addFunction: (function(func) {
  for (var i = 0; i < Runtime.functionPointers.length; i++) {
   if (!Runtime.functionPointers[i]) {
    Runtime.functionPointers[i] = func;
    return 2 * (1 + i);
   }
  }
  throw "Finished up all reserved function pointers. Use a higher value for RESERVED_FUNCTION_POINTERS.";
 }),
 removeFunction: (function(index) {
  Runtime.functionPointers[(index - 2) / 2] = null;
 }),
 warnOnce: (function(text) {
  if (!Runtime.warnOnce.shown) Runtime.warnOnce.shown = {};
  if (!Runtime.warnOnce.shown[text]) {
   Runtime.warnOnce.shown[text] = 1;
   Module.printErr(text);
  }
 }),
 funcWrappers: {},
 getFuncWrapper: (function(func, sig) {
  assert(sig);
  if (!Runtime.funcWrappers[sig]) {
   Runtime.funcWrappers[sig] = {};
  }
  var sigCache = Runtime.funcWrappers[sig];
  if (!sigCache[func]) {
   sigCache[func] = function dynCall_wrapper() {
    return Runtime.dynCall(sig, func, arguments);
   };
  }
  return sigCache[func];
 }),
 getCompilerSetting: (function(name) {
  throw "You must build with -s RETAIN_COMPILER_SETTINGS=1 for Runtime.getCompilerSetting or emscripten_get_compiler_setting to work";
 }),
 stackAlloc: (function(size) {
  var ret = STACKTOP;
  STACKTOP = STACKTOP + size | 0;
  STACKTOP = STACKTOP + 15 & -16;
  return ret;
 }),
 staticAlloc: (function(size) {
  var ret = STATICTOP;
  STATICTOP = STATICTOP + size | 0;
  STATICTOP = STATICTOP + 15 & -16;
  return ret;
 }),
 dynamicAlloc: (function(size) {
  var ret = DYNAMICTOP;
  DYNAMICTOP = DYNAMICTOP + size | 0;
  DYNAMICTOP = DYNAMICTOP + 15 & -16;
  if (DYNAMICTOP >= TOTAL_MEMORY) {
   var success = enlargeMemory();
   if (!success) {
    DYNAMICTOP = ret;
    return 0;
   }
  }
  return ret;
 }),
 alignMemory: (function(size, quantum) {
  var ret = size = Math.ceil(size / (quantum ? quantum : 16)) * (quantum ? quantum : 16);
  return ret;
 }),
 makeBigInt: (function(low, high, unsigned) {
  var ret = unsigned ? +(low >>> 0) + +(high >>> 0) * +4294967296 : +(low >>> 0) + +(high | 0) * +4294967296;
  return ret;
 }),
 GLOBAL_BASE: 8,
 QUANTUM_SIZE: 4,
 __dummy__: 0
};
Module["Runtime"] = Runtime;
var __THREW__ = 0;
var ABORT = false;
var EXITSTATUS = 0;
var undef = 0;
var tempValue, tempInt, tempBigInt, tempInt2, tempBigInt2, tempPair, tempBigIntI, tempBigIntR, tempBigIntS, tempBigIntP, tempBigIntD, tempDouble, tempFloat;
var tempI64, tempI64b;
var tempRet0, tempRet1, tempRet2, tempRet3, tempRet4, tempRet5, tempRet6, tempRet7, tempRet8, tempRet9;
function assert(condition, text) {
 if (!condition) {
  abort("Assertion failed: " + text);
 }
}
var globalScope = this;
function getCFunc(ident) {
 var func = Module["_" + ident];
 if (!func) {
  try {
   func = eval("_" + ident);
  } catch (e) {}
 }
 assert(func, "Cannot call unknown function " + ident + " (perhaps LLVM optimizations or closure removed it?)");
 return func;
}
var cwrap, ccall;
((function() {
 var JSfuncs = {
  "stackSave": (function() {
   Runtime.stackSave();
  }),
  "stackRestore": (function() {
   Runtime.stackRestore();
  }),
  "arrayToC": (function(arr) {
   var ret = Runtime.stackAlloc(arr.length);
   writeArrayToMemory(arr, ret);
   return ret;
  }),
  "stringToC": (function(str) {
   var ret = 0;
   if (str !== null && str !== undefined && str !== 0) {
    ret = Runtime.stackAlloc((str.length << 2) + 1);
    writeStringToMemory(str, ret);
   }
   return ret;
  })
 };
 var toC = {
  "string": JSfuncs["stringToC"],
  "array": JSfuncs["arrayToC"]
 };
 ccall = function ccallFunc(ident, returnType, argTypes, args, opts) {
  var func = getCFunc(ident);
  var cArgs = [];
  var stack = 0;
  if (args) {
   for (var i = 0; i < args.length; i++) {
    var converter = toC[argTypes[i]];
    if (converter) {
     if (stack === 0) stack = Runtime.stackSave();
     cArgs[i] = converter(args[i]);
    } else {
     cArgs[i] = args[i];
    }
   }
  }
  var ret = func.apply(null, cArgs);
  if (returnType === "string") ret = Pointer_stringify(ret);
  if (stack !== 0) {
   if (opts && opts.async) {
    EmterpreterAsync.asyncFinalizers.push((function() {
     Runtime.stackRestore(stack);
    }));
    return;
   }
   Runtime.stackRestore(stack);
  }
  return ret;
 };
 var sourceRegex = /^function\s*\(([^)]*)\)\s*{\s*([^*]*?)[\s;]*(?:return\s*(.*?)[;\s]*)?}$/;
 function parseJSFunc(jsfunc) {
  var parsed = jsfunc.toString().match(sourceRegex).slice(1);
  return {
   arguments: parsed[0],
   body: parsed[1],
   returnValue: parsed[2]
  };
 }
 var JSsource = {};
 for (var fun in JSfuncs) {
  if (JSfuncs.hasOwnProperty(fun)) {
   JSsource[fun] = parseJSFunc(JSfuncs[fun]);
  }
 }
 cwrap = function cwrap(ident, returnType, argTypes) {
  argTypes = argTypes || [];
  var cfunc = getCFunc(ident);
  var numericArgs = argTypes.every((function(type) {
   return type === "number";
  }));
  var numericRet = returnType !== "string";
  if (numericRet && numericArgs) {
   return cfunc;
  }
  var argNames = argTypes.map((function(x, i) {
   return "$" + i;
  }));
  var funcstr = "(function(" + argNames.join(",") + ") {";
  var nargs = argTypes.length;
  if (!numericArgs) {
   funcstr += "var stack = " + JSsource["stackSave"].body + ";";
   for (var i = 0; i < nargs; i++) {
    var arg = argNames[i], type = argTypes[i];
    if (type === "number") continue;
    var convertCode = JSsource[type + "ToC"];
    funcstr += "var " + convertCode.arguments + " = " + arg + ";";
    funcstr += convertCode.body + ";";
    funcstr += arg + "=" + convertCode.returnValue + ";";
   }
  }
  var cfuncname = parseJSFunc((function() {
   return cfunc;
  })).returnValue;
  funcstr += "var ret = " + cfuncname + "(" + argNames.join(",") + ");";
  if (!numericRet) {
   var strgfy = parseJSFunc((function() {
    return Pointer_stringify;
   })).returnValue;
   funcstr += "ret = " + strgfy + "(ret);";
  }
  if (!numericArgs) {
   funcstr += JSsource["stackRestore"].body.replace("()", "(stack)") + ";";
  }
  funcstr += "return ret})";
  return eval(funcstr);
 };
}))();
Module["ccall"] = ccall;
Module["cwrap"] = cwrap;
function setValue(ptr, value, type, noSafe) {
 type = type || "i8";
 if (type.charAt(type.length - 1) === "*") type = "i32";
 switch (type) {
 case "i1":
  HEAP8[ptr >> 0] = value;
  break;
 case "i8":
  HEAP8[ptr >> 0] = value;
  break;
 case "i16":
  HEAP16[ptr >> 1] = value;
  break;
 case "i32":
  HEAP32[ptr >> 2] = value;
  break;
 case "i64":
  tempI64 = [ value >>> 0, (tempDouble = value, +Math_abs(tempDouble) >= +1 ? tempDouble > +0 ? (Math_min(+Math_floor(tempDouble / +4294967296), +4294967295) | 0) >>> 0 : ~~+Math_ceil((tempDouble - +(~~tempDouble >>> 0)) / +4294967296) >>> 0 : 0) ], HEAP32[ptr >> 2] = tempI64[0], HEAP32[ptr + 4 >> 2] = tempI64[1];
  break;
 case "float":
  HEAPF32[ptr >> 2] = value;
  break;
 case "double":
  HEAPF64[ptr >> 3] = value;
  break;
 default:
  abort("invalid type for setValue: " + type);
 }
}
Module["setValue"] = setValue;
function getValue(ptr, type, noSafe) {
 type = type || "i8";
 if (type.charAt(type.length - 1) === "*") type = "i32";
 switch (type) {
 case "i1":
  return HEAP8[ptr >> 0];
 case "i8":
  return HEAP8[ptr >> 0];
 case "i16":
  return HEAP16[ptr >> 1];
 case "i32":
  return HEAP32[ptr >> 2];
 case "i64":
  return HEAP32[ptr >> 2];
 case "float":
  return HEAPF32[ptr >> 2];
 case "double":
  return HEAPF64[ptr >> 3];
 default:
  abort("invalid type for setValue: " + type);
 }
 return null;
}
Module["getValue"] = getValue;
var ALLOC_NORMAL = 0;
var ALLOC_STACK = 1;
var ALLOC_STATIC = 2;
var ALLOC_DYNAMIC = 3;
var ALLOC_NONE = 4;
Module["ALLOC_NORMAL"] = ALLOC_NORMAL;
Module["ALLOC_STACK"] = ALLOC_STACK;
Module["ALLOC_STATIC"] = ALLOC_STATIC;
Module["ALLOC_DYNAMIC"] = ALLOC_DYNAMIC;
Module["ALLOC_NONE"] = ALLOC_NONE;
function allocate(slab, types, allocator, ptr) {
 var zeroinit, size;
 if (typeof slab === "number") {
  zeroinit = true;
  size = slab;
 } else {
  zeroinit = false;
  size = slab.length;
 }
 var singleType = typeof types === "string" ? types : null;
 var ret;
 if (allocator == ALLOC_NONE) {
  ret = ptr;
 } else {
  ret = [ _malloc, Runtime.stackAlloc, Runtime.staticAlloc, Runtime.dynamicAlloc ][allocator === undefined ? ALLOC_STATIC : allocator](Math.max(size, singleType ? 1 : types.length));
 }
 if (zeroinit) {
  var ptr = ret, stop;
  assert((ret & 3) == 0);
  stop = ret + (size & ~3);
  for (; ptr < stop; ptr += 4) {
   HEAP32[ptr >> 2] = 0;
  }
  stop = ret + size;
  while (ptr < stop) {
   HEAP8[ptr++ >> 0] = 0;
  }
  return ret;
 }
 if (singleType === "i8") {
  if (slab.subarray || slab.slice) {
   HEAPU8.set(slab, ret);
  } else {
   HEAPU8.set(new Uint8Array(slab), ret);
  }
  return ret;
 }
 var i = 0, type, typeSize, previousType;
 while (i < size) {
  var curr = slab[i];
  if (typeof curr === "function") {
   curr = Runtime.getFunctionIndex(curr);
  }
  type = singleType || types[i];
  if (type === 0) {
   i++;
   continue;
  }
  if (type == "i64") type = "i32";
  setValue(ret + i, curr, type);
  if (previousType !== type) {
   typeSize = Runtime.getNativeTypeSize(type);
   previousType = type;
  }
  i += typeSize;
 }
 return ret;
}
Module["allocate"] = allocate;
function getMemory(size) {
 if (!staticSealed) return Runtime.staticAlloc(size);
 if (typeof _sbrk !== "undefined" && !_sbrk.called || !runtimeInitialized) return Runtime.dynamicAlloc(size);
 return _malloc(size);
}
Module["getMemory"] = getMemory;
function Pointer_stringify(ptr, length) {
 if (length === 0 || !ptr) return "";
 var hasUtf = 0;
 var t;
 var i = 0;
 while (1) {
  t = HEAPU8[ptr + i >> 0];
  hasUtf |= t;
  if (t == 0 && !length) break;
  i++;
  if (length && i == length) break;
 }
 if (!length) length = i;
 var ret = "";
 if (hasUtf < 128) {
  var MAX_CHUNK = 1024;
  var curr;
  while (length > 0) {
   curr = String.fromCharCode.apply(String, HEAPU8.subarray(ptr, ptr + Math.min(length, MAX_CHUNK)));
   ret = ret ? ret + curr : curr;
   ptr += MAX_CHUNK;
   length -= MAX_CHUNK;
  }
  return ret;
 }
 return Module["UTF8ToString"](ptr);
}
Module["Pointer_stringify"] = Pointer_stringify;
function AsciiToString(ptr) {
 var str = "";
 while (1) {
  var ch = HEAP8[ptr++ >> 0];
  if (!ch) return str;
  str += String.fromCharCode(ch);
 }
}
Module["AsciiToString"] = AsciiToString;
function stringToAscii(str, outPtr) {
 return writeAsciiToMemory(str, outPtr, false);
}
Module["stringToAscii"] = stringToAscii;
function UTF8ArrayToString(u8Array, idx) {
 var u0, u1, u2, u3, u4, u5;
 var str = "";
 while (1) {
  u0 = u8Array[idx++];
  if (!u0) return str;
  if (!(u0 & 128)) {
   str += String.fromCharCode(u0);
   continue;
  }
  u1 = u8Array[idx++] & 63;
  if ((u0 & 224) == 192) {
   str += String.fromCharCode((u0 & 31) << 6 | u1);
   continue;
  }
  u2 = u8Array[idx++] & 63;
  if ((u0 & 240) == 224) {
   u0 = (u0 & 15) << 12 | u1 << 6 | u2;
  } else {
   u3 = u8Array[idx++] & 63;
   if ((u0 & 248) == 240) {
    u0 = (u0 & 7) << 18 | u1 << 12 | u2 << 6 | u3;
   } else {
    u4 = u8Array[idx++] & 63;
    if ((u0 & 252) == 248) {
     u0 = (u0 & 3) << 24 | u1 << 18 | u2 << 12 | u3 << 6 | u4;
    } else {
     u5 = u8Array[idx++] & 63;
     u0 = (u0 & 1) << 30 | u1 << 24 | u2 << 18 | u3 << 12 | u4 << 6 | u5;
    }
   }
  }
  if (u0 < 65536) {
   str += String.fromCharCode(u0);
  } else {
   var ch = u0 - 65536;
   str += String.fromCharCode(55296 | ch >> 10, 56320 | ch & 1023);
  }
 }
}
Module["UTF8ArrayToString"] = UTF8ArrayToString;
function UTF8ToString(ptr) {
 return UTF8ArrayToString(HEAPU8, ptr);
}
Module["UTF8ToString"] = UTF8ToString;
function stringToUTF8Array(str, outU8Array, outIdx, maxBytesToWrite) {
 if (!(maxBytesToWrite > 0)) return 0;
 var startIdx = outIdx;
 var endIdx = outIdx + maxBytesToWrite - 1;
 for (var i = 0; i < str.length; ++i) {
  var u = str.charCodeAt(i);
  if (u >= 55296 && u <= 57343) u = 65536 + ((u & 1023) << 10) | str.charCodeAt(++i) & 1023;
  if (u <= 127) {
   if (outIdx >= endIdx) break;
   outU8Array[outIdx++] = u;
  } else if (u <= 2047) {
   if (outIdx + 1 >= endIdx) break;
   outU8Array[outIdx++] = 192 | u >> 6;
   outU8Array[outIdx++] = 128 | u & 63;
  } else if (u <= 65535) {
   if (outIdx + 2 >= endIdx) break;
   outU8Array[outIdx++] = 224 | u >> 12;
   outU8Array[outIdx++] = 128 | u >> 6 & 63;
   outU8Array[outIdx++] = 128 | u & 63;
  } else if (u <= 2097151) {
   if (outIdx + 3 >= endIdx) break;
   outU8Array[outIdx++] = 240 | u >> 18;
   outU8Array[outIdx++] = 128 | u >> 12 & 63;
   outU8Array[outIdx++] = 128 | u >> 6 & 63;
   outU8Array[outIdx++] = 128 | u & 63;
  } else if (u <= 67108863) {
   if (outIdx + 4 >= endIdx) break;
   outU8Array[outIdx++] = 248 | u >> 24;
   outU8Array[outIdx++] = 128 | u >> 18 & 63;
   outU8Array[outIdx++] = 128 | u >> 12 & 63;
   outU8Array[outIdx++] = 128 | u >> 6 & 63;
   outU8Array[outIdx++] = 128 | u & 63;
  } else {
   if (outIdx + 5 >= endIdx) break;
   outU8Array[outIdx++] = 252 | u >> 30;
   outU8Array[outIdx++] = 128 | u >> 24 & 63;
   outU8Array[outIdx++] = 128 | u >> 18 & 63;
   outU8Array[outIdx++] = 128 | u >> 12 & 63;
   outU8Array[outIdx++] = 128 | u >> 6 & 63;
   outU8Array[outIdx++] = 128 | u & 63;
  }
 }
 outU8Array[outIdx] = 0;
 return outIdx - startIdx;
}
Module["stringToUTF8Array"] = stringToUTF8Array;
function stringToUTF8(str, outPtr, maxBytesToWrite) {
 return stringToUTF8Array(str, HEAPU8, outPtr, maxBytesToWrite);
}
Module["stringToUTF8"] = stringToUTF8;
function lengthBytesUTF8(str) {
 var len = 0;
 for (var i = 0; i < str.length; ++i) {
  var u = str.charCodeAt(i);
  if (u >= 55296 && u <= 57343) u = 65536 + ((u & 1023) << 10) | str.charCodeAt(++i) & 1023;
  if (u <= 127) {
   ++len;
  } else if (u <= 2047) {
   len += 2;
  } else if (u <= 65535) {
   len += 3;
  } else if (u <= 2097151) {
   len += 4;
  } else if (u <= 67108863) {
   len += 5;
  } else {
   len += 6;
  }
 }
 return len;
}
Module["lengthBytesUTF8"] = lengthBytesUTF8;
function UTF16ToString(ptr) {
 var i = 0;
 var str = "";
 while (1) {
  var codeUnit = HEAP16[ptr + i * 2 >> 1];
  if (codeUnit == 0) return str;
  ++i;
  str += String.fromCharCode(codeUnit);
 }
}
Module["UTF16ToString"] = UTF16ToString;
function stringToUTF16(str, outPtr, maxBytesToWrite) {
 if (maxBytesToWrite === undefined) {
  maxBytesToWrite = 2147483647;
 }
 if (maxBytesToWrite < 2) return 0;
 maxBytesToWrite -= 2;
 var startPtr = outPtr;
 var numCharsToWrite = maxBytesToWrite < str.length * 2 ? maxBytesToWrite / 2 : str.length;
 for (var i = 0; i < numCharsToWrite; ++i) {
  var codeUnit = str.charCodeAt(i);
  HEAP16[outPtr >> 1] = codeUnit;
  outPtr += 2;
 }
 HEAP16[outPtr >> 1] = 0;
 return outPtr - startPtr;
}
Module["stringToUTF16"] = stringToUTF16;
function lengthBytesUTF16(str) {
 return str.length * 2;
}
Module["lengthBytesUTF16"] = lengthBytesUTF16;
function UTF32ToString(ptr) {
 var i = 0;
 var str = "";
 while (1) {
  var utf32 = HEAP32[ptr + i * 4 >> 2];
  if (utf32 == 0) return str;
  ++i;
  if (utf32 >= 65536) {
   var ch = utf32 - 65536;
   str += String.fromCharCode(55296 | ch >> 10, 56320 | ch & 1023);
  } else {
   str += String.fromCharCode(utf32);
  }
 }
}
Module["UTF32ToString"] = UTF32ToString;
function stringToUTF32(str, outPtr, maxBytesToWrite) {
 if (maxBytesToWrite === undefined) {
  maxBytesToWrite = 2147483647;
 }
 if (maxBytesToWrite < 4) return 0;
 var startPtr = outPtr;
 var endPtr = startPtr + maxBytesToWrite - 4;
 for (var i = 0; i < str.length; ++i) {
  var codeUnit = str.charCodeAt(i);
  if (codeUnit >= 55296 && codeUnit <= 57343) {
   var trailSurrogate = str.charCodeAt(++i);
   codeUnit = 65536 + ((codeUnit & 1023) << 10) | trailSurrogate & 1023;
  }
  HEAP32[outPtr >> 2] = codeUnit;
  outPtr += 4;
  if (outPtr + 4 > endPtr) break;
 }
 HEAP32[outPtr >> 2] = 0;
 return outPtr - startPtr;
}
Module["stringToUTF32"] = stringToUTF32;
function lengthBytesUTF32(str) {
 var len = 0;
 for (var i = 0; i < str.length; ++i) {
  var codeUnit = str.charCodeAt(i);
  if (codeUnit >= 55296 && codeUnit <= 57343) ++i;
  len += 4;
 }
 return len;
}
Module["lengthBytesUTF32"] = lengthBytesUTF32;
function demangle(func) {
 var hasLibcxxabi = !!Module["___cxa_demangle"];
 if (hasLibcxxabi) {
  try {
   var buf = _malloc(func.length);
   writeStringToMemory(func.substr(1), buf);
   var status = _malloc(4);
   var ret = Module["___cxa_demangle"](buf, 0, 0, status);
   if (getValue(status, "i32") === 0 && ret) {
    return Pointer_stringify(ret);
   }
  } catch (e) {} finally {
   if (buf) _free(buf);
   if (status) _free(status);
   if (ret) _free(ret);
  }
 }
 var i = 3;
 var basicTypes = {
  "v": "void",
  "b": "bool",
  "c": "char",
  "s": "short",
  "i": "int",
  "l": "long",
  "f": "float",
  "d": "double",
  "w": "wchar_t",
  "a": "signed char",
  "h": "unsigned char",
  "t": "unsigned short",
  "j": "unsigned int",
  "m": "unsigned long",
  "x": "long long",
  "y": "unsigned long long",
  "z": "..."
 };
 var subs = [];
 var first = true;
 function dump(x) {
  if (x) Module.print(x);
  Module.print(func);
  var pre = "";
  for (var a = 0; a < i; a++) pre += " ";
  Module.print(pre + "^");
 }
 function parseNested() {
  i++;
  if (func[i] === "K") i++;
  var parts = [];
  while (func[i] !== "E") {
   if (func[i] === "S") {
    i++;
    var next = func.indexOf("_", i);
    var num = func.substring(i, next) || 0;
    parts.push(subs[num] || "?");
    i = next + 1;
    continue;
   }
   if (func[i] === "C") {
    parts.push(parts[parts.length - 1]);
    i += 2;
    continue;
   }
   var size = parseInt(func.substr(i));
   var pre = size.toString().length;
   if (!size || !pre) {
    i--;
    break;
   }
   var curr = func.substr(i + pre, size);
   parts.push(curr);
   subs.push(curr);
   i += pre + size;
  }
  i++;
  return parts;
 }
 function parse(rawList, limit, allowVoid) {
  limit = limit || Infinity;
  var ret = "", list = [];
  function flushList() {
   return "(" + list.join(", ") + ")";
  }
  var name;
  if (func[i] === "N") {
   name = parseNested().join("::");
   limit--;
   if (limit === 0) return rawList ? [ name ] : name;
  } else {
   if (func[i] === "K" || first && func[i] === "L") i++;
   var size = parseInt(func.substr(i));
   if (size) {
    var pre = size.toString().length;
    name = func.substr(i + pre, size);
    i += pre + size;
   }
  }
  first = false;
  if (func[i] === "I") {
   i++;
   var iList = parse(true);
   var iRet = parse(true, 1, true);
   ret += iRet[0] + " " + name + "<" + iList.join(", ") + ">";
  } else {
   ret = name;
  }
  paramLoop : while (i < func.length && limit-- > 0) {
   var c = func[i++];
   if (c in basicTypes) {
    list.push(basicTypes[c]);
   } else {
    switch (c) {
    case "P":
     list.push(parse(true, 1, true)[0] + "*");
     break;
    case "R":
     list.push(parse(true, 1, true)[0] + "&");
     break;
    case "L":
     {
      i++;
      var end = func.indexOf("E", i);
      var size = end - i;
      list.push(func.substr(i, size));
      i += size + 2;
      break;
     }
    case "A":
     {
      var size = parseInt(func.substr(i));
      i += size.toString().length;
      if (func[i] !== "_") throw "?";
      i++;
      list.push(parse(true, 1, true)[0] + " [" + size + "]");
      break;
     }
    case "E":
     break paramLoop;
    default:
     ret += "?" + c;
     break paramLoop;
    }
   }
  }
  if (!allowVoid && list.length === 1 && list[0] === "void") list = [];
  if (rawList) {
   if (ret) {
    list.push(ret + "?");
   }
   return list;
  } else {
   return ret + flushList();
  }
 }
 var parsed = func;
 try {
  if (func == "Object._main" || func == "_main") {
   return "main()";
  }
  if (typeof func === "number") func = Pointer_stringify(func);
  if (func[0] !== "_") return func;
  if (func[1] !== "_") return func;
  if (func[2] !== "Z") return func;
  switch (func[3]) {
  case "n":
   return "operator new()";
  case "d":
   return "operator delete()";
  }
  parsed = parse();
 } catch (e) {
  parsed += "?";
 }
 if (parsed.indexOf("?") >= 0 && !hasLibcxxabi) {
  Runtime.warnOnce("warning: a problem occurred in builtin C++ name demangling; build with  -s DEMANGLE_SUPPORT=1  to link in libcxxabi demangling");
 }
 return parsed;
}
function demangleAll(text) {
 return text.replace(/__Z[\w\d_]+/g, (function(x) {
  var y = demangle(x);
  return x === y ? x : x + " [" + y + "]";
 }));
}
function jsStackTrace() {
 var err = new Error;
 if (!err.stack) {
  try {
   throw new Error(0);
  } catch (e) {
   err = e;
  }
  if (!err.stack) {
   return "(no stack trace available)";
  }
 }
 return err.stack.toString();
}
function stackTrace() {
 return demangleAll(jsStackTrace());
}
Module["stackTrace"] = stackTrace;
var PAGE_SIZE = 4096;
function alignMemoryPage(x) {
 if (x % 4096 > 0) {
  x += 4096 - x % 4096;
 }
 return x;
}
var HEAP;
var HEAP8, HEAPU8, HEAP16, HEAPU16, HEAP32, HEAPU32, HEAPF32, HEAPF64;
var STATIC_BASE = 0, STATICTOP = 0, staticSealed = false;
var STACK_BASE = 0, STACKTOP = 0, STACK_MAX = 0;
var DYNAMIC_BASE = 0, DYNAMICTOP = 0;
function enlargeMemory() {
 abort("Cannot enlarge memory arrays. Either (1) compile with -s TOTAL_MEMORY=X with X higher than the current value " + TOTAL_MEMORY + ", (2) compile with ALLOW_MEMORY_GROWTH which adjusts the size at runtime but prevents some optimizations, or (3) set Module.TOTAL_MEMORY before the program runs.");
}
var TOTAL_STACK = Module["TOTAL_STACK"] || 5242880;
var TOTAL_MEMORY = Module["TOTAL_MEMORY"] || 134217728;
var totalMemory = 64 * 1024;
while (totalMemory < TOTAL_MEMORY || totalMemory < 2 * TOTAL_STACK) {
 if (totalMemory < 16 * 1024 * 1024) {
  totalMemory *= 2;
 } else {
  totalMemory += 16 * 1024 * 1024;
 }
}
if (totalMemory !== TOTAL_MEMORY) {
 TOTAL_MEMORY = totalMemory;
}
assert(typeof Int32Array !== "undefined" && typeof Float64Array !== "undefined" && !!(new Int32Array(1))["subarray"] && !!(new Int32Array(1))["set"], "JS engine does not provide full typed array support");
var buffer;
buffer = new ArrayBuffer(TOTAL_MEMORY);
HEAP8 = new Int8Array(buffer);
HEAP16 = new Int16Array(buffer);
HEAP32 = new Int32Array(buffer);
HEAPU8 = new Uint8Array(buffer);
HEAPU16 = new Uint16Array(buffer);
HEAPU32 = new Uint32Array(buffer);
HEAPF32 = new Float32Array(buffer);
HEAPF64 = new Float64Array(buffer);
HEAP32[0] = 255;
assert(HEAPU8[0] === 255 && HEAPU8[3] === 0, "Typed arrays 2 must be run on a little-endian system");
Module["HEAP"] = HEAP;
Module["buffer"] = buffer;
Module["HEAP8"] = HEAP8;
Module["HEAP16"] = HEAP16;
Module["HEAP32"] = HEAP32;
Module["HEAPU8"] = HEAPU8;
Module["HEAPU16"] = HEAPU16;
Module["HEAPU32"] = HEAPU32;
Module["HEAPF32"] = HEAPF32;
Module["HEAPF64"] = HEAPF64;
function callRuntimeCallbacks(callbacks) {
 while (callbacks.length > 0) {
  var callback = callbacks.shift();
  if (typeof callback == "function") {
   callback();
   continue;
  }
  var func = callback.func;
  if (typeof func === "number") {
   if (callback.arg === undefined) {
    Runtime.dynCall("v", func);
   } else {
    Runtime.dynCall("vi", func, [ callback.arg ]);
   }
  } else {
   func(callback.arg === undefined ? null : callback.arg);
  }
 }
}
var __ATPRERUN__ = [];
var __ATINIT__ = [];
var __ATMAIN__ = [];
var __ATEXIT__ = [];
var __ATPOSTRUN__ = [];
var runtimeInitialized = false;
var runtimeExited = false;
function preRun() {
 if (Module["preRun"]) {
  if (typeof Module["preRun"] == "function") Module["preRun"] = [ Module["preRun"] ];
  while (Module["preRun"].length) {
   addOnPreRun(Module["preRun"].shift());
  }
 }
 callRuntimeCallbacks(__ATPRERUN__);
}
function ensureInitRuntime() {
 if (runtimeInitialized) return;
 runtimeInitialized = true;
 callRuntimeCallbacks(__ATINIT__);
}
function preMain() {
 callRuntimeCallbacks(__ATMAIN__);
}
function exitRuntime() {
 callRuntimeCallbacks(__ATEXIT__);
 runtimeExited = true;
}
function postRun() {
 if (Module["postRun"]) {
  if (typeof Module["postRun"] == "function") Module["postRun"] = [ Module["postRun"] ];
  while (Module["postRun"].length) {
   addOnPostRun(Module["postRun"].shift());
  }
 }
 callRuntimeCallbacks(__ATPOSTRUN__);
}
function addOnPreRun(cb) {
 __ATPRERUN__.unshift(cb);
}
Module["addOnPreRun"] = addOnPreRun;
function addOnInit(cb) {
 __ATINIT__.unshift(cb);
}
Module["addOnInit"] = addOnInit;
function addOnPreMain(cb) {
 __ATMAIN__.unshift(cb);
}
Module["addOnPreMain"] = addOnPreMain;
function addOnExit(cb) {
 __ATEXIT__.unshift(cb);
}
Module["addOnExit"] = addOnExit;
function addOnPostRun(cb) {
 __ATPOSTRUN__.unshift(cb);
}
Module["addOnPostRun"] = addOnPostRun;
function intArrayFromString(stringy, dontAddNull, length) {
 var len = length > 0 ? length : lengthBytesUTF8(stringy) + 1;
 var u8array = new Array(len);
 var numBytesWritten = stringToUTF8Array(stringy, u8array, 0, u8array.length);
 if (dontAddNull) u8array.length = numBytesWritten;
 return u8array;
}
Module["intArrayFromString"] = intArrayFromString;
function intArrayToString(array) {
 var ret = [];
 for (var i = 0; i < array.length; i++) {
  var chr = array[i];
  if (chr > 255) {
   chr &= 255;
  }
  ret.push(String.fromCharCode(chr));
 }
 return ret.join("");
}
Module["intArrayToString"] = intArrayToString;
function writeStringToMemory(string, buffer, dontAddNull) {
 var array = intArrayFromString(string, dontAddNull);
 var i = 0;
 while (i < array.length) {
  var chr = array[i];
  HEAP8[buffer + i >> 0] = chr;
  i = i + 1;
 }
}
Module["writeStringToMemory"] = writeStringToMemory;
function writeArrayToMemory(array, buffer) {
 for (var i = 0; i < array.length; i++) {
  HEAP8[buffer++ >> 0] = array[i];
 }
}
Module["writeArrayToMemory"] = writeArrayToMemory;
function writeAsciiToMemory(str, buffer, dontAddNull) {
 for (var i = 0; i < str.length; ++i) {
  HEAP8[buffer++ >> 0] = str.charCodeAt(i);
 }
 if (!dontAddNull) HEAP8[buffer >> 0] = 0;
}
Module["writeAsciiToMemory"] = writeAsciiToMemory;
function unSign(value, bits, ignore) {
 if (value >= 0) {
  return value;
 }
 return bits <= 32 ? 2 * Math.abs(1 << bits - 1) + value : Math.pow(2, bits) + value;
}
function reSign(value, bits, ignore) {
 if (value <= 0) {
  return value;
 }
 var half = bits <= 32 ? Math.abs(1 << bits - 1) : Math.pow(2, bits - 1);
 if (value >= half && (bits <= 32 || value > half)) {
  value = -2 * half + value;
 }
 return value;
}
if (!Math["imul"] || Math["imul"](4294967295, 5) !== -5) Math["imul"] = function imul(a, b) {
 var ah = a >>> 16;
 var al = a & 65535;
 var bh = b >>> 16;
 var bl = b & 65535;
 return al * bl + (ah * bl + al * bh << 16) | 0;
};
Math.imul = Math["imul"];
if (!Math["clz32"]) Math["clz32"] = (function(x) {
 x = x >>> 0;
 for (var i = 0; i < 32; i++) {
  if (x & 1 << 31 - i) return i;
 }
 return 32;
});
Math.clz32 = Math["clz32"];
var Math_abs = Math.abs;
var Math_cos = Math.cos;
var Math_sin = Math.sin;
var Math_tan = Math.tan;
var Math_acos = Math.acos;
var Math_asin = Math.asin;
var Math_atan = Math.atan;
var Math_atan2 = Math.atan2;
var Math_exp = Math.exp;
var Math_log = Math.log;
var Math_sqrt = Math.sqrt;
var Math_ceil = Math.ceil;
var Math_floor = Math.floor;
var Math_pow = Math.pow;
var Math_imul = Math.imul;
var Math_fround = Math.fround;
var Math_min = Math.min;
var Math_clz32 = Math.clz32;
var runDependencies = 0;
var runDependencyWatcher = null;
var dependenciesFulfilled = null;
function getUniqueRunDependency(id) {
 return id;
}
function addRunDependency(id) {
 runDependencies++;
 if (Module["monitorRunDependencies"]) {
  Module["monitorRunDependencies"](runDependencies);
 }
}
Module["addRunDependency"] = addRunDependency;
function removeRunDependency(id) {
 runDependencies--;
 if (Module["monitorRunDependencies"]) {
  Module["monitorRunDependencies"](runDependencies);
 }
 if (runDependencies == 0) {
  if (runDependencyWatcher !== null) {
   clearInterval(runDependencyWatcher);
   runDependencyWatcher = null;
  }
  if (dependenciesFulfilled) {
   var callback = dependenciesFulfilled;
   dependenciesFulfilled = null;
   callback();
  }
 }
}
Module["removeRunDependency"] = removeRunDependency;
Module["preloadedImages"] = {};
Module["preloadedAudios"] = {};
var memoryInitializer = null;
var ASM_CONSTS = [];
STATIC_BASE = 8;
STATICTOP = STATIC_BASE + 18288;
__ATINIT__.push();
memoryInitializer = "zlib0.js.mem";
var tempDoublePtr = Runtime.alignMemory(allocate(12, "i8", ALLOC_STATIC), 8);
assert(tempDoublePtr % 8 == 0);
function copyTempFloat(ptr) {
 HEAP8[tempDoublePtr] = HEAP8[ptr];
 HEAP8[tempDoublePtr + 1] = HEAP8[ptr + 1];
 HEAP8[tempDoublePtr + 2] = HEAP8[ptr + 2];
 HEAP8[tempDoublePtr + 3] = HEAP8[ptr + 3];
}
function copyTempDouble(ptr) {
 HEAP8[tempDoublePtr] = HEAP8[ptr];
 HEAP8[tempDoublePtr + 1] = HEAP8[ptr + 1];
 HEAP8[tempDoublePtr + 2] = HEAP8[ptr + 2];
 HEAP8[tempDoublePtr + 3] = HEAP8[ptr + 3];
 HEAP8[tempDoublePtr + 4] = HEAP8[ptr + 4];
 HEAP8[tempDoublePtr + 5] = HEAP8[ptr + 5];
 HEAP8[tempDoublePtr + 6] = HEAP8[ptr + 6];
 HEAP8[tempDoublePtr + 7] = HEAP8[ptr + 7];
}
var _BDtoIHigh = true;
Module["_i64Subtract"] = _i64Subtract;
function ___setErrNo(value) {
 if (Module["___errno_location"]) HEAP32[Module["___errno_location"]() >> 2] = value;
 return value;
}
var ERRNO_CODES = {
 EPERM: 1,
 ENOENT: 2,
 ESRCH: 3,
 EINTR: 4,
 EIO: 5,
 ENXIO: 6,
 E2BIG: 7,
 ENOEXEC: 8,
 EBADF: 9,
 ECHILD: 10,
 EAGAIN: 11,
 EWOULDBLOCK: 11,
 ENOMEM: 12,
 EACCES: 13,
 EFAULT: 14,
 ENOTBLK: 15,
 EBUSY: 16,
 EEXIST: 17,
 EXDEV: 18,
 ENODEV: 19,
 ENOTDIR: 20,
 EISDIR: 21,
 EINVAL: 22,
 ENFILE: 23,
 EMFILE: 24,
 ENOTTY: 25,
 ETXTBSY: 26,
 EFBIG: 27,
 ENOSPC: 28,
 ESPIPE: 29,
 EROFS: 30,
 EMLINK: 31,
 EPIPE: 32,
 EDOM: 33,
 ERANGE: 34,
 ENOMSG: 42,
 EIDRM: 43,
 ECHRNG: 44,
 EL2NSYNC: 45,
 EL3HLT: 46,
 EL3RST: 47,
 ELNRNG: 48,
 EUNATCH: 49,
 ENOCSI: 50,
 EL2HLT: 51,
 EDEADLK: 35,
 ENOLCK: 37,
 EBADE: 52,
 EBADR: 53,
 EXFULL: 54,
 ENOANO: 55,
 EBADRQC: 56,
 EBADSLT: 57,
 EDEADLOCK: 35,
 EBFONT: 59,
 ENOSTR: 60,
 ENODATA: 61,
 ETIME: 62,
 ENOSR: 63,
 ENONET: 64,
 ENOPKG: 65,
 EREMOTE: 66,
 ENOLINK: 67,
 EADV: 68,
 ESRMNT: 69,
 ECOMM: 70,
 EPROTO: 71,
 EMULTIHOP: 72,
 EDOTDOT: 73,
 EBADMSG: 74,
 ENOTUNIQ: 76,
 EBADFD: 77,
 EREMCHG: 78,
 ELIBACC: 79,
 ELIBBAD: 80,
 ELIBSCN: 81,
 ELIBMAX: 82,
 ELIBEXEC: 83,
 ENOSYS: 38,
 ENOTEMPTY: 39,
 ENAMETOOLONG: 36,
 ELOOP: 40,
 EOPNOTSUPP: 95,
 EPFNOSUPPORT: 96,
 ECONNRESET: 104,
 ENOBUFS: 105,
 EAFNOSUPPORT: 97,
 EPROTOTYPE: 91,
 ENOTSOCK: 88,
 ENOPROTOOPT: 92,
 ESHUTDOWN: 108,
 ECONNREFUSED: 111,
 EADDRINUSE: 98,
 ECONNABORTED: 103,
 ENETUNREACH: 101,
 ENETDOWN: 100,
 ETIMEDOUT: 110,
 EHOSTDOWN: 112,
 EHOSTUNREACH: 113,
 EINPROGRESS: 115,
 EALREADY: 114,
 EDESTADDRREQ: 89,
 EMSGSIZE: 90,
 EPROTONOSUPPORT: 93,
 ESOCKTNOSUPPORT: 94,
 EADDRNOTAVAIL: 99,
 ENETRESET: 102,
 EISCONN: 106,
 ENOTCONN: 107,
 ETOOMANYREFS: 109,
 EUSERS: 87,
 EDQUOT: 122,
 ESTALE: 116,
 ENOTSUP: 95,
 ENOMEDIUM: 123,
 EILSEQ: 84,
 EOVERFLOW: 75,
 ECANCELED: 125,
 ENOTRECOVERABLE: 131,
 EOWNERDEAD: 130,
 ESTRPIPE: 86
};
function _sysconf(name) {
 switch (name) {
 case 30:
  return PAGE_SIZE;
 case 85:
  return totalMemory / PAGE_SIZE;
 case 132:
 case 133:
 case 12:
 case 137:
 case 138:
 case 15:
 case 235:
 case 16:
 case 17:
 case 18:
 case 19:
 case 20:
 case 149:
 case 13:
 case 10:
 case 236:
 case 153:
 case 9:
 case 21:
 case 22:
 case 159:
 case 154:
 case 14:
 case 77:
 case 78:
 case 139:
 case 80:
 case 81:
 case 82:
 case 68:
 case 67:
 case 164:
 case 11:
 case 29:
 case 47:
 case 48:
 case 95:
 case 52:
 case 51:
 case 46:
  return 200809;
 case 79:
  return 0;
 case 27:
 case 246:
 case 127:
 case 128:
 case 23:
 case 24:
 case 160:
 case 161:
 case 181:
 case 182:
 case 242:
 case 183:
 case 184:
 case 243:
 case 244:
 case 245:
 case 165:
 case 178:
 case 179:
 case 49:
 case 50:
 case 168:
 case 169:
 case 175:
 case 170:
 case 171:
 case 172:
 case 97:
 case 76:
 case 32:
 case 173:
 case 35:
  return -1;
 case 176:
 case 177:
 case 7:
 case 155:
 case 8:
 case 157:
 case 125:
 case 126:
 case 92:
 case 93:
 case 129:
 case 130:
 case 131:
 case 94:
 case 91:
  return 1;
 case 74:
 case 60:
 case 69:
 case 70:
 case 4:
  return 1024;
 case 31:
 case 42:
 case 72:
  return 32;
 case 87:
 case 26:
 case 33:
  return 2147483647;
 case 34:
 case 1:
  return 47839;
 case 38:
 case 36:
  return 99;
 case 43:
 case 37:
  return 2048;
 case 0:
  return 2097152;
 case 3:
  return 65536;
 case 28:
  return 32768;
 case 44:
  return 32767;
 case 75:
  return 16384;
 case 39:
  return 1e3;
 case 89:
  return 700;
 case 71:
  return 256;
 case 40:
  return 255;
 case 2:
  return 100;
 case 180:
  return 64;
 case 25:
  return 20;
 case 5:
  return 16;
 case 6:
  return 6;
 case 73:
  return 4;
 case 84:
  {
   if (typeof navigator === "object") return navigator["hardwareConcurrency"] || 1;
   return 1;
  }
 }
 ___setErrNo(ERRNO_CODES.EINVAL);
 return -1;
}
function _pthread_cleanup_push(routine, arg) {
 __ATEXIT__.push((function() {
  Runtime.dynCall("vi", routine, [ arg ]);
 }));
 _pthread_cleanup_push.level = __ATEXIT__.length;
}
Module["_memset"] = _memset;
var _BDtoILow = true;
Module["_bitshift64Lshr"] = _bitshift64Lshr;
Module["_bitshift64Shl"] = _bitshift64Shl;
function _pthread_cleanup_pop() {
 assert(_pthread_cleanup_push.level == __ATEXIT__.length, "cannot pop if something else added meanwhile!");
 __ATEXIT__.pop();
 _pthread_cleanup_push.level = __ATEXIT__.length;
}
function _abort() {
 Module["abort"]();
}
function ___assert_fail(condition, filename, line, func) {
 ABORT = true;
 throw "Assertion failed: " + Pointer_stringify(condition) + ", at: " + [ filename ? Pointer_stringify(filename) : "unknown filename", line, func ? Pointer_stringify(func) : "unknown function" ] + " at " + stackTrace();
}
function _pthread_self() {
 return 0;
}
function ___lock() {}
function ___unlock() {}
var ERRNO_MESSAGES = {
 0: "Success",
 1: "Not super-user",
 2: "No such file or directory",
 3: "No such process",
 4: "Interrupted system call",
 5: "I/O error",
 6: "No such device or address",
 7: "Arg list too long",
 8: "Exec format error",
 9: "Bad file number",
 10: "No children",
 11: "No more processes",
 12: "Not enough core",
 13: "Permission denied",
 14: "Bad address",
 15: "Block device required",
 16: "Mount device busy",
 17: "File exists",
 18: "Cross-device link",
 19: "No such device",
 20: "Not a directory",
 21: "Is a directory",
 22: "Invalid argument",
 23: "Too many open files in system",
 24: "Too many open files",
 25: "Not a typewriter",
 26: "Text file busy",
 27: "File too large",
 28: "No space left on device",
 29: "Illegal seek",
 30: "Read only file system",
 31: "Too many links",
 32: "Broken pipe",
 33: "Math arg out of domain of func",
 34: "Math result not representable",
 35: "File locking deadlock error",
 36: "File or path name too long",
 37: "No record locks available",
 38: "Function not implemented",
 39: "Directory not empty",
 40: "Too many symbolic links",
 42: "No message of desired type",
 43: "Identifier removed",
 44: "Channel number out of range",
 45: "Level 2 not synchronized",
 46: "Level 3 halted",
 47: "Level 3 reset",
 48: "Link number out of range",
 49: "Protocol driver not attached",
 50: "No CSI structure available",
 51: "Level 2 halted",
 52: "Invalid exchange",
 53: "Invalid request descriptor",
 54: "Exchange full",
 55: "No anode",
 56: "Invalid request code",
 57: "Invalid slot",
 59: "Bad font file fmt",
 60: "Device not a stream",
 61: "No data (for no delay io)",
 62: "Timer expired",
 63: "Out of streams resources",
 64: "Machine is not on the network",
 65: "Package not installed",
 66: "The object is remote",
 67: "The link has been severed",
 68: "Advertise error",
 69: "Srmount error",
 70: "Communication error on send",
 71: "Protocol error",
 72: "Multihop attempted",
 73: "Cross mount point (not really error)",
 74: "Trying to read unreadable message",
 75: "Value too large for defined data type",
 76: "Given log. name not unique",
 77: "f.d. invalid for this operation",
 78: "Remote address changed",
 79: "Can   access a needed shared lib",
 80: "Accessing a corrupted shared lib",
 81: ".lib section in a.out corrupted",
 82: "Attempting to link in too many libs",
 83: "Attempting to exec a shared library",
 84: "Illegal byte sequence",
 86: "Streams pipe error",
 87: "Too many users",
 88: "Socket operation on non-socket",
 89: "Destination address required",
 90: "Message too long",
 91: "Protocol wrong type for socket",
 92: "Protocol not available",
 93: "Unknown protocol",
 94: "Socket type not supported",
 95: "Not supported",
 96: "Protocol family not supported",
 97: "Address family not supported by protocol family",
 98: "Address already in use",
 99: "Address not available",
 100: "Network interface is not configured",
 101: "Network is unreachable",
 102: "Connection reset by network",
 103: "Connection aborted",
 104: "Connection reset by peer",
 105: "No buffer space available",
 106: "Socket is already connected",
 107: "Socket is not connected",
 108: "Can't send after socket shutdown",
 109: "Too many references",
 110: "Connection timed out",
 111: "Connection refused",
 112: "Host is down",
 113: "Host is unreachable",
 114: "Socket already connected",
 115: "Connection already in progress",
 116: "Stale file handle",
 122: "Quota exceeded",
 123: "No medium (in tape drive)",
 125: "Operation canceled",
 130: "Previous owner died",
 131: "State not recoverable"
};
var PATH = {
 splitPath: (function(filename) {
  var splitPathRe = /^(\/?|)([\s\S]*?)((?:\.{1,2}|[^\/]+?|)(\.[^.\/]*|))(?:[\/]*)$/;
  return splitPathRe.exec(filename).slice(1);
 }),
 normalizeArray: (function(parts, allowAboveRoot) {
  var up = 0;
  for (var i = parts.length - 1; i >= 0; i--) {
   var last = parts[i];
   if (last === ".") {
    parts.splice(i, 1);
   } else if (last === "..") {
    parts.splice(i, 1);
    up++;
   } else if (up) {
    parts.splice(i, 1);
    up--;
   }
  }
  if (allowAboveRoot) {
   for (; up--; up) {
    parts.unshift("..");
   }
  }
  return parts;
 }),
 normalize: (function(path) {
  var isAbsolute = path.charAt(0) === "/", trailingSlash = path.substr(-1) === "/";
  path = PATH.normalizeArray(path.split("/").filter((function(p) {
   return !!p;
  })), !isAbsolute).join("/");
  if (!path && !isAbsolute) {
   path = ".";
  }
  if (path && trailingSlash) {
   path += "/";
  }
  return (isAbsolute ? "/" : "") + path;
 }),
 dirname: (function(path) {
  var result = PATH.splitPath(path), root = result[0], dir = result[1];
  if (!root && !dir) {
   return ".";
  }
  if (dir) {
   dir = dir.substr(0, dir.length - 1);
  }
  return root + dir;
 }),
 basename: (function(path) {
  if (path === "/") return "/";
  var lastSlash = path.lastIndexOf("/");
  if (lastSlash === -1) return path;
  return path.substr(lastSlash + 1);
 }),
 extname: (function(path) {
  return PATH.splitPath(path)[3];
 }),
 join: (function() {
  var paths = Array.prototype.slice.call(arguments, 0);
  return PATH.normalize(paths.join("/"));
 }),
 join2: (function(l, r) {
  return PATH.normalize(l + "/" + r);
 }),
 resolve: (function() {
  var resolvedPath = "", resolvedAbsolute = false;
  for (var i = arguments.length - 1; i >= -1 && !resolvedAbsolute; i--) {
   var path = i >= 0 ? arguments[i] : FS.cwd();
   if (typeof path !== "string") {
    throw new TypeError("Arguments to path.resolve must be strings");
   } else if (!path) {
    return "";
   }
   resolvedPath = path + "/" + resolvedPath;
   resolvedAbsolute = path.charAt(0) === "/";
  }
  resolvedPath = PATH.normalizeArray(resolvedPath.split("/").filter((function(p) {
   return !!p;
  })), !resolvedAbsolute).join("/");
  return (resolvedAbsolute ? "/" : "") + resolvedPath || ".";
 }),
 relative: (function(from, to) {
  from = PATH.resolve(from).substr(1);
  to = PATH.resolve(to).substr(1);
  function trim(arr) {
   var start = 0;
   for (; start < arr.length; start++) {
    if (arr[start] !== "") break;
   }
   var end = arr.length - 1;
   for (; end >= 0; end--) {
    if (arr[end] !== "") break;
   }
   if (start > end) return [];
   return arr.slice(start, end - start + 1);
  }
  var fromParts = trim(from.split("/"));
  var toParts = trim(to.split("/"));
  var length = Math.min(fromParts.length, toParts.length);
  var samePartsLength = length;
  for (var i = 0; i < length; i++) {
   if (fromParts[i] !== toParts[i]) {
    samePartsLength = i;
    break;
   }
  }
  var outputParts = [];
  for (var i = samePartsLength; i < fromParts.length; i++) {
   outputParts.push("..");
  }
  outputParts = outputParts.concat(toParts.slice(samePartsLength));
  return outputParts.join("/");
 })
};
var TTY = {
 ttys: [],
 init: (function() {}),
 shutdown: (function() {}),
 register: (function(dev, ops) {
  TTY.ttys[dev] = {
   input: [],
   output: [],
   ops: ops
  };
  FS.registerDevice(dev, TTY.stream_ops);
 }),
 stream_ops: {
  open: (function(stream) {
   var tty = TTY.ttys[stream.node.rdev];
   if (!tty) {
    throw new FS.ErrnoError(ERRNO_CODES.ENODEV);
   }
   stream.tty = tty;
   stream.seekable = false;
  }),
  close: (function(stream) {
   stream.tty.ops.flush(stream.tty);
  }),
  flush: (function(stream) {
   stream.tty.ops.flush(stream.tty);
  }),
  read: (function(stream, buffer, offset, length, pos) {
   if (!stream.tty || !stream.tty.ops.get_char) {
    throw new FS.ErrnoError(ERRNO_CODES.ENXIO);
   }
   var bytesRead = 0;
   for (var i = 0; i < length; i++) {
    var result;
    try {
     result = stream.tty.ops.get_char(stream.tty);
    } catch (e) {
     throw new FS.ErrnoError(ERRNO_CODES.EIO);
    }
    if (result === undefined && bytesRead === 0) {
     throw new FS.ErrnoError(ERRNO_CODES.EAGAIN);
    }
    if (result === null || result === undefined) break;
    bytesRead++;
    buffer[offset + i] = result;
   }
   if (bytesRead) {
    stream.node.timestamp = Date.now();
   }
   return bytesRead;
  }),
  write: (function(stream, buffer, offset, length, pos) {
   if (!stream.tty || !stream.tty.ops.put_char) {
    throw new FS.ErrnoError(ERRNO_CODES.ENXIO);
   }
   for (var i = 0; i < length; i++) {
    try {
     stream.tty.ops.put_char(stream.tty, buffer[offset + i]);
    } catch (e) {
     throw new FS.ErrnoError(ERRNO_CODES.EIO);
    }
   }
   if (length) {
    stream.node.timestamp = Date.now();
   }
   return i;
  })
 },
 default_tty_ops: {
  get_char: (function(tty) {
   if (!tty.input.length) {
    var result = null;
    if (ENVIRONMENT_IS_NODE) {
     var BUFSIZE = 256;
     var buf = new Buffer(BUFSIZE);
     var bytesRead = 0;
     var fd = process.stdin.fd;
     var usingDevice = false;
     try {
      fd = fs.openSync("/dev/stdin", "r");
      usingDevice = true;
     } catch (e) {}
     bytesRead = fs.readSync(fd, buf, 0, BUFSIZE, null);
     if (usingDevice) {
      fs.closeSync(fd);
     }
     if (bytesRead > 0) {
      result = buf.slice(0, bytesRead).toString("utf-8");
     } else {
      result = null;
     }
    } else if (typeof window != "undefined" && typeof window.prompt == "function") {
     result = window.prompt("Input: ");
     if (result !== null) {
      result += "\n";
     }
    } else if (typeof readline == "function") {
     result = readline();
     if (result !== null) {
      result += "\n";
     }
    }
    if (!result) {
     return null;
    }
    tty.input = intArrayFromString(result, true);
   }
   return tty.input.shift();
  }),
  put_char: (function(tty, val) {
   if (val === null || val === 10) {
    Module["print"](UTF8ArrayToString(tty.output, 0));
    tty.output = [];
   } else {
    if (val != 0) tty.output.push(val);
   }
  }),
  flush: (function(tty) {
   if (tty.output && tty.output.length > 0) {
    Module["print"](UTF8ArrayToString(tty.output, 0));
    tty.output = [];
   }
  })
 },
 default_tty1_ops: {
  put_char: (function(tty, val) {
   if (val === null || val === 10) {
    Module["printErr"](UTF8ArrayToString(tty.output, 0));
    tty.output = [];
   } else {
    if (val != 0) tty.output.push(val);
   }
  }),
  flush: (function(tty) {
   if (tty.output && tty.output.length > 0) {
    Module["printErr"](UTF8ArrayToString(tty.output, 0));
    tty.output = [];
   }
  })
 }
};
var MEMFS = {
 ops_table: null,
 mount: (function(mount) {
  return MEMFS.createNode(null, "/", 16384 | 511, 0);
 }),
 createNode: (function(parent, name, mode, dev) {
  if (FS.isBlkdev(mode) || FS.isFIFO(mode)) {
   throw new FS.ErrnoError(ERRNO_CODES.EPERM);
  }
  if (!MEMFS.ops_table) {
   MEMFS.ops_table = {
    dir: {
     node: {
      getattr: MEMFS.node_ops.getattr,
      setattr: MEMFS.node_ops.setattr,
      lookup: MEMFS.node_ops.lookup,
      mknod: MEMFS.node_ops.mknod,
      rename: MEMFS.node_ops.rename,
      unlink: MEMFS.node_ops.unlink,
      rmdir: MEMFS.node_ops.rmdir,
      readdir: MEMFS.node_ops.readdir,
      symlink: MEMFS.node_ops.symlink
     },
     stream: {
      llseek: MEMFS.stream_ops.llseek
     }
    },
    file: {
     node: {
      getattr: MEMFS.node_ops.getattr,
      setattr: MEMFS.node_ops.setattr
     },
     stream: {
      llseek: MEMFS.stream_ops.llseek,
      read: MEMFS.stream_ops.read,
      write: MEMFS.stream_ops.write,
      allocate: MEMFS.stream_ops.allocate,
      mmap: MEMFS.stream_ops.mmap,
      msync: MEMFS.stream_ops.msync
     }
    },
    link: {
     node: {
      getattr: MEMFS.node_ops.getattr,
      setattr: MEMFS.node_ops.setattr,
      readlink: MEMFS.node_ops.readlink
     },
     stream: {}
    },
    chrdev: {
     node: {
      getattr: MEMFS.node_ops.getattr,
      setattr: MEMFS.node_ops.setattr
     },
     stream: FS.chrdev_stream_ops
    }
   };
  }
  var node = FS.createNode(parent, name, mode, dev);
  if (FS.isDir(node.mode)) {
   node.node_ops = MEMFS.ops_table.dir.node;
   node.stream_ops = MEMFS.ops_table.dir.stream;
   node.contents = {};
  } else if (FS.isFile(node.mode)) {
   node.node_ops = MEMFS.ops_table.file.node;
   node.stream_ops = MEMFS.ops_table.file.stream;
   node.usedBytes = 0;
   node.contents = null;
  } else if (FS.isLink(node.mode)) {
   node.node_ops = MEMFS.ops_table.link.node;
   node.stream_ops = MEMFS.ops_table.link.stream;
  } else if (FS.isChrdev(node.mode)) {
   node.node_ops = MEMFS.ops_table.chrdev.node;
   node.stream_ops = MEMFS.ops_table.chrdev.stream;
  }
  node.timestamp = Date.now();
  if (parent) {
   parent.contents[name] = node;
  }
  return node;
 }),
 getFileDataAsRegularArray: (function(node) {
  if (node.contents && node.contents.subarray) {
   var arr = [];
   for (var i = 0; i < node.usedBytes; ++i) arr.push(node.contents[i]);
   return arr;
  }
  return node.contents;
 }),
 getFileDataAsTypedArray: (function(node) {
  if (!node.contents) return new Uint8Array;
  if (node.contents.subarray) return node.contents.subarray(0, node.usedBytes);
  return new Uint8Array(node.contents);
 }),
 expandFileStorage: (function(node, newCapacity) {
  if (node.contents && node.contents.subarray && newCapacity > node.contents.length) {
   node.contents = MEMFS.getFileDataAsRegularArray(node);
   node.usedBytes = node.contents.length;
  }
  if (!node.contents || node.contents.subarray) {
   var prevCapacity = node.contents ? node.contents.buffer.byteLength : 0;
   if (prevCapacity >= newCapacity) return;
   var CAPACITY_DOUBLING_MAX = 1024 * 1024;
   newCapacity = Math.max(newCapacity, prevCapacity * (prevCapacity < CAPACITY_DOUBLING_MAX ? 2 : 1.125) | 0);
   if (prevCapacity != 0) newCapacity = Math.max(newCapacity, 256);
   var oldContents = node.contents;
   node.contents = new Uint8Array(newCapacity);
   if (node.usedBytes > 0) node.contents.set(oldContents.subarray(0, node.usedBytes), 0);
   return;
  }
  if (!node.contents && newCapacity > 0) node.contents = [];
  while (node.contents.length < newCapacity) node.contents.push(0);
 }),
 resizeFileStorage: (function(node, newSize) {
  if (node.usedBytes == newSize) return;
  if (newSize == 0) {
   node.contents = null;
   node.usedBytes = 0;
   return;
  }
  if (!node.contents || node.contents.subarray) {
   var oldContents = node.contents;
   node.contents = new Uint8Array(new ArrayBuffer(newSize));
   if (oldContents) {
    node.contents.set(oldContents.subarray(0, Math.min(newSize, node.usedBytes)));
   }
   node.usedBytes = newSize;
   return;
  }
  if (!node.contents) node.contents = [];
  if (node.contents.length > newSize) node.contents.length = newSize; else while (node.contents.length < newSize) node.contents.push(0);
  node.usedBytes = newSize;
 }),
 node_ops: {
  getattr: (function(node) {
   var attr = {};
   attr.dev = FS.isChrdev(node.mode) ? node.id : 1;
   attr.ino = node.id;
   attr.mode = node.mode;
   attr.nlink = 1;
   attr.uid = 0;
   attr.gid = 0;
   attr.rdev = node.rdev;
   if (FS.isDir(node.mode)) {
    attr.size = 4096;
   } else if (FS.isFile(node.mode)) {
    attr.size = node.usedBytes;
   } else if (FS.isLink(node.mode)) {
    attr.size = node.link.length;
   } else {
    attr.size = 0;
   }
   attr.atime = new Date(node.timestamp);
   attr.mtime = new Date(node.timestamp);
   attr.ctime = new Date(node.timestamp);
   attr.blksize = 4096;
   attr.blocks = Math.ceil(attr.size / attr.blksize);
   return attr;
  }),
  setattr: (function(node, attr) {
   if (attr.mode !== undefined) {
    node.mode = attr.mode;
   }
   if (attr.timestamp !== undefined) {
    node.timestamp = attr.timestamp;
   }
   if (attr.size !== undefined) {
    MEMFS.resizeFileStorage(node, attr.size);
   }
  }),
  lookup: (function(parent, name) {
   throw FS.genericErrors[ERRNO_CODES.ENOENT];
  }),
  mknod: (function(parent, name, mode, dev) {
   return MEMFS.createNode(parent, name, mode, dev);
  }),
  rename: (function(old_node, new_dir, new_name) {
   if (FS.isDir(old_node.mode)) {
    var new_node;
    try {
     new_node = FS.lookupNode(new_dir, new_name);
    } catch (e) {}
    if (new_node) {
     for (var i in new_node.contents) {
      throw new FS.ErrnoError(ERRNO_CODES.ENOTEMPTY);
     }
    }
   }
   delete old_node.parent.contents[old_node.name];
   old_node.name = new_name;
   new_dir.contents[new_name] = old_node;
   old_node.parent = new_dir;
  }),
  unlink: (function(parent, name) {
   delete parent.contents[name];
  }),
  rmdir: (function(parent, name) {
   var node = FS.lookupNode(parent, name);
   for (var i in node.contents) {
    throw new FS.ErrnoError(ERRNO_CODES.ENOTEMPTY);
   }
   delete parent.contents[name];
  }),
  readdir: (function(node) {
   var entries = [ ".", ".." ];
   for (var key in node.contents) {
    if (!node.contents.hasOwnProperty(key)) {
     continue;
    }
    entries.push(key);
   }
   return entries;
  }),
  symlink: (function(parent, newname, oldpath) {
   var node = MEMFS.createNode(parent, newname, 511 | 40960, 0);
   node.link = oldpath;
   return node;
  }),
  readlink: (function(node) {
   if (!FS.isLink(node.mode)) {
    throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
   }
   return node.link;
  })
 },
 stream_ops: {
  read: (function(stream, buffer, offset, length, position) {
   var contents = stream.node.contents;
   if (position >= stream.node.usedBytes) return 0;
   var size = Math.min(stream.node.usedBytes - position, length);
   assert(size >= 0);
   if (size > 8 && contents.subarray) {
    buffer.set(contents.subarray(position, position + size), offset);
   } else {
    for (var i = 0; i < size; i++) buffer[offset + i] = contents[position + i];
   }
   return size;
  }),
  write: (function(stream, buffer, offset, length, position, canOwn) {
   if (!length) return 0;
   var node = stream.node;
   node.timestamp = Date.now();
   if (buffer.subarray && (!node.contents || node.contents.subarray)) {
    if (canOwn) {
     node.contents = buffer.subarray(offset, offset + length);
     node.usedBytes = length;
     return length;
    } else if (node.usedBytes === 0 && position === 0) {
     node.contents = new Uint8Array(buffer.subarray(offset, offset + length));
     node.usedBytes = length;
     return length;
    } else if (position + length <= node.usedBytes) {
     node.contents.set(buffer.subarray(offset, offset + length), position);
     return length;
    }
   }
   MEMFS.expandFileStorage(node, position + length);
   if (node.contents.subarray && buffer.subarray) node.contents.set(buffer.subarray(offset, offset + length), position); else {
    for (var i = 0; i < length; i++) {
     node.contents[position + i] = buffer[offset + i];
    }
   }
   node.usedBytes = Math.max(node.usedBytes, position + length);
   return length;
  }),
  llseek: (function(stream, offset, whence) {
   var position = offset;
   if (whence === 1) {
    position += stream.position;
   } else if (whence === 2) {
    if (FS.isFile(stream.node.mode)) {
     position += stream.node.usedBytes;
    }
   }
   if (position < 0) {
    throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
   }
   return position;
  }),
  allocate: (function(stream, offset, length) {
   MEMFS.expandFileStorage(stream.node, offset + length);
   stream.node.usedBytes = Math.max(stream.node.usedBytes, offset + length);
  }),
  mmap: (function(stream, buffer, offset, length, position, prot, flags) {
   if (!FS.isFile(stream.node.mode)) {
    throw new FS.ErrnoError(ERRNO_CODES.ENODEV);
   }
   var ptr;
   var allocated;
   var contents = stream.node.contents;
   if (!(flags & 2) && (contents.buffer === buffer || contents.buffer === buffer.buffer)) {
    allocated = false;
    ptr = contents.byteOffset;
   } else {
    if (position > 0 || position + length < stream.node.usedBytes) {
     if (contents.subarray) {
      contents = contents.subarray(position, position + length);
     } else {
      contents = Array.prototype.slice.call(contents, position, position + length);
     }
    }
    allocated = true;
    ptr = _malloc(length);
    if (!ptr) {
     throw new FS.ErrnoError(ERRNO_CODES.ENOMEM);
    }
    buffer.set(contents, ptr);
   }
   return {
    ptr: ptr,
    allocated: allocated
   };
  }),
  msync: (function(stream, buffer, offset, length, mmapFlags) {
   if (!FS.isFile(stream.node.mode)) {
    throw new FS.ErrnoError(ERRNO_CODES.ENODEV);
   }
   if (mmapFlags & 2) {
    return 0;
   }
   var bytesWritten = MEMFS.stream_ops.write(stream, buffer, 0, length, offset, false);
   return 0;
  })
 }
};
var IDBFS = {
 dbs: {},
 indexedDB: (function() {
  if (typeof indexedDB !== "undefined") return indexedDB;
  var ret = null;
  if (typeof window === "object") ret = window.indexedDB || window.mozIndexedDB || window.webkitIndexedDB || window.msIndexedDB;
  assert(ret, "IDBFS used, but indexedDB not supported");
  return ret;
 }),
 DB_VERSION: 21,
 DB_STORE_NAME: "FILE_DATA",
 mount: (function(mount) {
  return MEMFS.mount.apply(null, arguments);
 }),
 syncfs: (function(mount, populate, callback) {
  IDBFS.getLocalSet(mount, (function(err, local) {
   if (err) return callback(err);
   IDBFS.getRemoteSet(mount, (function(err, remote) {
    if (err) return callback(err);
    var src = populate ? remote : local;
    var dst = populate ? local : remote;
    IDBFS.reconcile(src, dst, callback);
   }));
  }));
 }),
 getDB: (function(name, callback) {
  var db = IDBFS.dbs[name];
  if (db) {
   return callback(null, db);
  }
  var req;
  try {
   req = IDBFS.indexedDB().open(name, IDBFS.DB_VERSION);
  } catch (e) {
   return callback(e);
  }
  req.onupgradeneeded = (function(e) {
   var db = e.target.result;
   var transaction = e.target.transaction;
   var fileStore;
   if (db.objectStoreNames.contains(IDBFS.DB_STORE_NAME)) {
    fileStore = transaction.objectStore(IDBFS.DB_STORE_NAME);
   } else {
    fileStore = db.createObjectStore(IDBFS.DB_STORE_NAME);
   }
   if (!fileStore.indexNames.contains("timestamp")) {
    fileStore.createIndex("timestamp", "timestamp", {
     unique: false
    });
   }
  });
  req.onsuccess = (function() {
   db = req.result;
   IDBFS.dbs[name] = db;
   callback(null, db);
  });
  req.onerror = (function(e) {
   callback(this.error);
   e.preventDefault();
  });
 }),
 getLocalSet: (function(mount, callback) {
  var entries = {};
  function isRealDir(p) {
   return p !== "." && p !== "..";
  }
  function toAbsolute(root) {
   return (function(p) {
    return PATH.join2(root, p);
   });
  }
  var check = FS.readdir(mount.mountpoint).filter(isRealDir).map(toAbsolute(mount.mountpoint));
  while (check.length) {
   var path = check.pop();
   var stat;
   try {
    stat = FS.stat(path);
   } catch (e) {
    return callback(e);
   }
   if (FS.isDir(stat.mode)) {
    check.push.apply(check, FS.readdir(path).filter(isRealDir).map(toAbsolute(path)));
   }
   entries[path] = {
    timestamp: stat.mtime
   };
  }
  return callback(null, {
   type: "local",
   entries: entries
  });
 }),
 getRemoteSet: (function(mount, callback) {
  var entries = {};
  IDBFS.getDB(mount.mountpoint, (function(err, db) {
   if (err) return callback(err);
   var transaction = db.transaction([ IDBFS.DB_STORE_NAME ], "readonly");
   transaction.onerror = (function(e) {
    callback(this.error);
    e.preventDefault();
   });
   var store = transaction.objectStore(IDBFS.DB_STORE_NAME);
   var index = store.index("timestamp");
   index.openKeyCursor().onsuccess = (function(event) {
    var cursor = event.target.result;
    if (!cursor) {
     return callback(null, {
      type: "remote",
      db: db,
      entries: entries
     });
    }
    entries[cursor.primaryKey] = {
     timestamp: cursor.key
    };
    cursor.continue();
   });
  }));
 }),
 loadLocalEntry: (function(path, callback) {
  var stat, node;
  try {
   var lookup = FS.lookupPath(path);
   node = lookup.node;
   stat = FS.stat(path);
  } catch (e) {
   return callback(e);
  }
  if (FS.isDir(stat.mode)) {
   return callback(null, {
    timestamp: stat.mtime,
    mode: stat.mode
   });
  } else if (FS.isFile(stat.mode)) {
   node.contents = MEMFS.getFileDataAsTypedArray(node);
   return callback(null, {
    timestamp: stat.mtime,
    mode: stat.mode,
    contents: node.contents
   });
  } else {
   return callback(new Error("node type not supported"));
  }
 }),
 storeLocalEntry: (function(path, entry, callback) {
  try {
   if (FS.isDir(entry.mode)) {
    FS.mkdir(path, entry.mode);
   } else if (FS.isFile(entry.mode)) {
    FS.writeFile(path, entry.contents, {
     encoding: "binary",
     canOwn: true
    });
   } else {
    return callback(new Error("node type not supported"));
   }
   FS.chmod(path, entry.mode);
   FS.utime(path, entry.timestamp, entry.timestamp);
  } catch (e) {
   return callback(e);
  }
  callback(null);
 }),
 removeLocalEntry: (function(path, callback) {
  try {
   var lookup = FS.lookupPath(path);
   var stat = FS.stat(path);
   if (FS.isDir(stat.mode)) {
    FS.rmdir(path);
   } else if (FS.isFile(stat.mode)) {
    FS.unlink(path);
   }
  } catch (e) {
   return callback(e);
  }
  callback(null);
 }),
 loadRemoteEntry: (function(store, path, callback) {
  var req = store.get(path);
  req.onsuccess = (function(event) {
   callback(null, event.target.result);
  });
  req.onerror = (function(e) {
   callback(this.error);
   e.preventDefault();
  });
 }),
 storeRemoteEntry: (function(store, path, entry, callback) {
  var req = store.put(entry, path);
  req.onsuccess = (function() {
   callback(null);
  });
  req.onerror = (function(e) {
   callback(this.error);
   e.preventDefault();
  });
 }),
 removeRemoteEntry: (function(store, path, callback) {
  var req = store.delete(path);
  req.onsuccess = (function() {
   callback(null);
  });
  req.onerror = (function(e) {
   callback(this.error);
   e.preventDefault();
  });
 }),
 reconcile: (function(src, dst, callback) {
  var total = 0;
  var create = [];
  Object.keys(src.entries).forEach((function(key) {
   var e = src.entries[key];
   var e2 = dst.entries[key];
   if (!e2 || e.timestamp > e2.timestamp) {
    create.push(key);
    total++;
   }
  }));
  var remove = [];
  Object.keys(dst.entries).forEach((function(key) {
   var e = dst.entries[key];
   var e2 = src.entries[key];
   if (!e2) {
    remove.push(key);
    total++;
   }
  }));
  if (!total) {
   return callback(null);
  }
  var errored = false;
  var completed = 0;
  var db = src.type === "remote" ? src.db : dst.db;
  var transaction = db.transaction([ IDBFS.DB_STORE_NAME ], "readwrite");
  var store = transaction.objectStore(IDBFS.DB_STORE_NAME);
  function done(err) {
   if (err) {
    if (!done.errored) {
     done.errored = true;
     return callback(err);
    }
    return;
   }
   if (++completed >= total) {
    return callback(null);
   }
  }
  transaction.onerror = (function(e) {
   done(this.error);
   e.preventDefault();
  });
  create.sort().forEach((function(path) {
   if (dst.type === "local") {
    IDBFS.loadRemoteEntry(store, path, (function(err, entry) {
     if (err) return done(err);
     IDBFS.storeLocalEntry(path, entry, done);
    }));
   } else {
    IDBFS.loadLocalEntry(path, (function(err, entry) {
     if (err) return done(err);
     IDBFS.storeRemoteEntry(store, path, entry, done);
    }));
   }
  }));
  remove.sort().reverse().forEach((function(path) {
   if (dst.type === "local") {
    IDBFS.removeLocalEntry(path, done);
   } else {
    IDBFS.removeRemoteEntry(store, path, done);
   }
  }));
 })
};
var NODEFS = {
 isWindows: false,
 staticInit: (function() {
  NODEFS.isWindows = !!process.platform.match(/^win/);
 }),
 mount: (function(mount) {
  assert(ENVIRONMENT_IS_NODE);
  return NODEFS.createNode(null, "/", NODEFS.getMode(mount.opts.root), 0);
 }),
 createNode: (function(parent, name, mode, dev) {
  if (!FS.isDir(mode) && !FS.isFile(mode) && !FS.isLink(mode)) {
   throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
  }
  var node = FS.createNode(parent, name, mode);
  node.node_ops = NODEFS.node_ops;
  node.stream_ops = NODEFS.stream_ops;
  return node;
 }),
 getMode: (function(path) {
  var stat;
  try {
   stat = fs.lstatSync(path);
   if (NODEFS.isWindows) {
    stat.mode = stat.mode | (stat.mode & 146) >> 1;
   }
  } catch (e) {
   if (!e.code) throw e;
   throw new FS.ErrnoError(ERRNO_CODES[e.code]);
  }
  return stat.mode;
 }),
 realPath: (function(node) {
  var parts = [];
  while (node.parent !== node) {
   parts.push(node.name);
   node = node.parent;
  }
  parts.push(node.mount.opts.root);
  parts.reverse();
  return PATH.join.apply(null, parts);
 }),
 flagsToPermissionStringMap: {
  0: "r",
  1: "r+",
  2: "r+",
  64: "r",
  65: "r+",
  66: "r+",
  129: "rx+",
  193: "rx+",
  514: "w+",
  577: "w",
  578: "w+",
  705: "wx",
  706: "wx+",
  1024: "a",
  1025: "a",
  1026: "a+",
  1089: "a",
  1090: "a+",
  1153: "ax",
  1154: "ax+",
  1217: "ax",
  1218: "ax+",
  4096: "rs",
  4098: "rs+"
 },
 flagsToPermissionString: (function(flags) {
  flags &= ~32768;
  if (flags in NODEFS.flagsToPermissionStringMap) {
   return NODEFS.flagsToPermissionStringMap[flags];
  } else {
   throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
  }
 }),
 node_ops: {
  getattr: (function(node) {
   var path = NODEFS.realPath(node);
   var stat;
   try {
    stat = fs.lstatSync(path);
   } catch (e) {
    if (!e.code) throw e;
    throw new FS.ErrnoError(ERRNO_CODES[e.code]);
   }
   if (NODEFS.isWindows && !stat.blksize) {
    stat.blksize = 4096;
   }
   if (NODEFS.isWindows && !stat.blocks) {
    stat.blocks = (stat.size + stat.blksize - 1) / stat.blksize | 0;
   }
   return {
    dev: stat.dev,
    ino: stat.ino,
    mode: stat.mode,
    nlink: stat.nlink,
    uid: stat.uid,
    gid: stat.gid,
    rdev: stat.rdev,
    size: stat.size,
    atime: stat.atime,
    mtime: stat.mtime,
    ctime: stat.ctime,
    blksize: stat.blksize,
    blocks: stat.blocks
   };
  }),
  setattr: (function(node, attr) {
   var path = NODEFS.realPath(node);
   try {
    if (attr.mode !== undefined) {
     fs.chmodSync(path, attr.mode);
     node.mode = attr.mode;
    }
    if (attr.timestamp !== undefined) {
     var date = new Date(attr.timestamp);
     fs.utimesSync(path, date, date);
    }
    if (attr.size !== undefined) {
     fs.truncateSync(path, attr.size);
    }
   } catch (e) {
    if (!e.code) throw e;
    throw new FS.ErrnoError(ERRNO_CODES[e.code]);
   }
  }),
  lookup: (function(parent, name) {
   var path = PATH.join2(NODEFS.realPath(parent), name);
   var mode = NODEFS.getMode(path);
   return NODEFS.createNode(parent, name, mode);
  }),
  mknod: (function(parent, name, mode, dev) {
   var node = NODEFS.createNode(parent, name, mode, dev);
   var path = NODEFS.realPath(node);
   try {
    if (FS.isDir(node.mode)) {
     fs.mkdirSync(path, node.mode);
    } else {
     fs.writeFileSync(path, "", {
      mode: node.mode
     });
    }
   } catch (e) {
    if (!e.code) throw e;
    throw new FS.ErrnoError(ERRNO_CODES[e.code]);
   }
   return node;
  }),
  rename: (function(oldNode, newDir, newName) {
   var oldPath = NODEFS.realPath(oldNode);
   var newPath = PATH.join2(NODEFS.realPath(newDir), newName);
   try {
    fs.renameSync(oldPath, newPath);
   } catch (e) {
    if (!e.code) throw e;
    throw new FS.ErrnoError(ERRNO_CODES[e.code]);
   }
  }),
  unlink: (function(parent, name) {
   var path = PATH.join2(NODEFS.realPath(parent), name);
   try {
    fs.unlinkSync(path);
   } catch (e) {
    if (!e.code) throw e;
    throw new FS.ErrnoError(ERRNO_CODES[e.code]);
   }
  }),
  rmdir: (function(parent, name) {
   var path = PATH.join2(NODEFS.realPath(parent), name);
   try {
    fs.rmdirSync(path);
   } catch (e) {
    if (!e.code) throw e;
    throw new FS.ErrnoError(ERRNO_CODES[e.code]);
   }
  }),
  readdir: (function(node) {
   var path = NODEFS.realPath(node);
   try {
    return fs.readdirSync(path);
   } catch (e) {
    if (!e.code) throw e;
    throw new FS.ErrnoError(ERRNO_CODES[e.code]);
   }
  }),
  symlink: (function(parent, newName, oldPath) {
   var newPath = PATH.join2(NODEFS.realPath(parent), newName);
   try {
    fs.symlinkSync(oldPath, newPath);
   } catch (e) {
    if (!e.code) throw e;
    throw new FS.ErrnoError(ERRNO_CODES[e.code]);
   }
  }),
  readlink: (function(node) {
   var path = NODEFS.realPath(node);
   try {
    path = fs.readlinkSync(path);
    path = NODEJS_PATH.relative(NODEJS_PATH.resolve(node.mount.opts.root), path);
    return path;
   } catch (e) {
    if (!e.code) throw e;
    throw new FS.ErrnoError(ERRNO_CODES[e.code]);
   }
  })
 },
 stream_ops: {
  open: (function(stream) {
   var path = NODEFS.realPath(stream.node);
   try {
    if (FS.isFile(stream.node.mode)) {
     stream.nfd = fs.openSync(path, NODEFS.flagsToPermissionString(stream.flags));
    }
   } catch (e) {
    if (!e.code) throw e;
    throw new FS.ErrnoError(ERRNO_CODES[e.code]);
   }
  }),
  close: (function(stream) {
   try {
    if (FS.isFile(stream.node.mode) && stream.nfd) {
     fs.closeSync(stream.nfd);
    }
   } catch (e) {
    if (!e.code) throw e;
    throw new FS.ErrnoError(ERRNO_CODES[e.code]);
   }
  }),
  read: (function(stream, buffer, offset, length, position) {
   if (length === 0) return 0;
   var nbuffer = new Buffer(length);
   var res;
   try {
    res = fs.readSync(stream.nfd, nbuffer, 0, length, position);
   } catch (e) {
    throw new FS.ErrnoError(ERRNO_CODES[e.code]);
   }
   if (res > 0) {
    for (var i = 0; i < res; i++) {
     buffer[offset + i] = nbuffer[i];
    }
   }
   return res;
  }),
  write: (function(stream, buffer, offset, length, position) {
   var nbuffer = new Buffer(buffer.subarray(offset, offset + length));
   var res;
   try {
    res = fs.writeSync(stream.nfd, nbuffer, 0, length, position);
   } catch (e) {
    throw new FS.ErrnoError(ERRNO_CODES[e.code]);
   }
   return res;
  }),
  llseek: (function(stream, offset, whence) {
   var position = offset;
   if (whence === 1) {
    position += stream.position;
   } else if (whence === 2) {
    if (FS.isFile(stream.node.mode)) {
     try {
      var stat = fs.fstatSync(stream.nfd);
      position += stat.size;
     } catch (e) {
      throw new FS.ErrnoError(ERRNO_CODES[e.code]);
     }
    }
   }
   if (position < 0) {
    throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
   }
   return position;
  })
 }
};
var WORKERFS = {
 DIR_MODE: 16895,
 FILE_MODE: 33279,
 reader: null,
 mount: (function(mount) {
  assert(ENVIRONMENT_IS_WORKER);
  if (!WORKERFS.reader) WORKERFS.reader = new FileReaderSync;
  var root = WORKERFS.createNode(null, "/", WORKERFS.DIR_MODE, 0);
  var createdParents = {};
  function ensureParent(path) {
   var parts = path.split("/");
   var parent = root;
   for (var i = 0; i < parts.length - 1; i++) {
    var curr = parts.slice(0, i + 1).join("/");
    if (!createdParents[curr]) {
     createdParents[curr] = WORKERFS.createNode(parent, curr, WORKERFS.DIR_MODE, 0);
    }
    parent = createdParents[curr];
   }
   return parent;
  }
  function base(path) {
   var parts = path.split("/");
   return parts[parts.length - 1];
  }
  Array.prototype.forEach.call(mount.opts["files"] || [], (function(file) {
   WORKERFS.createNode(ensureParent(file.name), base(file.name), WORKERFS.FILE_MODE, 0, file, file.lastModifiedDate);
  }));
  (mount.opts["blobs"] || []).forEach((function(obj) {
   WORKERFS.createNode(ensureParent(obj["name"]), base(obj["name"]), WORKERFS.FILE_MODE, 0, obj["data"]);
  }));
  (mount.opts["packages"] || []).forEach((function(pack) {
   pack["metadata"].files.forEach((function(file) {
    var name = file.filename.substr(1);
    WORKERFS.createNode(ensureParent(name), base(name), WORKERFS.FILE_MODE, 0, pack["blob"].slice(file.start, file.end));
   }));
  }));
  return root;
 }),
 createNode: (function(parent, name, mode, dev, contents, mtime) {
  var node = FS.createNode(parent, name, mode);
  node.mode = mode;
  node.node_ops = WORKERFS.node_ops;
  node.stream_ops = WORKERFS.stream_ops;
  node.timestamp = (mtime || new Date).getTime();
  assert(WORKERFS.FILE_MODE !== WORKERFS.DIR_MODE);
  if (mode === WORKERFS.FILE_MODE) {
   node.size = contents.size;
   node.contents = contents;
  } else {
   node.size = 4096;
   node.contents = {};
  }
  if (parent) {
   parent.contents[name] = node;
  }
  return node;
 }),
 node_ops: {
  getattr: (function(node) {
   return {
    dev: 1,
    ino: undefined,
    mode: node.mode,
    nlink: 1,
    uid: 0,
    gid: 0,
    rdev: undefined,
    size: node.size,
    atime: new Date(node.timestamp),
    mtime: new Date(node.timestamp),
    ctime: new Date(node.timestamp),
    blksize: 4096,
    blocks: Math.ceil(node.size / 4096)
   };
  }),
  setattr: (function(node, attr) {
   if (attr.mode !== undefined) {
    node.mode = attr.mode;
   }
   if (attr.timestamp !== undefined) {
    node.timestamp = attr.timestamp;
   }
  }),
  lookup: (function(parent, name) {
   throw new FS.ErrnoError(ERRNO_CODES.ENOENT);
  }),
  mknod: (function(parent, name, mode, dev) {
   throw new FS.ErrnoError(ERRNO_CODES.EPERM);
  }),
  rename: (function(oldNode, newDir, newName) {
   throw new FS.ErrnoError(ERRNO_CODES.EPERM);
  }),
  unlink: (function(parent, name) {
   throw new FS.ErrnoError(ERRNO_CODES.EPERM);
  }),
  rmdir: (function(parent, name) {
   throw new FS.ErrnoError(ERRNO_CODES.EPERM);
  }),
  readdir: (function(node) {
   throw new FS.ErrnoError(ERRNO_CODES.EPERM);
  }),
  symlink: (function(parent, newName, oldPath) {
   throw new FS.ErrnoError(ERRNO_CODES.EPERM);
  }),
  readlink: (function(node) {
   throw new FS.ErrnoError(ERRNO_CODES.EPERM);
  })
 },
 stream_ops: {
  read: (function(stream, buffer, offset, length, position) {
   if (position >= stream.node.size) return 0;
   var chunk = stream.node.contents.slice(position, position + length);
   var ab = WORKERFS.reader.readAsArrayBuffer(chunk);
   buffer.set(new Uint8Array(ab), offset);
   return chunk.size;
  }),
  write: (function(stream, buffer, offset, length, position) {
   throw new FS.ErrnoError(ERRNO_CODES.EIO);
  }),
  llseek: (function(stream, offset, whence) {
   var position = offset;
   if (whence === 1) {
    position += stream.position;
   } else if (whence === 2) {
    if (FS.isFile(stream.node.mode)) {
     position += stream.node.size;
    }
   }
   if (position < 0) {
    throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
   }
   return position;
  })
 }
};
var _stdin = allocate(1, "i32*", ALLOC_STATIC);
var _stdout = allocate(1, "i32*", ALLOC_STATIC);
var _stderr = allocate(1, "i32*", ALLOC_STATIC);
var FS = {
 root: null,
 mounts: [],
 devices: [ null ],
 streams: [],
 nextInode: 1,
 nameTable: null,
 currentPath: "/",
 initialized: false,
 ignorePermissions: true,
 trackingDelegate: {},
 tracking: {
  openFlags: {
   READ: 1,
   WRITE: 2
  }
 },
 ErrnoError: null,
 genericErrors: {},
 filesystems: null,
 handleFSError: (function(e) {
  if (!(e instanceof FS.ErrnoError)) throw e + " : " + stackTrace();
  return ___setErrNo(e.errno);
 }),
 lookupPath: (function(path, opts) {
  path = PATH.resolve(FS.cwd(), path);
  opts = opts || {};
  if (!path) return {
   path: "",
   node: null
  };
  var defaults = {
   follow_mount: true,
   recurse_count: 0
  };
  for (var key in defaults) {
   if (opts[key] === undefined) {
    opts[key] = defaults[key];
   }
  }
  if (opts.recurse_count > 8) {
   throw new FS.ErrnoError(ERRNO_CODES.ELOOP);
  }
  var parts = PATH.normalizeArray(path.split("/").filter((function(p) {
   return !!p;
  })), false);
  var current = FS.root;
  var current_path = "/";
  for (var i = 0; i < parts.length; i++) {
   var islast = i === parts.length - 1;
   if (islast && opts.parent) {
    break;
   }
   current = FS.lookupNode(current, parts[i]);
   current_path = PATH.join2(current_path, parts[i]);
   if (FS.isMountpoint(current)) {
    if (!islast || islast && opts.follow_mount) {
     current = current.mounted.root;
    }
   }
   if (!islast || opts.follow) {
    var count = 0;
    while (FS.isLink(current.mode)) {
     var link = FS.readlink(current_path);
     current_path = PATH.resolve(PATH.dirname(current_path), link);
     var lookup = FS.lookupPath(current_path, {
      recurse_count: opts.recurse_count
     });
     current = lookup.node;
     if (count++ > 40) {
      throw new FS.ErrnoError(ERRNO_CODES.ELOOP);
     }
    }
   }
  }
  return {
   path: current_path,
   node: current
  };
 }),
 getPath: (function(node) {
  var path;
  while (true) {
   if (FS.isRoot(node)) {
    var mount = node.mount.mountpoint;
    if (!path) return mount;
    return mount[mount.length - 1] !== "/" ? mount + "/" + path : mount + path;
   }
   path = path ? node.name + "/" + path : node.name;
   node = node.parent;
  }
 }),
 hashName: (function(parentid, name) {
  var hash = 0;
  for (var i = 0; i < name.length; i++) {
   hash = (hash << 5) - hash + name.charCodeAt(i) | 0;
  }
  return (parentid + hash >>> 0) % FS.nameTable.length;
 }),
 hashAddNode: (function(node) {
  var hash = FS.hashName(node.parent.id, node.name);
  node.name_next = FS.nameTable[hash];
  FS.nameTable[hash] = node;
 }),
 hashRemoveNode: (function(node) {
  var hash = FS.hashName(node.parent.id, node.name);
  if (FS.nameTable[hash] === node) {
   FS.nameTable[hash] = node.name_next;
  } else {
   var current = FS.nameTable[hash];
   while (current) {
    if (current.name_next === node) {
     current.name_next = node.name_next;
     break;
    }
    current = current.name_next;
   }
  }
 }),
 lookupNode: (function(parent, name) {
  var err = FS.mayLookup(parent);
  if (err) {
   throw new FS.ErrnoError(err, parent);
  }
  var hash = FS.hashName(parent.id, name);
  for (var node = FS.nameTable[hash]; node; node = node.name_next) {
   var nodeName = node.name;
   if (node.parent.id === parent.id && nodeName === name) {
    return node;
   }
  }
  return FS.lookup(parent, name);
 }),
 createNode: (function(parent, name, mode, rdev) {
  if (!FS.FSNode) {
   FS.FSNode = (function(parent, name, mode, rdev) {
    if (!parent) {
     parent = this;
    }
    this.parent = parent;
    this.mount = parent.mount;
    this.mounted = null;
    this.id = FS.nextInode++;
    this.name = name;
    this.mode = mode;
    this.node_ops = {};
    this.stream_ops = {};
    this.rdev = rdev;
   });
   FS.FSNode.prototype = {};
   var readMode = 292 | 73;
   var writeMode = 146;
   Object.defineProperties(FS.FSNode.prototype, {
    read: {
     get: (function() {
      return (this.mode & readMode) === readMode;
     }),
     set: (function(val) {
      val ? this.mode |= readMode : this.mode &= ~readMode;
     })
    },
    write: {
     get: (function() {
      return (this.mode & writeMode) === writeMode;
     }),
     set: (function(val) {
      val ? this.mode |= writeMode : this.mode &= ~writeMode;
     })
    },
    isFolder: {
     get: (function() {
      return FS.isDir(this.mode);
     })
    },
    isDevice: {
     get: (function() {
      return FS.isChrdev(this.mode);
     })
    }
   });
  }
  var node = new FS.FSNode(parent, name, mode, rdev);
  FS.hashAddNode(node);
  return node;
 }),
 destroyNode: (function(node) {
  FS.hashRemoveNode(node);
 }),
 isRoot: (function(node) {
  return node === node.parent;
 }),
 isMountpoint: (function(node) {
  return !!node.mounted;
 }),
 isFile: (function(mode) {
  return (mode & 61440) === 32768;
 }),
 isDir: (function(mode) {
  return (mode & 61440) === 16384;
 }),
 isLink: (function(mode) {
  return (mode & 61440) === 40960;
 }),
 isChrdev: (function(mode) {
  return (mode & 61440) === 8192;
 }),
 isBlkdev: (function(mode) {
  return (mode & 61440) === 24576;
 }),
 isFIFO: (function(mode) {
  return (mode & 61440) === 4096;
 }),
 isSocket: (function(mode) {
  return (mode & 49152) === 49152;
 }),
 flagModes: {
  "r": 0,
  "rs": 1052672,
  "r+": 2,
  "w": 577,
  "wx": 705,
  "xw": 705,
  "w+": 578,
  "wx+": 706,
  "xw+": 706,
  "a": 1089,
  "ax": 1217,
  "xa": 1217,
  "a+": 1090,
  "ax+": 1218,
  "xa+": 1218
 },
 modeStringToFlags: (function(str) {
  var flags = FS.flagModes[str];
  if (typeof flags === "undefined") {
   throw new Error("Unknown file open mode: " + str);
  }
  return flags;
 }),
 flagsToPermissionString: (function(flag) {
  var perms = [ "r", "w", "rw" ][flag & 3];
  if (flag & 512) {
   perms += "w";
  }
  return perms;
 }),
 nodePermissions: (function(node, perms) {
  if (FS.ignorePermissions) {
   return 0;
  }
  if (perms.indexOf("r") !== -1 && !(node.mode & 292)) {
   return ERRNO_CODES.EACCES;
  } else if (perms.indexOf("w") !== -1 && !(node.mode & 146)) {
   return ERRNO_CODES.EACCES;
  } else if (perms.indexOf("x") !== -1 && !(node.mode & 73)) {
   return ERRNO_CODES.EACCES;
  }
  return 0;
 }),
 mayLookup: (function(dir) {
  var err = FS.nodePermissions(dir, "x");
  if (err) return err;
  if (!dir.node_ops.lookup) return ERRNO_CODES.EACCES;
  return 0;
 }),
 mayCreate: (function(dir, name) {
  try {
   var node = FS.lookupNode(dir, name);
   return ERRNO_CODES.EEXIST;
  } catch (e) {}
  return FS.nodePermissions(dir, "wx");
 }),
 mayDelete: (function(dir, name, isdir) {
  var node;
  try {
   node = FS.lookupNode(dir, name);
  } catch (e) {
   return e.errno;
  }
  var err = FS.nodePermissions(dir, "wx");
  if (err) {
   return err;
  }
  if (isdir) {
   if (!FS.isDir(node.mode)) {
    return ERRNO_CODES.ENOTDIR;
   }
   if (FS.isRoot(node) || FS.getPath(node) === FS.cwd()) {
    return ERRNO_CODES.EBUSY;
   }
  } else {
   if (FS.isDir(node.mode)) {
    return ERRNO_CODES.EISDIR;
   }
  }
  return 0;
 }),
 mayOpen: (function(node, flags) {
  if (!node) {
   return ERRNO_CODES.ENOENT;
  }
  if (FS.isLink(node.mode)) {
   return ERRNO_CODES.ELOOP;
  } else if (FS.isDir(node.mode)) {
   if ((flags & 2097155) !== 0 || flags & 512) {
    return ERRNO_CODES.EISDIR;
   }
  }
  return FS.nodePermissions(node, FS.flagsToPermissionString(flags));
 }),
 MAX_OPEN_FDS: 4096,
 nextfd: (function(fd_start, fd_end) {
  fd_start = fd_start || 0;
  fd_end = fd_end || FS.MAX_OPEN_FDS;
  for (var fd = fd_start; fd <= fd_end; fd++) {
   if (!FS.streams[fd]) {
    return fd;
   }
  }
  throw new FS.ErrnoError(ERRNO_CODES.EMFILE);
 }),
 getStream: (function(fd) {
  return FS.streams[fd];
 }),
 createStream: (function(stream, fd_start, fd_end) {
  if (!FS.FSStream) {
   FS.FSStream = (function() {});
   FS.FSStream.prototype = {};
   Object.defineProperties(FS.FSStream.prototype, {
    object: {
     get: (function() {
      return this.node;
     }),
     set: (function(val) {
      this.node = val;
     })
    },
    isRead: {
     get: (function() {
      return (this.flags & 2097155) !== 1;
     })
    },
    isWrite: {
     get: (function() {
      return (this.flags & 2097155) !== 0;
     })
    },
    isAppend: {
     get: (function() {
      return this.flags & 1024;
     })
    }
   });
  }
  var newStream = new FS.FSStream;
  for (var p in stream) {
   newStream[p] = stream[p];
  }
  stream = newStream;
  var fd = FS.nextfd(fd_start, fd_end);
  stream.fd = fd;
  FS.streams[fd] = stream;
  return stream;
 }),
 closeStream: (function(fd) {
  FS.streams[fd] = null;
 }),
 chrdev_stream_ops: {
  open: (function(stream) {
   var device = FS.getDevice(stream.node.rdev);
   stream.stream_ops = device.stream_ops;
   if (stream.stream_ops.open) {
    stream.stream_ops.open(stream);
   }
  }),
  llseek: (function() {
   throw new FS.ErrnoError(ERRNO_CODES.ESPIPE);
  })
 },
 major: (function(dev) {
  return dev >> 8;
 }),
 minor: (function(dev) {
  return dev & 255;
 }),
 makedev: (function(ma, mi) {
  return ma << 8 | mi;
 }),
 registerDevice: (function(dev, ops) {
  FS.devices[dev] = {
   stream_ops: ops
  };
 }),
 getDevice: (function(dev) {
  return FS.devices[dev];
 }),
 getMounts: (function(mount) {
  var mounts = [];
  var check = [ mount ];
  while (check.length) {
   var m = check.pop();
   mounts.push(m);
   check.push.apply(check, m.mounts);
  }
  return mounts;
 }),
 syncfs: (function(populate, callback) {
  if (typeof populate === "function") {
   callback = populate;
   populate = false;
  }
  var mounts = FS.getMounts(FS.root.mount);
  var completed = 0;
  function done(err) {
   if (err) {
    if (!done.errored) {
     done.errored = true;
     return callback(err);
    }
    return;
   }
   if (++completed >= mounts.length) {
    callback(null);
   }
  }
  mounts.forEach((function(mount) {
   if (!mount.type.syncfs) {
    return done(null);
   }
   mount.type.syncfs(mount, populate, done);
  }));
 }),
 mount: (function(type, opts, mountpoint) {
  var root = mountpoint === "/";
  var pseudo = !mountpoint;
  var node;
  if (root && FS.root) {
   throw new FS.ErrnoError(ERRNO_CODES.EBUSY);
  } else if (!root && !pseudo) {
   var lookup = FS.lookupPath(mountpoint, {
    follow_mount: false
   });
   mountpoint = lookup.path;
   node = lookup.node;
   if (FS.isMountpoint(node)) {
    throw new FS.ErrnoError(ERRNO_CODES.EBUSY);
   }
   if (!FS.isDir(node.mode)) {
    throw new FS.ErrnoError(ERRNO_CODES.ENOTDIR);
   }
  }
  var mount = {
   type: type,
   opts: opts,
   mountpoint: mountpoint,
   mounts: []
  };
  var mountRoot = type.mount(mount);
  mountRoot.mount = mount;
  mount.root = mountRoot;
  if (root) {
   FS.root = mountRoot;
  } else if (node) {
   node.mounted = mount;
   if (node.mount) {
    node.mount.mounts.push(mount);
   }
  }
  return mountRoot;
 }),
 unmount: (function(mountpoint) {
  var lookup = FS.lookupPath(mountpoint, {
   follow_mount: false
  });
  if (!FS.isMountpoint(lookup.node)) {
   throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
  }
  var node = lookup.node;
  var mount = node.mounted;
  var mounts = FS.getMounts(mount);
  Object.keys(FS.nameTable).forEach((function(hash) {
   var current = FS.nameTable[hash];
   while (current) {
    var next = current.name_next;
    if (mounts.indexOf(current.mount) !== -1) {
     FS.destroyNode(current);
    }
    current = next;
   }
  }));
  node.mounted = null;
  var idx = node.mount.mounts.indexOf(mount);
  assert(idx !== -1);
  node.mount.mounts.splice(idx, 1);
 }),
 lookup: (function(parent, name) {
  return parent.node_ops.lookup(parent, name);
 }),
 mknod: (function(path, mode, dev) {
  var lookup = FS.lookupPath(path, {
   parent: true
  });
  var parent = lookup.node;
  var name = PATH.basename(path);
  if (!name || name === "." || name === "..") {
   throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
  }
  var err = FS.mayCreate(parent, name);
  if (err) {
   throw new FS.ErrnoError(err);
  }
  if (!parent.node_ops.mknod) {
   throw new FS.ErrnoError(ERRNO_CODES.EPERM);
  }
  return parent.node_ops.mknod(parent, name, mode, dev);
 }),
 create: (function(path, mode) {
  mode = mode !== undefined ? mode : 438;
  mode &= 4095;
  mode |= 32768;
  return FS.mknod(path, mode, 0);
 }),
 mkdir: (function(path, mode) {
  mode = mode !== undefined ? mode : 511;
  mode &= 511 | 512;
  mode |= 16384;
  return FS.mknod(path, mode, 0);
 }),
 mkdev: (function(path, mode, dev) {
  if (typeof dev === "undefined") {
   dev = mode;
   mode = 438;
  }
  mode |= 8192;
  return FS.mknod(path, mode, dev);
 }),
 symlink: (function(oldpath, newpath) {
  if (!PATH.resolve(oldpath)) {
   throw new FS.ErrnoError(ERRNO_CODES.ENOENT);
  }
  var lookup = FS.lookupPath(newpath, {
   parent: true
  });
  var parent = lookup.node;
  if (!parent) {
   throw new FS.ErrnoError(ERRNO_CODES.ENOENT);
  }
  var newname = PATH.basename(newpath);
  var err = FS.mayCreate(parent, newname);
  if (err) {
   throw new FS.ErrnoError(err);
  }
  if (!parent.node_ops.symlink) {
   throw new FS.ErrnoError(ERRNO_CODES.EPERM);
  }
  return parent.node_ops.symlink(parent, newname, oldpath);
 }),
 rename: (function(old_path, new_path) {
  var old_dirname = PATH.dirname(old_path);
  var new_dirname = PATH.dirname(new_path);
  var old_name = PATH.basename(old_path);
  var new_name = PATH.basename(new_path);
  var lookup, old_dir, new_dir;
  try {
   lookup = FS.lookupPath(old_path, {
    parent: true
   });
   old_dir = lookup.node;
   lookup = FS.lookupPath(new_path, {
    parent: true
   });
   new_dir = lookup.node;
  } catch (e) {
   throw new FS.ErrnoError(ERRNO_CODES.EBUSY);
  }
  if (!old_dir || !new_dir) throw new FS.ErrnoError(ERRNO_CODES.ENOENT);
  if (old_dir.mount !== new_dir.mount) {
   throw new FS.ErrnoError(ERRNO_CODES.EXDEV);
  }
  var old_node = FS.lookupNode(old_dir, old_name);
  var relative = PATH.relative(old_path, new_dirname);
  if (relative.charAt(0) !== ".") {
   throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
  }
  relative = PATH.relative(new_path, old_dirname);
  if (relative.charAt(0) !== ".") {
   throw new FS.ErrnoError(ERRNO_CODES.ENOTEMPTY);
  }
  var new_node;
  try {
   new_node = FS.lookupNode(new_dir, new_name);
  } catch (e) {}
  if (old_node === new_node) {
   return;
  }
  var isdir = FS.isDir(old_node.mode);
  var err = FS.mayDelete(old_dir, old_name, isdir);
  if (err) {
   throw new FS.ErrnoError(err);
  }
  err = new_node ? FS.mayDelete(new_dir, new_name, isdir) : FS.mayCreate(new_dir, new_name);
  if (err) {
   throw new FS.ErrnoError(err);
  }
  if (!old_dir.node_ops.rename) {
   throw new FS.ErrnoError(ERRNO_CODES.EPERM);
  }
  if (FS.isMountpoint(old_node) || new_node && FS.isMountpoint(new_node)) {
   throw new FS.ErrnoError(ERRNO_CODES.EBUSY);
  }
  if (new_dir !== old_dir) {
   err = FS.nodePermissions(old_dir, "w");
   if (err) {
    throw new FS.ErrnoError(err);
   }
  }
  try {
   if (FS.trackingDelegate["willMovePath"]) {
    FS.trackingDelegate["willMovePath"](old_path, new_path);
   }
  } catch (e) {
   console.log("FS.trackingDelegate['willMovePath']('" + old_path + "', '" + new_path + "') threw an exception: " + e.message);
  }
  FS.hashRemoveNode(old_node);
  try {
   old_dir.node_ops.rename(old_node, new_dir, new_name);
  } catch (e) {
   throw e;
  } finally {
   FS.hashAddNode(old_node);
  }
  try {
   if (FS.trackingDelegate["onMovePath"]) FS.trackingDelegate["onMovePath"](old_path, new_path);
  } catch (e) {
   console.log("FS.trackingDelegate['onMovePath']('" + old_path + "', '" + new_path + "') threw an exception: " + e.message);
  }
 }),
 rmdir: (function(path) {
  var lookup = FS.lookupPath(path, {
   parent: true
  });
  var parent = lookup.node;
  var name = PATH.basename(path);
  var node = FS.lookupNode(parent, name);
  var err = FS.mayDelete(parent, name, true);
  if (err) {
   throw new FS.ErrnoError(err);
  }
  if (!parent.node_ops.rmdir) {
   throw new FS.ErrnoError(ERRNO_CODES.EPERM);
  }
  if (FS.isMountpoint(node)) {
   throw new FS.ErrnoError(ERRNO_CODES.EBUSY);
  }
  try {
   if (FS.trackingDelegate["willDeletePath"]) {
    FS.trackingDelegate["willDeletePath"](path);
   }
  } catch (e) {
   console.log("FS.trackingDelegate['willDeletePath']('" + path + "') threw an exception: " + e.message);
  }
  parent.node_ops.rmdir(parent, name);
  FS.destroyNode(node);
  try {
   if (FS.trackingDelegate["onDeletePath"]) FS.trackingDelegate["onDeletePath"](path);
  } catch (e) {
   console.log("FS.trackingDelegate['onDeletePath']('" + path + "') threw an exception: " + e.message);
  }
 }),
 readdir: (function(path) {
  var lookup = FS.lookupPath(path, {
   follow: true
  });
  var node = lookup.node;
  if (!node.node_ops.readdir) {
   throw new FS.ErrnoError(ERRNO_CODES.ENOTDIR);
  }
  return node.node_ops.readdir(node);
 }),
 unlink: (function(path) {
  var lookup = FS.lookupPath(path, {
   parent: true
  });
  var parent = lookup.node;
  var name = PATH.basename(path);
  var node = FS.lookupNode(parent, name);
  var err = FS.mayDelete(parent, name, false);
  if (err) {
   if (err === ERRNO_CODES.EISDIR) err = ERRNO_CODES.EPERM;
   throw new FS.ErrnoError(err);
  }
  if (!parent.node_ops.unlink) {
   throw new FS.ErrnoError(ERRNO_CODES.EPERM);
  }
  if (FS.isMountpoint(node)) {
   throw new FS.ErrnoError(ERRNO_CODES.EBUSY);
  }
  try {
   if (FS.trackingDelegate["willDeletePath"]) {
    FS.trackingDelegate["willDeletePath"](path);
   }
  } catch (e) {
   console.log("FS.trackingDelegate['willDeletePath']('" + path + "') threw an exception: " + e.message);
  }
  parent.node_ops.unlink(parent, name);
  FS.destroyNode(node);
  try {
   if (FS.trackingDelegate["onDeletePath"]) FS.trackingDelegate["onDeletePath"](path);
  } catch (e) {
   console.log("FS.trackingDelegate['onDeletePath']('" + path + "') threw an exception: " + e.message);
  }
 }),
 readlink: (function(path) {
  var lookup = FS.lookupPath(path);
  var link = lookup.node;
  if (!link) {
   throw new FS.ErrnoError(ERRNO_CODES.ENOENT);
  }
  if (!link.node_ops.readlink) {
   throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
  }
  return PATH.resolve(FS.getPath(link.parent), link.node_ops.readlink(link));
 }),
 stat: (function(path, dontFollow) {
  var lookup = FS.lookupPath(path, {
   follow: !dontFollow
  });
  var node = lookup.node;
  if (!node) {
   throw new FS.ErrnoError(ERRNO_CODES.ENOENT);
  }
  if (!node.node_ops.getattr) {
   throw new FS.ErrnoError(ERRNO_CODES.EPERM);
  }
  return node.node_ops.getattr(node);
 }),
 lstat: (function(path) {
  return FS.stat(path, true);
 }),
 chmod: (function(path, mode, dontFollow) {
  var node;
  if (typeof path === "string") {
   var lookup = FS.lookupPath(path, {
    follow: !dontFollow
   });
   node = lookup.node;
  } else {
   node = path;
  }
  if (!node.node_ops.setattr) {
   throw new FS.ErrnoError(ERRNO_CODES.EPERM);
  }
  node.node_ops.setattr(node, {
   mode: mode & 4095 | node.mode & ~4095,
   timestamp: Date.now()
  });
 }),
 lchmod: (function(path, mode) {
  FS.chmod(path, mode, true);
 }),
 fchmod: (function(fd, mode) {
  var stream = FS.getStream(fd);
  if (!stream) {
   throw new FS.ErrnoError(ERRNO_CODES.EBADF);
  }
  FS.chmod(stream.node, mode);
 }),
 chown: (function(path, uid, gid, dontFollow) {
  var node;
  if (typeof path === "string") {
   var lookup = FS.lookupPath(path, {
    follow: !dontFollow
   });
   node = lookup.node;
  } else {
   node = path;
  }
  if (!node.node_ops.setattr) {
   throw new FS.ErrnoError(ERRNO_CODES.EPERM);
  }
  node.node_ops.setattr(node, {
   timestamp: Date.now()
  });
 }),
 lchown: (function(path, uid, gid) {
  FS.chown(path, uid, gid, true);
 }),
 fchown: (function(fd, uid, gid) {
  var stream = FS.getStream(fd);
  if (!stream) {
   throw new FS.ErrnoError(ERRNO_CODES.EBADF);
  }
  FS.chown(stream.node, uid, gid);
 }),
 truncate: (function(path, len) {
  if (len < 0) {
   throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
  }
  var node;
  if (typeof path === "string") {
   var lookup = FS.lookupPath(path, {
    follow: true
   });
   node = lookup.node;
  } else {
   node = path;
  }
  if (!node.node_ops.setattr) {
   throw new FS.ErrnoError(ERRNO_CODES.EPERM);
  }
  if (FS.isDir(node.mode)) {
   throw new FS.ErrnoError(ERRNO_CODES.EISDIR);
  }
  if (!FS.isFile(node.mode)) {
   throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
  }
  var err = FS.nodePermissions(node, "w");
  if (err) {
   throw new FS.ErrnoError(err);
  }
  node.node_ops.setattr(node, {
   size: len,
   timestamp: Date.now()
  });
 }),
 ftruncate: (function(fd, len) {
  var stream = FS.getStream(fd);
  if (!stream) {
   throw new FS.ErrnoError(ERRNO_CODES.EBADF);
  }
  if ((stream.flags & 2097155) === 0) {
   throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
  }
  FS.truncate(stream.node, len);
 }),
 utime: (function(path, atime, mtime) {
  var lookup = FS.lookupPath(path, {
   follow: true
  });
  var node = lookup.node;
  node.node_ops.setattr(node, {
   timestamp: Math.max(atime, mtime)
  });
 }),
 open: (function(path, flags, mode, fd_start, fd_end) {
  if (path === "") {
   throw new FS.ErrnoError(ERRNO_CODES.ENOENT);
  }
  flags = typeof flags === "string" ? FS.modeStringToFlags(flags) : flags;
  mode = typeof mode === "undefined" ? 438 : mode;
  if (flags & 64) {
   mode = mode & 4095 | 32768;
  } else {
   mode = 0;
  }
  var node;
  if (typeof path === "object") {
   node = path;
  } else {
   path = PATH.normalize(path);
   try {
    var lookup = FS.lookupPath(path, {
     follow: !(flags & 131072)
    });
    node = lookup.node;
   } catch (e) {}
  }
  var created = false;
  if (flags & 64) {
   if (node) {
    if (flags & 128) {
     throw new FS.ErrnoError(ERRNO_CODES.EEXIST);
    }
   } else {
    node = FS.mknod(path, mode, 0);
    created = true;
   }
  }
  if (!node) {
   throw new FS.ErrnoError(ERRNO_CODES.ENOENT);
  }
  if (FS.isChrdev(node.mode)) {
   flags &= ~512;
  }
  if (flags & 65536 && !FS.isDir(node.mode)) {
   throw new FS.ErrnoError(ERRNO_CODES.ENOTDIR);
  }
  if (!created) {
   var err = FS.mayOpen(node, flags);
   if (err) {
    throw new FS.ErrnoError(err);
   }
  }
  if (flags & 512) {
   FS.truncate(node, 0);
  }
  flags &= ~(128 | 512);
  var stream = FS.createStream({
   node: node,
   path: FS.getPath(node),
   flags: flags,
   seekable: true,
   position: 0,
   stream_ops: node.stream_ops,
   ungotten: [],
   error: false
  }, fd_start, fd_end);
  if (stream.stream_ops.open) {
   stream.stream_ops.open(stream);
  }
  if (Module["logReadFiles"] && !(flags & 1)) {
   if (!FS.readFiles) FS.readFiles = {};
   if (!(path in FS.readFiles)) {
    FS.readFiles[path] = 1;
    Module["printErr"]("read file: " + path);
   }
  }
  try {
   if (FS.trackingDelegate["onOpenFile"]) {
    var trackingFlags = 0;
    if ((flags & 2097155) !== 1) {
     trackingFlags |= FS.tracking.openFlags.READ;
    }
    if ((flags & 2097155) !== 0) {
     trackingFlags |= FS.tracking.openFlags.WRITE;
    }
    FS.trackingDelegate["onOpenFile"](path, trackingFlags);
   }
  } catch (e) {
   console.log("FS.trackingDelegate['onOpenFile']('" + path + "', flags) threw an exception: " + e.message);
  }
  return stream;
 }),
 close: (function(stream) {
  if (stream.getdents) stream.getdents = null;
  try {
   if (stream.stream_ops.close) {
    stream.stream_ops.close(stream);
   }
  } catch (e) {
   throw e;
  } finally {
   FS.closeStream(stream.fd);
  }
 }),
 llseek: (function(stream, offset, whence) {
  if (!stream.seekable || !stream.stream_ops.llseek) {
   throw new FS.ErrnoError(ERRNO_CODES.ESPIPE);
  }
  stream.position = stream.stream_ops.llseek(stream, offset, whence);
  stream.ungotten = [];
  return stream.position;
 }),
 read: (function(stream, buffer, offset, length, position) {
  if (length < 0 || position < 0) {
   throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
  }
  if ((stream.flags & 2097155) === 1) {
   throw new FS.ErrnoError(ERRNO_CODES.EBADF);
  }
  if (FS.isDir(stream.node.mode)) {
   throw new FS.ErrnoError(ERRNO_CODES.EISDIR);
  }
  if (!stream.stream_ops.read) {
   throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
  }
  var seeking = true;
  if (typeof position === "undefined") {
   position = stream.position;
   seeking = false;
  } else if (!stream.seekable) {
   throw new FS.ErrnoError(ERRNO_CODES.ESPIPE);
  }
  var bytesRead = stream.stream_ops.read(stream, buffer, offset, length, position);
  if (!seeking) stream.position += bytesRead;
  return bytesRead;
 }),
 write: (function(stream, buffer, offset, length, position, canOwn) {
  if (length < 0 || position < 0) {
   throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
  }
  if ((stream.flags & 2097155) === 0) {
   throw new FS.ErrnoError(ERRNO_CODES.EBADF);
  }
  if (FS.isDir(stream.node.mode)) {
   throw new FS.ErrnoError(ERRNO_CODES.EISDIR);
  }
  if (!stream.stream_ops.write) {
   throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
  }
  if (stream.flags & 1024) {
   FS.llseek(stream, 0, 2);
  }
  var seeking = true;
  if (typeof position === "undefined") {
   position = stream.position;
   seeking = false;
  } else if (!stream.seekable) {
   throw new FS.ErrnoError(ERRNO_CODES.ESPIPE);
  }
  var bytesWritten = stream.stream_ops.write(stream, buffer, offset, length, position, canOwn);
  if (!seeking) stream.position += bytesWritten;
  try {
   if (stream.path && FS.trackingDelegate["onWriteToFile"]) FS.trackingDelegate["onWriteToFile"](stream.path);
  } catch (e) {
   console.log("FS.trackingDelegate['onWriteToFile']('" + path + "') threw an exception: " + e.message);
  }
  return bytesWritten;
 }),
 allocate: (function(stream, offset, length) {
  if (offset < 0 || length <= 0) {
   throw new FS.ErrnoError(ERRNO_CODES.EINVAL);
  }
  if ((stream.flags & 2097155) === 0) {
   throw new FS.ErrnoError(ERRNO_CODES.EBADF);
  }
  if (!FS.isFile(stream.node.mode) && !FS.isDir(node.mode)) {
   throw new FS.ErrnoError(ERRNO_CODES.ENODEV);
  }
  if (!stream.stream_ops.allocate) {
   throw new FS.ErrnoError(ERRNO_CODES.EOPNOTSUPP);
  }
  stream.stream_ops.allocate(stream, offset, length);
 }),
 mmap: (function(stream, buffer, offset, length, position, prot, flags) {
  if ((stream.flags & 2097155) === 1) {
   throw new FS.ErrnoError(ERRNO_CODES.EACCES);
  }
  if (!stream.stream_ops.mmap) {
   throw new FS.ErrnoError(ERRNO_CODES.ENODEV);
  }
  return stream.stream_ops.mmap(stream, buffer, offset, length, position, prot, flags);
 }),
 msync: (function(stream, buffer, offset, length, mmapFlags) {
  if (!stream || !stream.stream_ops.msync) {
   return 0;
  }
  return stream.stream_ops.msync(stream, buffer, offset, length, mmapFlags);
 }),
 munmap: (function(stream) {
  return 0;
 }),
 ioctl: (function(stream, cmd, arg) {
  if (!stream.stream_ops.ioctl) {
   throw new FS.ErrnoError(ERRNO_CODES.ENOTTY);
  }
  return stream.stream_ops.ioctl(stream, cmd, arg);
 }),
 readFile: (function(path, opts) {
  opts = opts || {};
  opts.flags = opts.flags || "r";
  opts.encoding = opts.encoding || "binary";
  if (opts.encoding !== "utf8" && opts.encoding !== "binary") {
   throw new Error('Invalid encoding type "' + opts.encoding + '"');
  }
  var ret;
  var stream = FS.open(path, opts.flags);
  var stat = FS.stat(path);
  var length = stat.size;
  var buf = new Uint8Array(length);
  FS.read(stream, buf, 0, length, 0);
  if (opts.encoding === "utf8") {
   ret = UTF8ArrayToString(buf, 0);
  } else if (opts.encoding === "binary") {
   ret = buf;
  }
  FS.close(stream);
  return ret;
 }),
 writeFile: (function(path, data, opts) {
  opts = opts || {};
  opts.flags = opts.flags || "w";
  opts.encoding = opts.encoding || "utf8";
  if (opts.encoding !== "utf8" && opts.encoding !== "binary") {
   throw new Error('Invalid encoding type "' + opts.encoding + '"');
  }
  var stream = FS.open(path, opts.flags, opts.mode);
  if (opts.encoding === "utf8") {
   var buf = new Uint8Array(lengthBytesUTF8(data) + 1);
   var actualNumBytes = stringToUTF8Array(data, buf, 0, buf.length);
   FS.write(stream, buf, 0, actualNumBytes, 0, opts.canOwn);
  } else if (opts.encoding === "binary") {
   FS.write(stream, data, 0, data.length, 0, opts.canOwn);
  }
  FS.close(stream);
 }),
 cwd: (function() {
  return FS.currentPath;
 }),
 chdir: (function(path) {
  var lookup = FS.lookupPath(path, {
   follow: true
  });
  if (!FS.isDir(lookup.node.mode)) {
   throw new FS.ErrnoError(ERRNO_CODES.ENOTDIR);
  }
  var err = FS.nodePermissions(lookup.node, "x");
  if (err) {
   throw new FS.ErrnoError(err);
  }
  FS.currentPath = lookup.path;
 }),
 createDefaultDirectories: (function() {
  FS.mkdir("/tmp");
  FS.mkdir("/home");
  FS.mkdir("/home/web_user");
 }),
 createDefaultDevices: (function() {
  FS.mkdir("/dev");
  FS.registerDevice(FS.makedev(1, 3), {
   read: (function() {
    return 0;
   }),
   write: (function(stream, buffer, offset, length, pos) {
    return length;
   })
  });
  FS.mkdev("/dev/null", FS.makedev(1, 3));
  TTY.register(FS.makedev(5, 0), TTY.default_tty_ops);
  TTY.register(FS.makedev(6, 0), TTY.default_tty1_ops);
  FS.mkdev("/dev/tty", FS.makedev(5, 0));
  FS.mkdev("/dev/tty1", FS.makedev(6, 0));
  var random_device;
  if (typeof crypto !== "undefined") {
   var randomBuffer = new Uint8Array(1);
   random_device = (function() {
    crypto.getRandomValues(randomBuffer);
    return randomBuffer[0];
   });
  } else if (ENVIRONMENT_IS_NODE) {
   random_device = (function() {
    return require("crypto").randomBytes(1)[0];
   });
  } else {
   random_device = (function() {
    return Math.random() * 256 | 0;
   });
  }
  FS.createDevice("/dev", "random", random_device);
  FS.createDevice("/dev", "urandom", random_device);
  FS.mkdir("/dev/shm");
  FS.mkdir("/dev/shm/tmp");
 }),
 createSpecialDirectories: (function() {
  FS.mkdir("/proc");
  FS.mkdir("/proc/self");
  FS.mkdir("/proc/self/fd");
  FS.mount({
   mount: (function() {
    var node = FS.createNode("/proc/self", "fd", 16384 | 511, 73);
    node.node_ops = {
     lookup: (function(parent, name) {
      var fd = +name;
      var stream = FS.getStream(fd);
      if (!stream) throw new FS.ErrnoError(ERRNO_CODES.EBADF);
      var ret = {
       parent: null,
       mount: {
        mountpoint: "fake"
       },
       node_ops: {
        readlink: (function() {
         return stream.path;
        })
       }
      };
      ret.parent = ret;
      return ret;
     })
    };
    return node;
   })
  }, {}, "/proc/self/fd");
 }),
 createStandardStreams: (function() {
  if (Module["stdin"]) {
   FS.createDevice("/dev", "stdin", Module["stdin"]);
  } else {
   FS.symlink("/dev/tty", "/dev/stdin");
  }
  if (Module["stdout"]) {
   FS.createDevice("/dev", "stdout", null, Module["stdout"]);
  } else {
   FS.symlink("/dev/tty", "/dev/stdout");
  }
  if (Module["stderr"]) {
   FS.createDevice("/dev", "stderr", null, Module["stderr"]);
  } else {
   FS.symlink("/dev/tty1", "/dev/stderr");
  }
  var stdin = FS.open("/dev/stdin", "r");
  assert(stdin.fd === 0, "invalid handle for stdin (" + stdin.fd + ")");
  var stdout = FS.open("/dev/stdout", "w");
  assert(stdout.fd === 1, "invalid handle for stdout (" + stdout.fd + ")");
  var stderr = FS.open("/dev/stderr", "w");
  assert(stderr.fd === 2, "invalid handle for stderr (" + stderr.fd + ")");
 }),
 ensureErrnoError: (function() {
  if (FS.ErrnoError) return;
  FS.ErrnoError = function ErrnoError(errno, node) {
   this.node = node;
   this.setErrno = (function(errno) {
    this.errno = errno;
    for (var key in ERRNO_CODES) {
     if (ERRNO_CODES[key] === errno) {
      this.code = key;
      break;
     }
    }
   });
   this.setErrno(errno);
   this.message = ERRNO_MESSAGES[errno];
  };
  FS.ErrnoError.prototype = new Error;
  FS.ErrnoError.prototype.constructor = FS.ErrnoError;
  [ ERRNO_CODES.ENOENT ].forEach((function(code) {
   FS.genericErrors[code] = new FS.ErrnoError(code);
   FS.genericErrors[code].stack = "<generic error, no stack>";
  }));
 }),
 staticInit: (function() {
  FS.ensureErrnoError();
  FS.nameTable = new Array(4096);
  FS.mount(MEMFS, {}, "/");
  FS.createDefaultDirectories();
  FS.createDefaultDevices();
  FS.createSpecialDirectories();
  FS.filesystems = {
   "MEMFS": MEMFS,
   "IDBFS": IDBFS,
   "NODEFS": NODEFS,
   "WORKERFS": WORKERFS
  };
 }),
 init: (function(input, output, error) {
  assert(!FS.init.initialized, "FS.init was previously called. If you want to initialize later with custom parameters, remove any earlier calls (note that one is automatically added to the generated code)");
  FS.init.initialized = true;
  FS.ensureErrnoError();
  Module["stdin"] = input || Module["stdin"];
  Module["stdout"] = output || Module["stdout"];
  Module["stderr"] = error || Module["stderr"];
  FS.createStandardStreams();
 }),
 quit: (function() {
  FS.init.initialized = false;
  var fflush = Module["_fflush"];
  if (fflush) fflush(0);
  for (var i = 0; i < FS.streams.length; i++) {
   var stream = FS.streams[i];
   if (!stream) {
    continue;
   }
   FS.close(stream);
  }
 }),
 getMode: (function(canRead, canWrite) {
  var mode = 0;
  if (canRead) mode |= 292 | 73;
  if (canWrite) mode |= 146;
  return mode;
 }),
 joinPath: (function(parts, forceRelative) {
  var path = PATH.join.apply(null, parts);
  if (forceRelative && path[0] == "/") path = path.substr(1);
  return path;
 }),
 absolutePath: (function(relative, base) {
  return PATH.resolve(base, relative);
 }),
 standardizePath: (function(path) {
  return PATH.normalize(path);
 }),
 findObject: (function(path, dontResolveLastLink) {
  var ret = FS.analyzePath(path, dontResolveLastLink);
  if (ret.exists) {
   return ret.object;
  } else {
   ___setErrNo(ret.error);
   return null;
  }
 }),
 analyzePath: (function(path, dontResolveLastLink) {
  try {
   var lookup = FS.lookupPath(path, {
    follow: !dontResolveLastLink
   });
   path = lookup.path;
  } catch (e) {}
  var ret = {
   isRoot: false,
   exists: false,
   error: 0,
   name: null,
   path: null,
   object: null,
   parentExists: false,
   parentPath: null,
   parentObject: null
  };
  try {
   var lookup = FS.lookupPath(path, {
    parent: true
   });
   ret.parentExists = true;
   ret.parentPath = lookup.path;
   ret.parentObject = lookup.node;
   ret.name = PATH.basename(path);
   lookup = FS.lookupPath(path, {
    follow: !dontResolveLastLink
   });
   ret.exists = true;
   ret.path = lookup.path;
   ret.object = lookup.node;
   ret.name = lookup.node.name;
   ret.isRoot = lookup.path === "/";
  } catch (e) {
   ret.error = e.errno;
  }
  return ret;
 }),
 createFolder: (function(parent, name, canRead, canWrite) {
  var path = PATH.join2(typeof parent === "string" ? parent : FS.getPath(parent), name);
  var mode = FS.getMode(canRead, canWrite);
  return FS.mkdir(path, mode);
 }),
 createPath: (function(parent, path, canRead, canWrite) {
  parent = typeof parent === "string" ? parent : FS.getPath(parent);
  var parts = path.split("/").reverse();
  while (parts.length) {
   var part = parts.pop();
   if (!part) continue;
   var current = PATH.join2(parent, part);
   try {
    FS.mkdir(current);
   } catch (e) {}
   parent = current;
  }
  return current;
 }),
 createFile: (function(parent, name, properties, canRead, canWrite) {
  var path = PATH.join2(typeof parent === "string" ? parent : FS.getPath(parent), name);
  var mode = FS.getMode(canRead, canWrite);
  return FS.create(path, mode);
 }),
 createDataFile: (function(parent, name, data, canRead, canWrite, canOwn) {
  var path = name ? PATH.join2(typeof parent === "string" ? parent : FS.getPath(parent), name) : parent;
  var mode = FS.getMode(canRead, canWrite);
  var node = FS.create(path, mode);
  if (data) {
   if (typeof data === "string") {
    var arr = new Array(data.length);
    for (var i = 0, len = data.length; i < len; ++i) arr[i] = data.charCodeAt(i);
    data = arr;
   }
   FS.chmod(node, mode | 146);
   var stream = FS.open(node, "w");
   FS.write(stream, data, 0, data.length, 0, canOwn);
   FS.close(stream);
   FS.chmod(node, mode);
  }
  return node;
 }),
 createDevice: (function(parent, name, input, output) {
  var path = PATH.join2(typeof parent === "string" ? parent : FS.getPath(parent), name);
  var mode = FS.getMode(!!input, !!output);
  if (!FS.createDevice.major) FS.createDevice.major = 64;
  var dev = FS.makedev(FS.createDevice.major++, 0);
  FS.registerDevice(dev, {
   open: (function(stream) {
    stream.seekable = false;
   }),
   close: (function(stream) {
    if (output && output.buffer && output.buffer.length) {
     output(10);
    }
   }),
   read: (function(stream, buffer, offset, length, pos) {
    var bytesRead = 0;
    for (var i = 0; i < length; i++) {
     var result;
     try {
      result = input();
     } catch (e) {
      throw new FS.ErrnoError(ERRNO_CODES.EIO);
     }
     if (result === undefined && bytesRead === 0) {
      throw new FS.ErrnoError(ERRNO_CODES.EAGAIN);
     }
     if (result === null || result === undefined) break;
     bytesRead++;
     buffer[offset + i] = result;
    }
    if (bytesRead) {
     stream.node.timestamp = Date.now();
    }
    return bytesRead;
   }),
   write: (function(stream, buffer, offset, length, pos) {
    for (var i = 0; i < length; i++) {
     try {
      output(buffer[offset + i]);
     } catch (e) {
      throw new FS.ErrnoError(ERRNO_CODES.EIO);
     }
    }
    if (length) {
     stream.node.timestamp = Date.now();
    }
    return i;
   })
  });
  return FS.mkdev(path, mode, dev);
 }),
 createLink: (function(parent, name, target, canRead, canWrite) {
  var path = PATH.join2(typeof parent === "string" ? parent : FS.getPath(parent), name);
  return FS.symlink(target, path);
 }),
 forceLoadFile: (function(obj) {
  if (obj.isDevice || obj.isFolder || obj.link || obj.contents) return true;
  var success = true;
  if (typeof XMLHttpRequest !== "undefined") {
   throw new Error("Lazy loading should have been performed (contents set) in createLazyFile, but it was not. Lazy loading only works in web workers. Use --embed-file or --preload-file in emcc on the main thread.");
  } else if (Module["read"]) {
   try {
    obj.contents = intArrayFromString(Module["read"](obj.url), true);
    obj.usedBytes = obj.contents.length;
   } catch (e) {
    success = false;
   }
  } else {
   throw new Error("Cannot load without read() or XMLHttpRequest.");
  }
  if (!success) ___setErrNo(ERRNO_CODES.EIO);
  return success;
 }),
 createLazyFile: (function(parent, name, url, canRead, canWrite) {
  function LazyUint8Array() {
   this.lengthKnown = false;
   this.chunks = [];
  }
  LazyUint8Array.prototype.get = function LazyUint8Array_get(idx) {
   if (idx > this.length - 1 || idx < 0) {
    return undefined;
   }
   var chunkOffset = idx % this.chunkSize;
   var chunkNum = idx / this.chunkSize | 0;
   return this.getter(chunkNum)[chunkOffset];
  };
  LazyUint8Array.prototype.setDataGetter = function LazyUint8Array_setDataGetter(getter) {
   this.getter = getter;
  };
  LazyUint8Array.prototype.cacheLength = function LazyUint8Array_cacheLength() {
   var xhr = new XMLHttpRequest;
   xhr.open("HEAD", url, false);
   xhr.send(null);
   if (!(xhr.status >= 200 && xhr.status < 300 || xhr.status === 304)) throw new Error("Couldn't load " + url + ". Status: " + xhr.status);
   var datalength = Number(xhr.getResponseHeader("Content-length"));
   var header;
   var hasByteServing = (header = xhr.getResponseHeader("Accept-Ranges")) && header === "bytes";
   var chunkSize = 1024 * 1024;
   if (!hasByteServing) chunkSize = datalength;
   var doXHR = (function(from, to) {
    if (from > to) throw new Error("invalid range (" + from + ", " + to + ") or no bytes requested!");
    if (to > datalength - 1) throw new Error("only " + datalength + " bytes available! programmer error!");
    var xhr = new XMLHttpRequest;
    xhr.open("GET", url, false);
    if (datalength !== chunkSize) xhr.setRequestHeader("Range", "bytes=" + from + "-" + to);
    if (typeof Uint8Array != "undefined") xhr.responseType = "arraybuffer";
    if (xhr.overrideMimeType) {
     xhr.overrideMimeType("text/plain; charset=x-user-defined");
    }
    xhr.send(null);
    if (!(xhr.status >= 200 && xhr.status < 300 || xhr.status === 304)) throw new Error("Couldn't load " + url + ". Status: " + xhr.status);
    if (xhr.response !== undefined) {
     return new Uint8Array(xhr.response || []);
    } else {
     return intArrayFromString(xhr.responseText || "", true);
    }
   });
   var lazyArray = this;
   lazyArray.setDataGetter((function(chunkNum) {
    var start = chunkNum * chunkSize;
    var end = (chunkNum + 1) * chunkSize - 1;
    end = Math.min(end, datalength - 1);
    if (typeof lazyArray.chunks[chunkNum] === "undefined") {
     lazyArray.chunks[chunkNum] = doXHR(start, end);
    }
    if (typeof lazyArray.chunks[chunkNum] === "undefined") throw new Error("doXHR failed!");
    return lazyArray.chunks[chunkNum];
   }));
   this._length = datalength;
   this._chunkSize = chunkSize;
   this.lengthKnown = true;
  };
  if (typeof XMLHttpRequest !== "undefined") {
   if (!ENVIRONMENT_IS_WORKER) throw "Cannot do synchronous binary XHRs outside webworkers in modern browsers. Use --embed-file or --preload-file in emcc";
   var lazyArray = new LazyUint8Array;
   Object.defineProperty(lazyArray, "length", {
    get: (function() {
     if (!this.lengthKnown) {
      this.cacheLength();
     }
     return this._length;
    })
   });
   Object.defineProperty(lazyArray, "chunkSize", {
    get: (function() {
     if (!this.lengthKnown) {
      this.cacheLength();
     }
     return this._chunkSize;
    })
   });
   var properties = {
    isDevice: false,
    contents: lazyArray
   };
  } else {
   var properties = {
    isDevice: false,
    url: url
   };
  }
  var node = FS.createFile(parent, name, properties, canRead, canWrite);
  if (properties.contents) {
   node.contents = properties.contents;
  } else if (properties.url) {
   node.contents = null;
   node.url = properties.url;
  }
  Object.defineProperty(node, "usedBytes", {
   get: (function() {
    return this.contents.length;
   })
  });
  var stream_ops = {};
  var keys = Object.keys(node.stream_ops);
  keys.forEach((function(key) {
   var fn = node.stream_ops[key];
   stream_ops[key] = function forceLoadLazyFile() {
    if (!FS.forceLoadFile(node)) {
     throw new FS.ErrnoError(ERRNO_CODES.EIO);
    }
    return fn.apply(null, arguments);
   };
  }));
  stream_ops.read = function stream_ops_read(stream, buffer, offset, length, position) {
   if (!FS.forceLoadFile(node)) {
    throw new FS.ErrnoError(ERRNO_CODES.EIO);
   }
   var contents = stream.node.contents;
   if (position >= contents.length) return 0;
   var size = Math.min(contents.length - position, length);
   assert(size >= 0);
   if (contents.slice) {
    for (var i = 0; i < size; i++) {
     buffer[offset + i] = contents[position + i];
    }
   } else {
    for (var i = 0; i < size; i++) {
     buffer[offset + i] = contents.get(position + i);
    }
   }
   return size;
  };
  node.stream_ops = stream_ops;
  return node;
 }),
 createPreloadedFile: (function(parent, name, url, canRead, canWrite, onload, onerror, dontCreateFile, canOwn, preFinish) {
  Browser.init();
  var fullname = name ? PATH.resolve(PATH.join2(parent, name)) : parent;
  var dep = getUniqueRunDependency("cp " + fullname);
  function processData(byteArray) {
   function finish(byteArray) {
    if (preFinish) preFinish();
    if (!dontCreateFile) {
     FS.createDataFile(parent, name, byteArray, canRead, canWrite, canOwn);
    }
    if (onload) onload();
    removeRunDependency(dep);
   }
   var handled = false;
   Module["preloadPlugins"].forEach((function(plugin) {
    if (handled) return;
    if (plugin["canHandle"](fullname)) {
     plugin["handle"](byteArray, fullname, finish, (function() {
      if (onerror) onerror();
      removeRunDependency(dep);
     }));
     handled = true;
    }
   }));
   if (!handled) finish(byteArray);
  }
  addRunDependency(dep);
  if (typeof url == "string") {
   Browser.asyncLoad(url, (function(byteArray) {
    processData(byteArray);
   }), onerror);
  } else {
   processData(url);
  }
 }),
 indexedDB: (function() {
  return window.indexedDB || window.mozIndexedDB || window.webkitIndexedDB || window.msIndexedDB;
 }),
 DB_NAME: (function() {
  return "EM_FS_" + window.location.pathname;
 }),
 DB_VERSION: 20,
 DB_STORE_NAME: "FILE_DATA",
 saveFilesToDB: (function(paths, onload, onerror) {
  onload = onload || (function() {});
  onerror = onerror || (function() {});
  var indexedDB = FS.indexedDB();
  try {
   var openRequest = indexedDB.open(FS.DB_NAME(), FS.DB_VERSION);
  } catch (e) {
   return onerror(e);
  }
  openRequest.onupgradeneeded = function openRequest_onupgradeneeded() {
   console.log("creating db");
   var db = openRequest.result;
   db.createObjectStore(FS.DB_STORE_NAME);
  };
  openRequest.onsuccess = function openRequest_onsuccess() {
   var db = openRequest.result;
   var transaction = db.transaction([ FS.DB_STORE_NAME ], "readwrite");
   var files = transaction.objectStore(FS.DB_STORE_NAME);
   var ok = 0, fail = 0, total = paths.length;
   function finish() {
    if (fail == 0) onload(); else onerror();
   }
   paths.forEach((function(path) {
    var putRequest = files.put(FS.analyzePath(path).object.contents, path);
    putRequest.onsuccess = function putRequest_onsuccess() {
     ok++;
     if (ok + fail == total) finish();
    };
    putRequest.onerror = function putRequest_onerror() {
     fail++;
     if (ok + fail == total) finish();
    };
   }));
   transaction.onerror = onerror;
  };
  openRequest.onerror = onerror;
 }),
 loadFilesFromDB: (function(paths, onload, onerror) {
  onload = onload || (function() {});
  onerror = onerror || (function() {});
  var indexedDB = FS.indexedDB();
  try {
   var openRequest = indexedDB.open(FS.DB_NAME(), FS.DB_VERSION);
  } catch (e) {
   return onerror(e);
  }
  openRequest.onupgradeneeded = onerror;
  openRequest.onsuccess = function openRequest_onsuccess() {
   var db = openRequest.result;
   try {
    var transaction = db.transaction([ FS.DB_STORE_NAME ], "readonly");
   } catch (e) {
    onerror(e);
    return;
   }
   var files = transaction.objectStore(FS.DB_STORE_NAME);
   var ok = 0, fail = 0, total = paths.length;
   function finish() {
    if (fail == 0) onload(); else onerror();
   }
   paths.forEach((function(path) {
    var getRequest = files.get(path);
    getRequest.onsuccess = function getRequest_onsuccess() {
     if (FS.analyzePath(path).exists) {
      FS.unlink(path);
     }
     FS.createDataFile(PATH.dirname(path), PATH.basename(path), getRequest.result, true, true, true);
     ok++;
     if (ok + fail == total) finish();
    };
    getRequest.onerror = function getRequest_onerror() {
     fail++;
     if (ok + fail == total) finish();
    };
   }));
   transaction.onerror = onerror;
  };
  openRequest.onerror = onerror;
 })
};
var SYSCALLS = {
 DEFAULT_POLLMASK: 5,
 mappings: {},
 umask: 511,
 calculateAt: (function(dirfd, path) {
  if (path[0] !== "/") {
   var dir;
   if (dirfd === -100) {
    dir = FS.cwd();
   } else {
    var dirstream = FS.getStream(dirfd);
    if (!dirstream) throw new FS.ErrnoError(ERRNO_CODES.EBADF);
    dir = dirstream.path;
   }
   path = PATH.join2(dir, path);
  }
  return path;
 }),
 doStat: (function(func, path, buf) {
  try {
   var stat = func(path);
  } catch (e) {
   if (e && e.node && PATH.normalize(path) !== PATH.normalize(FS.getPath(e.node))) {
    return -ERRNO_CODES.ENOTDIR;
   }
   throw e;
  }
  HEAP32[buf >> 2] = stat.dev;
  HEAP32[buf + 4 >> 2] = 0;
  HEAP32[buf + 8 >> 2] = stat.ino;
  HEAP32[buf + 12 >> 2] = stat.mode;
  HEAP32[buf + 16 >> 2] = stat.nlink;
  HEAP32[buf + 20 >> 2] = stat.uid;
  HEAP32[buf + 24 >> 2] = stat.gid;
  HEAP32[buf + 28 >> 2] = stat.rdev;
  HEAP32[buf + 32 >> 2] = 0;
  HEAP32[buf + 36 >> 2] = stat.size;
  HEAP32[buf + 40 >> 2] = 4096;
  HEAP32[buf + 44 >> 2] = stat.blocks;
  HEAP32[buf + 48 >> 2] = stat.atime.getTime() / 1e3 | 0;
  HEAP32[buf + 52 >> 2] = 0;
  HEAP32[buf + 56 >> 2] = stat.mtime.getTime() / 1e3 | 0;
  HEAP32[buf + 60 >> 2] = 0;
  HEAP32[buf + 64 >> 2] = stat.ctime.getTime() / 1e3 | 0;
  HEAP32[buf + 68 >> 2] = 0;
  HEAP32[buf + 72 >> 2] = stat.ino;
  return 0;
 }),
 doMsync: (function(addr, stream, len, flags) {
  var buffer = new Uint8Array(HEAPU8.buffer, addr, len);
  FS.msync(stream, buffer, 0, len, flags);
 }),
 doMkdir: (function(path, mode) {
  path = PATH.normalize(path);
  if (path[path.length - 1] === "/") path = path.substr(0, path.length - 1);
  FS.mkdir(path, mode, 0);
  return 0;
 }),
 doMknod: (function(path, mode, dev) {
  switch (mode & 61440) {
  case 32768:
  case 8192:
  case 24576:
  case 4096:
  case 49152:
   break;
  default:
   return -ERRNO_CODES.EINVAL;
  }
  FS.mknod(path, mode, dev);
  return 0;
 }),
 doReadlink: (function(path, buf, bufsize) {
  if (bufsize <= 0) return -ERRNO_CODES.EINVAL;
  var ret = FS.readlink(path);
  ret = ret.slice(0, Math.max(0, bufsize));
  writeStringToMemory(ret, buf, true);
  return ret.length;
 }),
 doAccess: (function(path, amode) {
  if (amode & ~7) {
   return -ERRNO_CODES.EINVAL;
  }
  var node;
  var lookup = FS.lookupPath(path, {
   follow: true
  });
  node = lookup.node;
  var perms = "";
  if (amode & 4) perms += "r";
  if (amode & 2) perms += "w";
  if (amode & 1) perms += "x";
  if (perms && FS.nodePermissions(node, perms)) {
   return -ERRNO_CODES.EACCES;
  }
  return 0;
 }),
 doDup: (function(path, flags, suggestFD) {
  var suggest = FS.getStream(suggestFD);
  if (suggest) FS.close(suggest);
  return FS.open(path, flags, 0, suggestFD, suggestFD).fd;
 }),
 doReadv: (function(stream, iov, iovcnt, offset) {
  var ret = 0;
  for (var i = 0; i < iovcnt; i++) {
   var ptr = HEAP32[iov + i * 8 >> 2];
   var len = HEAP32[iov + (i * 8 + 4) >> 2];
   var curr = FS.read(stream, HEAP8, ptr, len, offset);
   if (curr < 0) return -1;
   ret += curr;
   if (curr < len) break;
  }
  return ret;
 }),
 doWritev: (function(stream, iov, iovcnt, offset) {
  var ret = 0;
  for (var i = 0; i < iovcnt; i++) {
   var ptr = HEAP32[iov + i * 8 >> 2];
   var len = HEAP32[iov + (i * 8 + 4) >> 2];
   var curr = FS.write(stream, HEAP8, ptr, len, offset);
   if (curr < 0) return -1;
   ret += curr;
  }
  return ret;
 }),
 varargs: 0,
 get: (function(varargs) {
  SYSCALLS.varargs += 4;
  var ret = HEAP32[SYSCALLS.varargs - 4 >> 2];
  return ret;
 }),
 getStr: (function() {
  var ret = Pointer_stringify(SYSCALLS.get());
  return ret;
 }),
 getStreamFromFD: (function() {
  var stream = FS.getStream(SYSCALLS.get());
  if (!stream) throw new FS.ErrnoError(ERRNO_CODES.EBADF);
  return stream;
 }),
 getSocketFromFD: (function() {
  var socket = SOCKFS.getSocket(SYSCALLS.get());
  if (!socket) throw new FS.ErrnoError(ERRNO_CODES.EBADF);
  return socket;
 }),
 getSocketAddress: (function(allowNull) {
  var addrp = SYSCALLS.get(), addrlen = SYSCALLS.get();
  if (allowNull && addrp === 0) return null;
  var info = __read_sockaddr(addrp, addrlen);
  if (info.errno) throw new FS.ErrnoError(info.errno);
  info.addr = DNS.lookup_addr(info.addr) || info.addr;
  return info;
 }),
 get64: (function() {
  var low = SYSCALLS.get(), high = SYSCALLS.get();
  if (low >= 0) assert(high === 0); else assert(high === -1);
  return low;
 }),
 getZero: (function() {
  assert(SYSCALLS.get() === 0);
 })
};
function ___syscall6(which, varargs) {
 SYSCALLS.varargs = varargs;
 try {
  var stream = SYSCALLS.getStreamFromFD();
  FS.close(stream);
  return 0;
 } catch (e) {
  if (typeof FS === "undefined" || !(e instanceof FS.ErrnoError)) abort(e);
  return -e.errno;
 }
}
Module["_i64Add"] = _i64Add;
function _sbrk(bytes) {
 var self = _sbrk;
 if (!self.called) {
  DYNAMICTOP = alignMemoryPage(DYNAMICTOP);
  self.called = true;
  assert(Runtime.dynamicAlloc);
  self.alloc = Runtime.dynamicAlloc;
  Runtime.dynamicAlloc = (function() {
   abort("cannot dynamically allocate, sbrk now has control");
  });
 }
 var ret = DYNAMICTOP;
 if (bytes != 0) {
  var success = self.alloc(bytes);
  if (!success) return -1 >>> 0;
 }
 return ret;
}
var _BItoD = true;
function _emscripten_memcpy_big(dest, src, num) {
 HEAPU8.set(HEAPU8.subarray(src, src + num), dest);
 return dest;
}
Module["_memcpy"] = _memcpy;
function _emscripten_set_main_loop_timing(mode, value) {
 Browser.mainLoop.timingMode = mode;
 Browser.mainLoop.timingValue = value;
 if (!Browser.mainLoop.func) {
  return 1;
 }
 if (mode == 0) {
  Browser.mainLoop.scheduler = function Browser_mainLoop_scheduler_setTimeout() {
   setTimeout(Browser.mainLoop.runner, value);
  };
  Browser.mainLoop.method = "timeout";
 } else if (mode == 1) {
  Browser.mainLoop.scheduler = function Browser_mainLoop_scheduler_rAF() {
   Browser.requestAnimationFrame(Browser.mainLoop.runner);
  };
  Browser.mainLoop.method = "rAF";
 } else if (mode == 2) {
  if (!window["setImmediate"]) {
   var setImmediates = [];
   var emscriptenMainLoopMessageId = "__emcc";
   function Browser_setImmediate_messageHandler(event) {
    if (event.source === window && event.data === emscriptenMainLoopMessageId) {
     event.stopPropagation();
     setImmediates.shift()();
    }
   }
   window.addEventListener("message", Browser_setImmediate_messageHandler, true);
   window["setImmediate"] = function Browser_emulated_setImmediate(func) {
    setImmediates.push(func);
    window.postMessage(emscriptenMainLoopMessageId, "*");
   };
  }
  Browser.mainLoop.scheduler = function Browser_mainLoop_scheduler_setImmediate() {
   window["setImmediate"](Browser.mainLoop.runner);
  };
  Browser.mainLoop.method = "immediate";
 }
 return 0;
}
function _emscripten_set_main_loop(func, fps, simulateInfiniteLoop, arg, noSetTiming) {
 Module["noExitRuntime"] = true;
 assert(!Browser.mainLoop.func, "emscripten_set_main_loop: there can only be one main loop function at once: call emscripten_cancel_main_loop to cancel the previous one before setting a new one with different parameters.");
 Browser.mainLoop.func = func;
 Browser.mainLoop.arg = arg;
 var thisMainLoopId = Browser.mainLoop.currentlyRunningMainloop;
 Browser.mainLoop.runner = function Browser_mainLoop_runner() {
  if (ABORT) return;
  if (Browser.mainLoop.queue.length > 0) {
   var start = Date.now();
   var blocker = Browser.mainLoop.queue.shift();
   blocker.func(blocker.arg);
   if (Browser.mainLoop.remainingBlockers) {
    var remaining = Browser.mainLoop.remainingBlockers;
    var next = remaining % 1 == 0 ? remaining - 1 : Math.floor(remaining);
    if (blocker.counted) {
     Browser.mainLoop.remainingBlockers = next;
    } else {
     next = next + .5;
     Browser.mainLoop.remainingBlockers = (8 * remaining + next) / 9;
    }
   }
   console.log('main loop blocker "' + blocker.name + '" took ' + (Date.now() - start) + " ms");
   Browser.mainLoop.updateStatus();
   setTimeout(Browser.mainLoop.runner, 0);
   return;
  }
  if (thisMainLoopId < Browser.mainLoop.currentlyRunningMainloop) return;
  Browser.mainLoop.currentFrameNumber = Browser.mainLoop.currentFrameNumber + 1 | 0;
  if (Browser.mainLoop.timingMode == 1 && Browser.mainLoop.timingValue > 1 && Browser.mainLoop.currentFrameNumber % Browser.mainLoop.timingValue != 0) {
   Browser.mainLoop.scheduler();
   return;
  }
  if (Browser.mainLoop.method === "timeout" && Module.ctx) {
   Module.printErr("Looks like you are rendering without using requestAnimationFrame for the main loop. You should use 0 for the frame rate in emscripten_set_main_loop in order to use requestAnimationFrame, as that can greatly improve your frame rates!");
   Browser.mainLoop.method = "";
  }
  Browser.mainLoop.runIter((function() {
   if (typeof arg !== "undefined") {
    Runtime.dynCall("vi", func, [ arg ]);
   } else {
    Runtime.dynCall("v", func);
   }
  }));
  if (thisMainLoopId < Browser.mainLoop.currentlyRunningMainloop) return;
  if (typeof SDL === "object" && SDL.audio && SDL.audio.queueNewAudioData) SDL.audio.queueNewAudioData();
  Browser.mainLoop.scheduler();
 };
 if (!noSetTiming) {
  if (fps && fps > 0) _emscripten_set_main_loop_timing(0, 1e3 / fps); else _emscripten_set_main_loop_timing(1, 1);
  Browser.mainLoop.scheduler();
 }
 if (simulateInfiniteLoop) {
  throw "SimulateInfiniteLoop";
 }
}
var Browser = {
 mainLoop: {
  scheduler: null,
  method: "",
  currentlyRunningMainloop: 0,
  func: null,
  arg: 0,
  timingMode: 0,
  timingValue: 0,
  currentFrameNumber: 0,
  queue: [],
  pause: (function() {
   Browser.mainLoop.scheduler = null;
   Browser.mainLoop.currentlyRunningMainloop++;
  }),
  resume: (function() {
   Browser.mainLoop.currentlyRunningMainloop++;
   var timingMode = Browser.mainLoop.timingMode;
   var timingValue = Browser.mainLoop.timingValue;
   var func = Browser.mainLoop.func;
   Browser.mainLoop.func = null;
   _emscripten_set_main_loop(func, 0, false, Browser.mainLoop.arg, true);
   _emscripten_set_main_loop_timing(timingMode, timingValue);
   Browser.mainLoop.scheduler();
  }),
  updateStatus: (function() {
   if (Module["setStatus"]) {
    var message = Module["statusMessage"] || "Please wait...";
    var remaining = Browser.mainLoop.remainingBlockers;
    var expected = Browser.mainLoop.expectedBlockers;
    if (remaining) {
     if (remaining < expected) {
      Module["setStatus"](message + " (" + (expected - remaining) + "/" + expected + ")");
     } else {
      Module["setStatus"](message);
     }
    } else {
     Module["setStatus"]("");
    }
   }
  }),
  runIter: (function(func) {
   if (ABORT) return;
   if (Module["preMainLoop"]) {
    var preRet = Module["preMainLoop"]();
    if (preRet === false) {
     return;
    }
   }
   try {
    func();
   } catch (e) {
    if (e instanceof ExitStatus) {
     return;
    } else {
     if (e && typeof e === "object" && e.stack) Module.printErr("exception thrown: " + [ e, e.stack ]);
     throw e;
    }
   }
   if (Module["postMainLoop"]) Module["postMainLoop"]();
  })
 },
 isFullScreen: false,
 pointerLock: false,
 moduleContextCreatedCallbacks: [],
 workers: [],
 init: (function() {
  if (!Module["preloadPlugins"]) Module["preloadPlugins"] = [];
  if (Browser.initted) return;
  Browser.initted = true;
  try {
   new Blob;
   Browser.hasBlobConstructor = true;
  } catch (e) {
   Browser.hasBlobConstructor = false;
   console.log("warning: no blob constructor, cannot create blobs with mimetypes");
  }
  Browser.BlobBuilder = typeof MozBlobBuilder != "undefined" ? MozBlobBuilder : typeof WebKitBlobBuilder != "undefined" ? WebKitBlobBuilder : !Browser.hasBlobConstructor ? console.log("warning: no BlobBuilder") : null;
  Browser.URLObject = typeof window != "undefined" ? window.URL ? window.URL : window.webkitURL : undefined;
  if (!Module.noImageDecoding && typeof Browser.URLObject === "undefined") {
   console.log("warning: Browser does not support creating object URLs. Built-in browser image decoding will not be available.");
   Module.noImageDecoding = true;
  }
  var imagePlugin = {};
  imagePlugin["canHandle"] = function imagePlugin_canHandle(name) {
   return !Module.noImageDecoding && /\.(jpg|jpeg|png|bmp)$/i.test(name);
  };
  imagePlugin["handle"] = function imagePlugin_handle(byteArray, name, onload, onerror) {
   var b = null;
   if (Browser.hasBlobConstructor) {
    try {
     b = new Blob([ byteArray ], {
      type: Browser.getMimetype(name)
     });
     if (b.size !== byteArray.length) {
      b = new Blob([ (new Uint8Array(byteArray)).buffer ], {
       type: Browser.getMimetype(name)
      });
     }
    } catch (e) {
     Runtime.warnOnce("Blob constructor present but fails: " + e + "; falling back to blob builder");
    }
   }
   if (!b) {
    var bb = new Browser.BlobBuilder;
    bb.append((new Uint8Array(byteArray)).buffer);
    b = bb.getBlob();
   }
   var url = Browser.URLObject.createObjectURL(b);
   var img = new Image;
   img.onload = function img_onload() {
    assert(img.complete, "Image " + name + " could not be decoded");
    var canvas = document.createElement("canvas");
    canvas.width = img.width;
    canvas.height = img.height;
    var ctx = canvas.getContext("2d");
    ctx.drawImage(img, 0, 0);
    Module["preloadedImages"][name] = canvas;
    Browser.URLObject.revokeObjectURL(url);
    if (onload) onload(byteArray);
   };
   img.onerror = function img_onerror(event) {
    console.log("Image " + url + " could not be decoded");
    if (onerror) onerror();
   };
   img.src = url;
  };
  Module["preloadPlugins"].push(imagePlugin);
  var audioPlugin = {};
  audioPlugin["canHandle"] = function audioPlugin_canHandle(name) {
   return !Module.noAudioDecoding && name.substr(-4) in {
    ".ogg": 1,
    ".wav": 1,
    ".mp3": 1
   };
  };
  audioPlugin["handle"] = function audioPlugin_handle(byteArray, name, onload, onerror) {
   var done = false;
   function finish(audio) {
    if (done) return;
    done = true;
    Module["preloadedAudios"][name] = audio;
    if (onload) onload(byteArray);
   }
   function fail() {
    if (done) return;
    done = true;
    Module["preloadedAudios"][name] = new Audio;
    if (onerror) onerror();
   }
   if (Browser.hasBlobConstructor) {
    try {
     var b = new Blob([ byteArray ], {
      type: Browser.getMimetype(name)
     });
    } catch (e) {
     return fail();
    }
    var url = Browser.URLObject.createObjectURL(b);
    var audio = new Audio;
    audio.addEventListener("canplaythrough", (function() {
     finish(audio);
    }), false);
    audio.onerror = function audio_onerror(event) {
     if (done) return;
     console.log("warning: browser could not fully decode audio " + name + ", trying slower base64 approach");
     function encode64(data) {
      var BASE = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
      var PAD = "=";
      var ret = "";
      var leftchar = 0;
      var leftbits = 0;
      for (var i = 0; i < data.length; i++) {
       leftchar = leftchar << 8 | data[i];
       leftbits += 8;
       while (leftbits >= 6) {
        var curr = leftchar >> leftbits - 6 & 63;
        leftbits -= 6;
        ret += BASE[curr];
       }
      }
      if (leftbits == 2) {
       ret += BASE[(leftchar & 3) << 4];
       ret += PAD + PAD;
      } else if (leftbits == 4) {
       ret += BASE[(leftchar & 15) << 2];
       ret += PAD;
      }
      return ret;
     }
     audio.src = "data:audio/x-" + name.substr(-3) + ";base64," + encode64(byteArray);
     finish(audio);
    };
    audio.src = url;
    Browser.safeSetTimeout((function() {
     finish(audio);
    }), 1e4);
   } else {
    return fail();
   }
  };
  Module["preloadPlugins"].push(audioPlugin);
  var canvas = Module["canvas"];
  function pointerLockChange() {
   Browser.pointerLock = document["pointerLockElement"] === canvas || document["mozPointerLockElement"] === canvas || document["webkitPointerLockElement"] === canvas || document["msPointerLockElement"] === canvas;
  }
  if (canvas) {
   canvas.requestPointerLock = canvas["requestPointerLock"] || canvas["mozRequestPointerLock"] || canvas["webkitRequestPointerLock"] || canvas["msRequestPointerLock"] || (function() {});
   canvas.exitPointerLock = document["exitPointerLock"] || document["mozExitPointerLock"] || document["webkitExitPointerLock"] || document["msExitPointerLock"] || (function() {});
   canvas.exitPointerLock = canvas.exitPointerLock.bind(document);
   document.addEventListener("pointerlockchange", pointerLockChange, false);
   document.addEventListener("mozpointerlockchange", pointerLockChange, false);
   document.addEventListener("webkitpointerlockchange", pointerLockChange, false);
   document.addEventListener("mspointerlockchange", pointerLockChange, false);
   if (Module["elementPointerLock"]) {
    canvas.addEventListener("click", (function(ev) {
     if (!Browser.pointerLock && canvas.requestPointerLock) {
      canvas.requestPointerLock();
      ev.preventDefault();
     }
    }), false);
   }
  }
 }),
 createContext: (function(canvas, useWebGL, setInModule, webGLContextAttributes) {
  if (useWebGL && Module.ctx && canvas == Module.canvas) return Module.ctx;
  var ctx;
  var contextHandle;
  if (useWebGL) {
   var contextAttributes = {
    antialias: false,
    alpha: false
   };
   if (webGLContextAttributes) {
    for (var attribute in webGLContextAttributes) {
     contextAttributes[attribute] = webGLContextAttributes[attribute];
    }
   }
   contextHandle = GL.createContext(canvas, contextAttributes);
   if (contextHandle) {
    ctx = GL.getContext(contextHandle).GLctx;
   }
   canvas.style.backgroundColor = "black";
  } else {
   ctx = canvas.getContext("2d");
  }
  if (!ctx) return null;
  if (setInModule) {
   if (!useWebGL) assert(typeof GLctx === "undefined", "cannot set in module if GLctx is used, but we are a non-GL context that would replace it");
   Module.ctx = ctx;
   if (useWebGL) GL.makeContextCurrent(contextHandle);
   Module.useWebGL = useWebGL;
   Browser.moduleContextCreatedCallbacks.forEach((function(callback) {
    callback();
   }));
   Browser.init();
  }
  return ctx;
 }),
 destroyContext: (function(canvas, useWebGL, setInModule) {}),
 fullScreenHandlersInstalled: false,
 lockPointer: undefined,
 resizeCanvas: undefined,
 requestFullScreen: (function(lockPointer, resizeCanvas, vrDevice) {
  Browser.lockPointer = lockPointer;
  Browser.resizeCanvas = resizeCanvas;
  Browser.vrDevice = vrDevice;
  if (typeof Browser.lockPointer === "undefined") Browser.lockPointer = true;
  if (typeof Browser.resizeCanvas === "undefined") Browser.resizeCanvas = false;
  if (typeof Browser.vrDevice === "undefined") Browser.vrDevice = null;
  var canvas = Module["canvas"];
  function fullScreenChange() {
   Browser.isFullScreen = false;
   var canvasContainer = canvas.parentNode;
   if ((document["webkitFullScreenElement"] || document["webkitFullscreenElement"] || document["mozFullScreenElement"] || document["mozFullscreenElement"] || document["fullScreenElement"] || document["fullscreenElement"] || document["msFullScreenElement"] || document["msFullscreenElement"] || document["webkitCurrentFullScreenElement"]) === canvasContainer) {
    canvas.cancelFullScreen = document["cancelFullScreen"] || document["mozCancelFullScreen"] || document["webkitCancelFullScreen"] || document["msExitFullscreen"] || document["exitFullscreen"] || (function() {});
    canvas.cancelFullScreen = canvas.cancelFullScreen.bind(document);
    if (Browser.lockPointer) canvas.requestPointerLock();
    Browser.isFullScreen = true;
    if (Browser.resizeCanvas) Browser.setFullScreenCanvasSize();
   } else {
    canvasContainer.parentNode.insertBefore(canvas, canvasContainer);
    canvasContainer.parentNode.removeChild(canvasContainer);
    if (Browser.resizeCanvas) Browser.setWindowedCanvasSize();
   }
   if (Module["onFullScreen"]) Module["onFullScreen"](Browser.isFullScreen);
   Browser.updateCanvasDimensions(canvas);
  }
  if (!Browser.fullScreenHandlersInstalled) {
   Browser.fullScreenHandlersInstalled = true;
   document.addEventListener("fullscreenchange", fullScreenChange, false);
   document.addEventListener("mozfullscreenchange", fullScreenChange, false);
   document.addEventListener("webkitfullscreenchange", fullScreenChange, false);
   document.addEventListener("MSFullscreenChange", fullScreenChange, false);
  }
  var canvasContainer = document.createElement("div");
  canvas.parentNode.insertBefore(canvasContainer, canvas);
  canvasContainer.appendChild(canvas);
  canvasContainer.requestFullScreen = canvasContainer["requestFullScreen"] || canvasContainer["mozRequestFullScreen"] || canvasContainer["msRequestFullscreen"] || (canvasContainer["webkitRequestFullScreen"] ? (function() {
   canvasContainer["webkitRequestFullScreen"](Element["ALLOW_KEYBOARD_INPUT"]);
  }) : null);
  if (vrDevice) {
   canvasContainer.requestFullScreen({
    vrDisplay: vrDevice
   });
  } else {
   canvasContainer.requestFullScreen();
  }
 }),
 nextRAF: 0,
 fakeRequestAnimationFrame: (function(func) {
  var now = Date.now();
  if (Browser.nextRAF === 0) {
   Browser.nextRAF = now + 1e3 / 60;
  } else {
   while (now + 2 >= Browser.nextRAF) {
    Browser.nextRAF += 1e3 / 60;
   }
  }
  var delay = Math.max(Browser.nextRAF - now, 0);
  setTimeout(func, delay);
 }),
 requestAnimationFrame: function requestAnimationFrame(func) {
  if (typeof window === "undefined") {
   Browser.fakeRequestAnimationFrame(func);
  } else {
   if (!window.requestAnimationFrame) {
    window.requestAnimationFrame = window["requestAnimationFrame"] || window["mozRequestAnimationFrame"] || window["webkitRequestAnimationFrame"] || window["msRequestAnimationFrame"] || window["oRequestAnimationFrame"] || Browser.fakeRequestAnimationFrame;
   }
   window.requestAnimationFrame(func);
  }
 },
 safeCallback: (function(func) {
  return (function() {
   if (!ABORT) return func.apply(null, arguments);
  });
 }),
 allowAsyncCallbacks: true,
 queuedAsyncCallbacks: [],
 pauseAsyncCallbacks: (function() {
  Browser.allowAsyncCallbacks = false;
 }),
 resumeAsyncCallbacks: (function() {
  Browser.allowAsyncCallbacks = true;
  if (Browser.queuedAsyncCallbacks.length > 0) {
   var callbacks = Browser.queuedAsyncCallbacks;
   Browser.queuedAsyncCallbacks = [];
   callbacks.forEach((function(func) {
    func();
   }));
  }
 }),
 safeRequestAnimationFrame: (function(func) {
  return Browser.requestAnimationFrame((function() {
   if (ABORT) return;
   if (Browser.allowAsyncCallbacks) {
    func();
   } else {
    Browser.queuedAsyncCallbacks.push(func);
   }
  }));
 }),
 safeSetTimeout: (function(func, timeout) {
  Module["noExitRuntime"] = true;
  return setTimeout((function() {
   if (ABORT) return;
   if (Browser.allowAsyncCallbacks) {
    func();
   } else {
    Browser.queuedAsyncCallbacks.push(func);
   }
  }), timeout);
 }),
 safeSetInterval: (function(func, timeout) {
  Module["noExitRuntime"] = true;
  return setInterval((function() {
   if (ABORT) return;
   if (Browser.allowAsyncCallbacks) {
    func();
   }
  }), timeout);
 }),
 getMimetype: (function(name) {
  return {
   "jpg": "image/jpeg",
   "jpeg": "image/jpeg",
   "png": "image/png",
   "bmp": "image/bmp",
   "ogg": "audio/ogg",
   "wav": "audio/wav",
   "mp3": "audio/mpeg"
  }[name.substr(name.lastIndexOf(".") + 1)];
 }),
 getUserMedia: (function(func) {
  if (!window.getUserMedia) {
   window.getUserMedia = navigator["getUserMedia"] || navigator["mozGetUserMedia"];
  }
  window.getUserMedia(func);
 }),
 getMovementX: (function(event) {
  return event["movementX"] || event["mozMovementX"] || event["webkitMovementX"] || 0;
 }),
 getMovementY: (function(event) {
  return event["movementY"] || event["mozMovementY"] || event["webkitMovementY"] || 0;
 }),
 getMouseWheelDelta: (function(event) {
  var delta = 0;
  switch (event.type) {
  case "DOMMouseScroll":
   delta = event.detail;
   break;
  case "mousewheel":
   delta = event.wheelDelta;
   break;
  case "wheel":
   delta = event["deltaY"];
   break;
  default:
   throw "unrecognized mouse wheel event: " + event.type;
  }
  return delta;
 }),
 mouseX: 0,
 mouseY: 0,
 mouseMovementX: 0,
 mouseMovementY: 0,
 touches: {},
 lastTouches: {},
 calculateMouseEvent: (function(event) {
  if (Browser.pointerLock) {
   if (event.type != "mousemove" && "mozMovementX" in event) {
    Browser.mouseMovementX = Browser.mouseMovementY = 0;
   } else {
    Browser.mouseMovementX = Browser.getMovementX(event);
    Browser.mouseMovementY = Browser.getMovementY(event);
   }
   if (typeof SDL != "undefined") {
    Browser.mouseX = SDL.mouseX + Browser.mouseMovementX;
    Browser.mouseY = SDL.mouseY + Browser.mouseMovementY;
   } else {
    Browser.mouseX += Browser.mouseMovementX;
    Browser.mouseY += Browser.mouseMovementY;
   }
  } else {
   var rect = Module["canvas"].getBoundingClientRect();
   var cw = Module["canvas"].width;
   var ch = Module["canvas"].height;
   var scrollX = typeof window.scrollX !== "undefined" ? window.scrollX : window.pageXOffset;
   var scrollY = typeof window.scrollY !== "undefined" ? window.scrollY : window.pageYOffset;
   if (event.type === "touchstart" || event.type === "touchend" || event.type === "touchmove") {
    var touch = event.touch;
    if (touch === undefined) {
     return;
    }
    var adjustedX = touch.pageX - (scrollX + rect.left);
    var adjustedY = touch.pageY - (scrollY + rect.top);
    adjustedX = adjustedX * (cw / rect.width);
    adjustedY = adjustedY * (ch / rect.height);
    var coords = {
     x: adjustedX,
     y: adjustedY
    };
    if (event.type === "touchstart") {
     Browser.lastTouches[touch.identifier] = coords;
     Browser.touches[touch.identifier] = coords;
    } else if (event.type === "touchend" || event.type === "touchmove") {
     var last = Browser.touches[touch.identifier];
     if (!last) last = coords;
     Browser.lastTouches[touch.identifier] = last;
     Browser.touches[touch.identifier] = coords;
    }
    return;
   }
   var x = event.pageX - (scrollX + rect.left);
   var y = event.pageY - (scrollY + rect.top);
   x = x * (cw / rect.width);
   y = y * (ch / rect.height);
   Browser.mouseMovementX = x - Browser.mouseX;
   Browser.mouseMovementY = y - Browser.mouseY;
   Browser.mouseX = x;
   Browser.mouseY = y;
  }
 }),
 xhrLoad: (function(url, onload, onerror) {
  var xhr = new XMLHttpRequest;
  xhr.open("GET", url, true);
  xhr.responseType = "arraybuffer";
  xhr.onload = function xhr_onload() {
   if (xhr.status == 200 || xhr.status == 0 && xhr.response) {
    onload(xhr.response);
   } else {
    onerror();
   }
  };
  xhr.onerror = onerror;
  xhr.send(null);
 }),
 asyncLoad: (function(url, onload, onerror, noRunDep) {
  Browser.xhrLoad(url, (function(arrayBuffer) {
   assert(arrayBuffer, 'Loading data file "' + url + '" failed (no arrayBuffer).');
   onload(new Uint8Array(arrayBuffer));
   if (!noRunDep) removeRunDependency("al " + url);
  }), (function(event) {
   if (onerror) {
    onerror();
   } else {
    throw 'Loading data file "' + url + '" failed.';
   }
  }));
  if (!noRunDep) addRunDependency("al " + url);
 }),
 resizeListeners: [],
 updateResizeListeners: (function() {
  var canvas = Module["canvas"];
  Browser.resizeListeners.forEach((function(listener) {
   listener(canvas.width, canvas.height);
  }));
 }),
 setCanvasSize: (function(width, height, noUpdates) {
  var canvas = Module["canvas"];
  Browser.updateCanvasDimensions(canvas, width, height);
  if (!noUpdates) Browser.updateResizeListeners();
 }),
 windowedWidth: 0,
 windowedHeight: 0,
 setFullScreenCanvasSize: (function() {
  if (typeof SDL != "undefined") {
   var flags = HEAPU32[SDL.screen + Runtime.QUANTUM_SIZE * 0 >> 2];
   flags = flags | 8388608;
   HEAP32[SDL.screen + Runtime.QUANTUM_SIZE * 0 >> 2] = flags;
  }
  Browser.updateResizeListeners();
 }),
 setWindowedCanvasSize: (function() {
  if (typeof SDL != "undefined") {
   var flags = HEAPU32[SDL.screen + Runtime.QUANTUM_SIZE * 0 >> 2];
   flags = flags & ~8388608;
   HEAP32[SDL.screen + Runtime.QUANTUM_SIZE * 0 >> 2] = flags;
  }
  Browser.updateResizeListeners();
 }),
 updateCanvasDimensions: (function(canvas, wNative, hNative) {
  if (wNative && hNative) {
   canvas.widthNative = wNative;
   canvas.heightNative = hNative;
  } else {
   wNative = canvas.widthNative;
   hNative = canvas.heightNative;
  }
  var w = wNative;
  var h = hNative;
  if (Module["forcedAspectRatio"] && Module["forcedAspectRatio"] > 0) {
   if (w / h < Module["forcedAspectRatio"]) {
    w = Math.round(h * Module["forcedAspectRatio"]);
   } else {
    h = Math.round(w / Module["forcedAspectRatio"]);
   }
  }
  if ((document["webkitFullScreenElement"] || document["webkitFullscreenElement"] || document["mozFullScreenElement"] || document["mozFullscreenElement"] || document["fullScreenElement"] || document["fullscreenElement"] || document["msFullScreenElement"] || document["msFullscreenElement"] || document["webkitCurrentFullScreenElement"]) === canvas.parentNode && typeof screen != "undefined") {
   var factor = Math.min(screen.width / w, screen.height / h);
   w = Math.round(w * factor);
   h = Math.round(h * factor);
  }
  if (Browser.resizeCanvas) {
   if (canvas.width != w) canvas.width = w;
   if (canvas.height != h) canvas.height = h;
   if (typeof canvas.style != "undefined") {
    canvas.style.removeProperty("width");
    canvas.style.removeProperty("height");
   }
  } else {
   if (canvas.width != wNative) canvas.width = wNative;
   if (canvas.height != hNative) canvas.height = hNative;
   if (typeof canvas.style != "undefined") {
    if (w != wNative || h != hNative) {
     canvas.style.setProperty("width", w + "px", "important");
     canvas.style.setProperty("height", h + "px", "important");
    } else {
     canvas.style.removeProperty("width");
     canvas.style.removeProperty("height");
    }
   }
  }
 }),
 wgetRequests: {},
 nextWgetRequestHandle: 0,
 getNextWgetRequestHandle: (function() {
  var handle = Browser.nextWgetRequestHandle;
  Browser.nextWgetRequestHandle++;
  return handle;
 })
};
function _time(ptr) {
 var ret = Date.now() / 1e3 | 0;
 if (ptr) {
  HEAP32[ptr >> 2] = ret;
 }
 return ret;
}
Module["_llvm_bswap_i32"] = _llvm_bswap_i32;
function ___syscall140(which, varargs) {
 SYSCALLS.varargs = varargs;
 try {
  var stream = SYSCALLS.getStreamFromFD(), offset_high = SYSCALLS.get(), offset_low = SYSCALLS.get(), result = SYSCALLS.get(), whence = SYSCALLS.get();
  var offset = offset_low;
  assert(offset_high === 0);
  FS.llseek(stream, offset, whence);
  HEAP32[result >> 2] = stream.position;
  if (stream.getdents && offset === 0 && whence === 0) stream.getdents = null;
  return 0;
 } catch (e) {
  if (typeof FS === "undefined" || !(e instanceof FS.ErrnoError)) abort(e);
  return -e.errno;
 }
}
function ___syscall146(which, varargs) {
 SYSCALLS.varargs = varargs;
 try {
  var stream = SYSCALLS.getStreamFromFD(), iov = SYSCALLS.get(), iovcnt = SYSCALLS.get();
  return SYSCALLS.doWritev(stream, iov, iovcnt);
 } catch (e) {
  if (typeof FS === "undefined" || !(e instanceof FS.ErrnoError)) abort(e);
  return -e.errno;
 }
}
function ___syscall54(which, varargs) {
 SYSCALLS.varargs = varargs;
 try {
  var stream = SYSCALLS.getStreamFromFD(), op = SYSCALLS.get();
  switch (op) {
  case 21505:
   {
    if (!stream.tty) return -ERRNO_CODES.ENOTTY;
    return 0;
   }
  case 21506:
   {
    if (!stream.tty) return -ERRNO_CODES.ENOTTY;
    return 0;
   }
  case 21519:
   {
    if (!stream.tty) return -ERRNO_CODES.ENOTTY;
    var argp = SYSCALLS.get();
    HEAP32[argp >> 2] = 0;
    return 0;
   }
  case 21520:
   {
    if (!stream.tty) return -ERRNO_CODES.ENOTTY;
    return -ERRNO_CODES.EINVAL;
   }
  case 21531:
   {
    var argp = SYSCALLS.get();
    return FS.ioctl(stream, op, argp);
   }
  default:
   abort("bad ioctl syscall " + op);
  }
 } catch (e) {
  if (typeof FS === "undefined" || !(e instanceof FS.ErrnoError)) abort(e);
  return -e.errno;
 }
}
FS.staticInit();
__ATINIT__.unshift((function() {
 if (!Module["noFSInit"] && !FS.init.initialized) FS.init();
}));
__ATMAIN__.push((function() {
 FS.ignorePermissions = false;
}));
__ATEXIT__.push((function() {
 FS.quit();
}));
Module["FS_createFolder"] = FS.createFolder;
Module["FS_createPath"] = FS.createPath;
Module["FS_createDataFile"] = FS.createDataFile;
Module["FS_createPreloadedFile"] = FS.createPreloadedFile;
Module["FS_createLazyFile"] = FS.createLazyFile;
Module["FS_createLink"] = FS.createLink;
Module["FS_createDevice"] = FS.createDevice;
Module["FS_unlink"] = FS.unlink;
__ATINIT__.unshift((function() {
 TTY.init();
}));
__ATEXIT__.push((function() {
 TTY.shutdown();
}));
if (ENVIRONMENT_IS_NODE) {
 var fs = require("fs");
 var NODEJS_PATH = require("path");
 NODEFS.staticInit();
}
Module["requestFullScreen"] = function Module_requestFullScreen(lockPointer, resizeCanvas, vrDevice) {
 Browser.requestFullScreen(lockPointer, resizeCanvas, vrDevice);
};
Module["requestAnimationFrame"] = function Module_requestAnimationFrame(func) {
 Browser.requestAnimationFrame(func);
};
Module["setCanvasSize"] = function Module_setCanvasSize(width, height, noUpdates) {
 Browser.setCanvasSize(width, height, noUpdates);
};
Module["pauseMainLoop"] = function Module_pauseMainLoop() {
 Browser.mainLoop.pause();
};
Module["resumeMainLoop"] = function Module_resumeMainLoop() {
 Browser.mainLoop.resume();
};
Module["getUserMedia"] = function Module_getUserMedia() {
 Browser.getUserMedia();
};
Module["createContext"] = function Module_createContext(canvas, useWebGL, setInModule, webGLContextAttributes) {
 return Browser.createContext(canvas, useWebGL, setInModule, webGLContextAttributes);
};
STACK_BASE = STACKTOP = Runtime.alignMemory(STATICTOP);
staticSealed = true;
STACK_MAX = STACK_BASE + TOTAL_STACK;
DYNAMIC_BASE = DYNAMICTOP = Runtime.alignMemory(STACK_MAX);
assert(DYNAMIC_BASE < TOTAL_MEMORY, "TOTAL_MEMORY not big enough for stack");
var cttz_i8 = allocate([ 8, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0 ], "i8", ALLOC_DYNAMIC);
function invoke_ii(index, a1) {
 try {
  return Module["dynCall_ii"](index, a1);
 } catch (e) {
  if (typeof e !== "number" && e !== "longjmp") throw e;
  asm["setThrew"](1, 0);
 }
}
function invoke_iiii(index, a1, a2, a3) {
 try {
  return Module["dynCall_iiii"](index, a1, a2, a3);
 } catch (e) {
  if (typeof e !== "number" && e !== "longjmp") throw e;
  asm["setThrew"](1, 0);
 }
}
function invoke_vii(index, a1, a2) {
 try {
  Module["dynCall_vii"](index, a1, a2);
 } catch (e) {
  if (typeof e !== "number" && e !== "longjmp") throw e;
  asm["setThrew"](1, 0);
 }
}
function invoke_iii(index, a1, a2) {
 try {
  return Module["dynCall_iii"](index, a1, a2);
 } catch (e) {
  if (typeof e !== "number" && e !== "longjmp") throw e;
  asm["setThrew"](1, 0);
 }
}
function invoke_vi(index, a1) {
 try {
  Module["dynCall_vi"](index, a1);
 } catch (e) {
  if (typeof e !== "number" && e !== "longjmp") throw e;
  asm["setThrew"](1, 0);
 }
}
Module.asmGlobalArg = {
 "Math": Math,
 "Int8Array": Int8Array,
 "Int16Array": Int16Array,
 "Int32Array": Int32Array,
 "Uint8Array": Uint8Array,
 "Uint16Array": Uint16Array,
 "Uint32Array": Uint32Array,
 "Float32Array": Float32Array,
 "Float64Array": Float64Array,
 "NaN": NaN,
 "Infinity": Infinity
};
Module.asmLibraryArg = {
 "abort": abort,
 "assert": assert,
 "invoke_ii": invoke_ii,
 "invoke_iiii": invoke_iiii,
 "invoke_vii": invoke_vii,
 "invoke_iii": invoke_iii,
 "invoke_vi": invoke_vi,
 "_pthread_cleanup_pop": _pthread_cleanup_pop,
 "___lock": ___lock,
 "___assert_fail": ___assert_fail,
 "_pthread_self": _pthread_self,
 "_emscripten_set_main_loop": _emscripten_set_main_loop,
 "___syscall6": ___syscall6,
 "_emscripten_set_main_loop_timing": _emscripten_set_main_loop_timing,
 "_abort": _abort,
 "_sbrk": _sbrk,
 "_time": _time,
 "___setErrNo": ___setErrNo,
 "_emscripten_memcpy_big": _emscripten_memcpy_big,
 "___syscall54": ___syscall54,
 "___unlock": ___unlock,
 "___syscall140": ___syscall140,
 "_pthread_cleanup_push": _pthread_cleanup_push,
 "_sysconf": _sysconf,
 "___syscall146": ___syscall146,
 "STACKTOP": STACKTOP,
 "STACK_MAX": STACK_MAX,
 "tempDoublePtr": tempDoublePtr,
 "ABORT": ABORT,
 "cttz_i8": cttz_i8
};
// EMSCRIPTEN_START_ASM

var asm = (function(global,env,buffer) {

  'use asm';
  
  var HEAP8 = new global.Int8Array(buffer);
  var HEAP16 = new global.Int16Array(buffer);
  var HEAP32 = new global.Int32Array(buffer);
  var HEAPU8 = new global.Uint8Array(buffer);
  var HEAPU16 = new global.Uint16Array(buffer);
  var HEAPU32 = new global.Uint32Array(buffer);
  var HEAPF32 = new global.Float32Array(buffer);
  var HEAPF64 = new global.Float64Array(buffer);


  var STACKTOP=env.STACKTOP|0;
  var STACK_MAX=env.STACK_MAX|0;
  var tempDoublePtr=env.tempDoublePtr|0;
  var ABORT=env.ABORT|0;
  var cttz_i8=env.cttz_i8|0;

  var __THREW__ = 0;
  var threwValue = 0;
  var setjmpId = 0;
  var undef = 0;
  var nan = global.NaN, inf = global.Infinity;
  var tempInt = 0, tempBigInt = 0, tempBigIntP = 0, tempBigIntS = 0, tempBigIntR = 0.0, tempBigIntI = 0, tempBigIntD = 0, tempValue = 0, tempDouble = 0.0;

  var tempRet0 = 0;
  var tempRet1 = 0;
  var tempRet2 = 0;
  var tempRet3 = 0;
  var tempRet4 = 0;
  var tempRet5 = 0;
  var tempRet6 = 0;
  var tempRet7 = 0;
  var tempRet8 = 0;
  var tempRet9 = 0;
  var Math_floor=global.Math.floor;
  var Math_abs=global.Math.abs;
  var Math_sqrt=global.Math.sqrt;
  var Math_pow=global.Math.pow;
  var Math_cos=global.Math.cos;
  var Math_sin=global.Math.sin;
  var Math_tan=global.Math.tan;
  var Math_acos=global.Math.acos;
  var Math_asin=global.Math.asin;
  var Math_atan=global.Math.atan;
  var Math_atan2=global.Math.atan2;
  var Math_exp=global.Math.exp;
  var Math_log=global.Math.log;
  var Math_ceil=global.Math.ceil;
  var Math_imul=global.Math.imul;
  var Math_min=global.Math.min;
  var Math_clz32=global.Math.clz32;
  var abort=env.abort;
  var assert=env.assert;
  var invoke_ii=env.invoke_ii;
  var invoke_iiii=env.invoke_iiii;
  var invoke_vii=env.invoke_vii;
  var invoke_iii=env.invoke_iii;
  var invoke_vi=env.invoke_vi;
  var _pthread_cleanup_pop=env._pthread_cleanup_pop;
  var ___lock=env.___lock;
  var ___assert_fail=env.___assert_fail;
  var _pthread_self=env._pthread_self;
  var _emscripten_set_main_loop=env._emscripten_set_main_loop;
  var ___syscall6=env.___syscall6;
  var _emscripten_set_main_loop_timing=env._emscripten_set_main_loop_timing;
  var _abort=env._abort;
  var _sbrk=env._sbrk;
  var _time=env._time;
  var ___setErrNo=env.___setErrNo;
  var _emscripten_memcpy_big=env._emscripten_memcpy_big;
  var ___syscall54=env.___syscall54;
  var ___unlock=env.___unlock;
  var ___syscall140=env.___syscall140;
  var _pthread_cleanup_push=env._pthread_cleanup_push;
  var _sysconf=env._sysconf;
  var ___syscall146=env.___syscall146;
  var tempFloat = 0.0;

// EMSCRIPTEN_START_FUNCS

function _inflate(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0, i24 = 0, i25 = 0, i26 = 0, i27 = 0, i28 = 0, i29 = 0, i30 = 0, i31 = 0, i32 = 0, i33 = 0, i34 = 0, i35 = 0, i36 = 0, i37 = 0, i38 = 0, i39 = 0, i40 = 0, i41 = 0, i42 = 0, i43 = 0, i44 = 0, i45 = 0, i46 = 0, i47 = 0, i48 = 0, i49 = 0, i50 = 0, i51 = 0, i52 = 0, i53 = 0, i54 = 0, i55 = 0, i56 = 0, i57 = 0, i58 = 0, i59 = 0, i60 = 0, i61 = 0, i62 = 0, i63 = 0, i64 = 0, i65 = 0, i66 = 0, i67 = 0, i68 = 0, i69 = 0, i70 = 0, i71 = 0, i72 = 0, i73 = 0, i74 = 0, i75 = 0, i76 = 0, i77 = 0, i78 = 0, i79 = 0, i80 = 0, i81 = 0, i82 = 0, i83 = 0, i84 = 0, i85 = 0, i86 = 0, i87 = 0, i88 = 0, i89 = 0, i90 = 0, i91 = 0, i92 = 0, i93 = 0, i94 = 0, i95 = 0, i96 = 0, i97 = 0, i98 = 0, i99 = 0, i100 = 0, i101 = 0, i102 = 0, i103 = 0, i104 = 0, i105 = 0, i106 = 0, i107 = 0, i108 = 0, i109 = 0, i110 = 0, i111 = 0, i112 = 0, i113 = 0, i114 = 0, i115 = 0, i116 = 0, i117 = 0, i118 = 0, i119 = 0, i120 = 0, i121 = 0, i122 = 0, i123 = 0, i124 = 0, i125 = 0, i126 = 0, i127 = 0, i128 = 0, i129 = 0, i130 = 0, i131 = 0, i132 = 0, i133 = 0, i134 = 0, i135 = 0, i136 = 0, i137 = 0, i138 = 0, i139 = 0, i140 = 0, i141 = 0, i142 = 0, i143 = 0, i144 = 0, i145 = 0, i146 = 0, i147 = 0, i148 = 0, i149 = 0, i150 = 0, i151 = 0, i152 = 0, i153 = 0, i154 = 0, i155 = 0, i156 = 0, i157 = 0, i158 = 0, i159 = 0, i160 = 0, i161 = 0, i162 = 0, i163 = 0, i164 = 0, i165 = 0, i166 = 0, i167 = 0, i168 = 0, i169 = 0, i170 = 0, i171 = 0, i172 = 0, i173 = 0, i174 = 0, i175 = 0, i176 = 0, i177 = 0, i178 = 0, i179 = 0, i180 = 0, i181 = 0, i182 = 0, i183 = 0, i184 = 0, i185 = 0, i186 = 0, i187 = 0, i188 = 0, i189 = 0, i190 = 0, i191 = 0, i192 = 0, i193 = 0, i194 = 0, i195 = 0, i196 = 0, i197 = 0, i198 = 0, i199 = 0, i200 = 0, i201 = 0, i202 = 0, i203 = 0, i204 = 0, i205 = 0, i206 = 0, i207 = 0, i208 = 0, i209 = 0, i210 = 0, i211 = 0, i212 = 0, i213 = 0, i214 = 0, i215 = 0, i216 = 0, i217 = 0, i218 = 0, i219 = 0, i220 = 0, i221 = 0, i222 = 0, i223 = 0, i224 = 0, i225 = 0, i226 = 0, i227 = 0, i228 = 0, i229 = 0, i230 = 0, i231 = 0, i232 = 0, i233 = 0, i234 = 0, i235 = 0, i236 = 0, i237 = 0, i238 = 0, i239 = 0, i240 = 0, i241 = 0, i242 = 0, i243 = 0, i244 = 0, i245 = 0, i246 = 0, i247 = 0, i248 = 0, i249 = 0, i250 = 0, i251 = 0, i252 = 0, i253 = 0, i254 = 0, i255 = 0, i256 = 0, i257 = 0, i258 = 0, i259 = 0, i260 = 0, i261 = 0, i262 = 0, i263 = 0, i264 = 0, i265 = 0, i266 = 0, i267 = 0, i268 = 0, i269 = 0, i270 = 0, i271 = 0, i272 = 0, i273 = 0, i274 = 0, i275 = 0, i276 = 0, i277 = 0, i278 = 0, i279 = 0, i280 = 0, i281 = 0, i282 = 0, i283 = 0, i284 = 0, i285 = 0, i286 = 0, i287 = 0, i288 = 0, i289 = 0, i290 = 0, i291 = 0, i292 = 0, i293 = 0, i294 = 0, i295 = 0, i296 = 0, i297 = 0, i298 = 0, i299 = 0, i300 = 0, i301 = 0, i302 = 0, i303 = 0, i304 = 0, i305 = 0, i306 = 0, i307 = 0, i308 = 0, i309 = 0, i310 = 0, i311 = 0, i312 = 0, i313 = 0, i314 = 0, i315 = 0, i316 = 0, i317 = 0, i318 = 0, i319 = 0, i320 = 0, i321 = 0, i322 = 0, i323 = 0, i324 = 0, i325 = 0, i326 = 0, i327 = 0, i328 = 0, i329 = 0, i330 = 0, i331 = 0, i332 = 0, i333 = 0, i334 = 0, i335 = 0, i336 = 0, i337 = 0, i338 = 0, i339 = 0, i340 = 0, i341 = 0, i342 = 0, i343 = 0, i344 = 0, i345 = 0, i346 = 0, i347 = 0, i348 = 0, i349 = 0, i350 = 0, i351 = 0, i352 = 0, i353 = 0, i354 = 0, i355 = 0, i356 = 0, i357 = 0, i358 = 0, i359 = 0, i360 = 0, i361 = 0, i362 = 0, i363 = 0, i364 = 0, i365 = 0, i366 = 0, i367 = 0, i368 = 0, i369 = 0, i370 = 0, i371 = 0, i372 = 0, i373 = 0, i374 = 0, i375 = 0, i376 = 0, i377 = 0, i378 = 0, i379 = 0, i380 = 0;
 i3 = STACKTOP;
 STACKTOP = STACKTOP + 16 | 0;
 if (!i1) {
  i4 = -2;
  STACKTOP = i3;
  return i4 | 0;
 }
 i5 = HEAP32[i1 + 28 >> 2] | 0;
 if (!i5) {
  i4 = -2;
  STACKTOP = i3;
  return i4 | 0;
 }
 i6 = HEAP32[i1 + 12 >> 2] | 0;
 if (!i6) {
  i4 = -2;
  STACKTOP = i3;
  return i4 | 0;
 }
 i7 = HEAP32[i1 >> 2] | 0;
 if ((i7 | 0) == 0 ? (HEAP32[i1 + 4 >> 2] | 0) != 0 : 0) {
  i4 = -2;
  STACKTOP = i3;
  return i4 | 0;
 }
 i8 = HEAP32[i5 >> 2] | 0;
 if ((i8 | 0) == 11) {
  HEAP32[i5 >> 2] = 12;
  i9 = 12;
 } else i9 = i8;
 i8 = HEAP32[i1 + 16 >> 2] | 0;
 i10 = HEAP32[i1 + 4 >> 2] | 0;
 i11 = i9;
 i9 = HEAP32[i5 + 60 >> 2] | 0;
 i12 = i10;
 i13 = HEAP32[i5 + 56 >> 2] | 0;
 i14 = i8;
 i15 = i7;
 i7 = i8;
 i8 = i6;
 i6 = 0;
 L17 : while (1) {
  L19 : do switch (i11 | 0) {
  case 28:
   {
    i16 = i14;
    i17 = i9;
    i18 = i12;
    i19 = i13;
    i20 = i15;
    i21 = i7;
    i22 = i8;
    i23 = 1;
    break L17;
    break;
   }
  case 29:
   {
    i24 = i9;
    i25 = i12;
    i26 = i13;
    i27 = i14;
    i28 = i15;
    i29 = i7;
    i30 = i8;
    i31 = 284;
    break L17;
    break;
   }
  case 30:
   {
    i4 = -4;
    i31 = 297;
    break L17;
    break;
   }
  case 0:
   {
    i32 = HEAP32[i5 + 8 >> 2] | 0;
    if (!i32) {
     HEAP32[i5 >> 2] = 12;
     i33 = i9;
     i34 = i12;
     i35 = i13;
     i36 = i14;
     i37 = i15;
     i38 = i7;
     i39 = i8;
     i40 = i6;
     break L19;
    }
    if (i9 >>> 0 < 16) {
     i41 = i9;
     i42 = i12;
     i43 = i13;
     i44 = i15;
     while (1) {
      if (!i42) {
       i16 = i14;
       i17 = i41;
       i18 = 0;
       i19 = i43;
       i20 = i44;
       i21 = i7;
       i22 = i8;
       i23 = i6;
       break L17;
      }
      i45 = i42 + -1 | 0;
      i46 = i44 + 1 | 0;
      i47 = (HEAPU8[i44 >> 0] << i41) + i43 | 0;
      i48 = i41 + 8 | 0;
      if (i48 >>> 0 < 16) {
       i41 = i48;
       i42 = i45;
       i43 = i47;
       i44 = i46;
      } else {
       i49 = i48;
       i50 = i45;
       i51 = i47;
       i52 = i46;
       break;
      }
     }
    } else {
     i49 = i9;
     i50 = i12;
     i51 = i13;
     i52 = i15;
    }
    if ((i51 | 0) == 35615 & (i32 & 2 | 0) != 0) {
     HEAP32[i5 + 24 >> 2] = _crc32(0, 0, 0) | 0;
     HEAP8[i3 >> 0] = 31;
     HEAP8[i3 + 1 >> 0] = -117;
     HEAP32[i5 + 24 >> 2] = _crc32(HEAP32[i5 + 24 >> 2] | 0, i3, 2) | 0;
     HEAP32[i5 >> 2] = 1;
     i33 = 0;
     i34 = i50;
     i35 = 0;
     i36 = i14;
     i37 = i52;
     i38 = i7;
     i39 = i8;
     i40 = i6;
     break L19;
    }
    HEAP32[i5 + 16 >> 2] = 0;
    i44 = HEAP32[i5 + 32 >> 2] | 0;
    if (i44) HEAP32[i44 + 48 >> 2] = -1;
    if ((i32 & 1 | 0) != 0 ? ((((i51 << 8 & 65280) + (i51 >>> 8) | 0) >>> 0) % 31 | 0 | 0) == 0 : 0) {
     if ((i51 & 15 | 0) != 8) {
      HEAP32[i1 + 24 >> 2] = 14395;
      HEAP32[i5 >> 2] = 29;
      i33 = i49;
      i34 = i50;
      i35 = i51;
      i36 = i14;
      i37 = i52;
      i38 = i7;
      i39 = i8;
      i40 = i6;
      break L19;
     }
     i44 = i51 >>> 4;
     i43 = i49 + -4 | 0;
     i42 = HEAP32[i5 + 36 >> 2] | 0;
     if (i42) {
      if (((i44 & 15) + 8 | 0) >>> 0 > i42 >>> 0) {
       HEAP32[i1 + 24 >> 2] = 14422;
       HEAP32[i5 >> 2] = 29;
       i33 = i43;
       i34 = i50;
       i35 = i44;
       i36 = i14;
       i37 = i52;
       i38 = i7;
       i39 = i8;
       i40 = i6;
       break L19;
      }
     } else HEAP32[i5 + 36 >> 2] = (i44 & 15) + 8;
     HEAP32[i5 + 20 >> 2] = 1 << (i44 & 15) + 8;
     i44 = _adler32(0, 0, 0) | 0;
     HEAP32[i5 + 24 >> 2] = i44;
     HEAP32[i1 + 48 >> 2] = i44;
     HEAP32[i5 >> 2] = i51 >>> 12 & 2 ^ 11;
     i33 = 0;
     i34 = i50;
     i35 = 0;
     i36 = i14;
     i37 = i52;
     i38 = i7;
     i39 = i8;
     i40 = i6;
     break L19;
    }
    HEAP32[i1 + 24 >> 2] = 14372;
    HEAP32[i5 >> 2] = 29;
    i33 = i49;
    i34 = i50;
    i35 = i51;
    i36 = i14;
    i37 = i52;
    i38 = i7;
    i39 = i8;
    i40 = i6;
    break;
   }
  case 1:
   {
    if (i9 >>> 0 < 16) {
     i44 = i9;
     i43 = i12;
     i42 = i13;
     i41 = i15;
     while (1) {
      if (!i43) {
       i16 = i14;
       i17 = i44;
       i18 = 0;
       i19 = i42;
       i20 = i41;
       i21 = i7;
       i22 = i8;
       i23 = i6;
       break L17;
      }
      i46 = i43 + -1 | 0;
      i47 = i41 + 1 | 0;
      i45 = (HEAPU8[i41 >> 0] << i44) + i42 | 0;
      i48 = i44 + 8 | 0;
      if (i48 >>> 0 < 16) {
       i44 = i48;
       i43 = i46;
       i42 = i45;
       i41 = i47;
      } else {
       i53 = i45;
       i54 = i48;
       i55 = i46;
       i56 = i47;
       break;
      }
     }
    } else {
     i53 = i13;
     i54 = i9;
     i55 = i12;
     i56 = i15;
    }
    HEAP32[i5 + 16 >> 2] = i53;
    if ((i53 & 255 | 0) != 8) {
     HEAP32[i1 + 24 >> 2] = 14395;
     HEAP32[i5 >> 2] = 29;
     i33 = i54;
     i34 = i55;
     i35 = i53;
     i36 = i14;
     i37 = i56;
     i38 = i7;
     i39 = i8;
     i40 = i6;
     break L19;
    }
    if (i53 & 57344) {
     HEAP32[i1 + 24 >> 2] = 14442;
     HEAP32[i5 >> 2] = 29;
     i33 = i54;
     i34 = i55;
     i35 = i53;
     i36 = i14;
     i37 = i56;
     i38 = i7;
     i39 = i8;
     i40 = i6;
     break L19;
    }
    i41 = HEAP32[i5 + 32 >> 2] | 0;
    if (i41) HEAP32[i41 >> 2] = i53 >>> 8 & 1;
    if (i53 & 512) {
     HEAP8[i3 >> 0] = i53;
     HEAP8[i3 + 1 >> 0] = i53 >>> 8;
     HEAP32[i5 + 24 >> 2] = _crc32(HEAP32[i5 + 24 >> 2] | 0, i3, 2) | 0;
    }
    HEAP32[i5 >> 2] = 2;
    i57 = 0;
    i58 = i55;
    i59 = 0;
    i60 = i56;
    i31 = 47;
    break;
   }
  case 2:
   {
    if (i9 >>> 0 < 32) {
     i57 = i9;
     i58 = i12;
     i59 = i13;
     i60 = i15;
     i31 = 47;
    } else {
     i61 = i12;
     i62 = i13;
     i63 = i15;
     i31 = 49;
    }
    break;
   }
  case 3:
   {
    if (i9 >>> 0 < 16) {
     i64 = i9;
     i65 = i12;
     i66 = i13;
     i67 = i15;
     i31 = 55;
    } else {
     i68 = i12;
     i69 = i13;
     i70 = i15;
     i31 = 57;
    }
    break;
   }
  case 4:
   {
    i71 = i9;
    i72 = i12;
    i73 = i13;
    i74 = i15;
    i31 = 62;
    break;
   }
  case 5:
   {
    i75 = i9;
    i76 = i12;
    i77 = i13;
    i78 = i15;
    i31 = 73;
    break;
   }
  case 6:
   {
    i79 = i9;
    i80 = i12;
    i81 = i13;
    i82 = i15;
    i31 = 83;
    break;
   }
  case 7:
   {
    i83 = i9;
    i84 = i12;
    i85 = i13;
    i86 = i15;
    i31 = 96;
    break;
   }
  case 8:
   {
    i87 = i9;
    i88 = i12;
    i89 = i13;
    i90 = i15;
    i31 = 109;
    break;
   }
  case 9:
   {
    if (i9 >>> 0 < 32) {
     i41 = i9;
     i42 = i12;
     i43 = i13;
     i44 = i15;
     while (1) {
      if (!i42) {
       i16 = i14;
       i17 = i41;
       i18 = 0;
       i19 = i43;
       i20 = i44;
       i21 = i7;
       i22 = i8;
       i23 = i6;
       break L17;
      }
      i32 = i42 + -1 | 0;
      i47 = i44 + 1 | 0;
      i46 = (HEAPU8[i44 >> 0] << i41) + i43 | 0;
      i41 = i41 + 8 | 0;
      if (i41 >>> 0 >= 32) {
       i91 = i32;
       i92 = i46;
       i93 = i47;
       break;
      } else {
       i42 = i32;
       i43 = i46;
       i44 = i47;
      }
     }
    } else {
     i91 = i12;
     i92 = i13;
     i93 = i15;
    }
    i44 = _llvm_bswap_i32(i92 | 0) | 0;
    HEAP32[i5 + 24 >> 2] = i44;
    HEAP32[i1 + 48 >> 2] = i44;
    HEAP32[i5 >> 2] = 10;
    i94 = 0;
    i95 = i91;
    i96 = 0;
    i97 = i93;
    i31 = 121;
    break;
   }
  case 10:
   {
    i94 = i9;
    i95 = i12;
    i96 = i13;
    i97 = i15;
    i31 = 121;
    break;
   }
  case 11:
   {
    i98 = i9;
    i99 = i12;
    i100 = i13;
    i101 = i15;
    i31 = 124;
    break;
   }
  case 12:
   {
    i102 = i9;
    i103 = i12;
    i104 = i13;
    i105 = i15;
    i31 = 125;
    break;
   }
  case 13:
   {
    i44 = i9 & 7;
    i43 = i13 >>> i44;
    i42 = i9 - i44 | 0;
    if (i42 >>> 0 < 32) {
     i44 = i42;
     i41 = i12;
     i47 = i43;
     i46 = i15;
     while (1) {
      if (!i41) {
       i16 = i14;
       i17 = i44;
       i18 = 0;
       i19 = i47;
       i20 = i46;
       i21 = i7;
       i22 = i8;
       i23 = i6;
       break L17;
      }
      i32 = i41 + -1 | 0;
      i48 = i46 + 1 | 0;
      i45 = (HEAPU8[i46 >> 0] << i44) + i47 | 0;
      i106 = i44 + 8 | 0;
      if (i106 >>> 0 < 32) {
       i44 = i106;
       i41 = i32;
       i47 = i45;
       i46 = i48;
      } else {
       i107 = i106;
       i108 = i32;
       i109 = i45;
       i110 = i48;
       break;
      }
     }
    } else {
     i107 = i42;
     i108 = i12;
     i109 = i43;
     i110 = i15;
    }
    i46 = i109 & 65535;
    if ((i46 | 0) == (i109 >>> 16 ^ 65535 | 0)) {
     HEAP32[i5 + 64 >> 2] = i46;
     HEAP32[i5 >> 2] = 14;
     if ((i2 | 0) == 6) {
      i16 = i14;
      i17 = 0;
      i18 = i108;
      i19 = 0;
      i20 = i110;
      i21 = i7;
      i22 = i8;
      i23 = i6;
      break L17;
     } else {
      i111 = 0;
      i112 = i108;
      i113 = 0;
      i114 = i110;
      i31 = 143;
      break L19;
     }
    } else {
     HEAP32[i1 + 24 >> 2] = 14506;
     HEAP32[i5 >> 2] = 29;
     i33 = i107;
     i34 = i108;
     i35 = i109;
     i36 = i14;
     i37 = i110;
     i38 = i7;
     i39 = i8;
     i40 = i6;
     break L19;
    }
    break;
   }
  case 14:
   {
    i111 = i9;
    i112 = i12;
    i113 = i13;
    i114 = i15;
    i31 = 143;
    break;
   }
  case 15:
   {
    i115 = i9;
    i116 = i12;
    i117 = i13;
    i118 = i15;
    i31 = 144;
    break;
   }
  case 16:
   {
    if (i9 >>> 0 < 14) {
     i46 = i9;
     i47 = i12;
     i41 = i13;
     i44 = i15;
     while (1) {
      if (!i47) {
       i16 = i14;
       i17 = i46;
       i18 = 0;
       i19 = i41;
       i20 = i44;
       i21 = i7;
       i22 = i8;
       i23 = i6;
       break L17;
      }
      i48 = i47 + -1 | 0;
      i45 = i44 + 1 | 0;
      i32 = (HEAPU8[i44 >> 0] << i46) + i41 | 0;
      i106 = i46 + 8 | 0;
      if (i106 >>> 0 < 14) {
       i46 = i106;
       i47 = i48;
       i41 = i32;
       i44 = i45;
      } else {
       i119 = i106;
       i120 = i48;
       i121 = i32;
       i122 = i45;
       break;
      }
     }
    } else {
     i119 = i9;
     i120 = i12;
     i121 = i13;
     i122 = i15;
    }
    i44 = (i121 & 31) + 257 | 0;
    HEAP32[i5 + 96 >> 2] = i44;
    i41 = (i121 >>> 5 & 31) + 1 | 0;
    HEAP32[i5 + 100 >> 2] = i41;
    i47 = (i121 >>> 10 & 15) + 4 | 0;
    HEAP32[i5 + 92 >> 2] = i47;
    i46 = i121 >>> 14;
    i43 = i119 + -14 | 0;
    if (i44 >>> 0 > 286 | i41 >>> 0 > 30) {
     HEAP32[i1 + 24 >> 2] = 14535;
     HEAP32[i5 >> 2] = 29;
     i33 = i43;
     i34 = i120;
     i35 = i46;
     i36 = i14;
     i37 = i122;
     i38 = i7;
     i39 = i8;
     i40 = i6;
     break L19;
    } else {
     HEAP32[i5 + 104 >> 2] = 0;
     HEAP32[i5 >> 2] = 17;
     i123 = i47;
     i124 = 0;
     i125 = i43;
     i126 = i120;
     i127 = i46;
     i128 = i122;
     i31 = 154;
     break L19;
    }
    break;
   }
  case 17:
   {
    i46 = HEAP32[i5 + 104 >> 2] | 0;
    i43 = HEAP32[i5 + 92 >> 2] | 0;
    if (i46 >>> 0 < i43 >>> 0) {
     i123 = i43;
     i124 = i46;
     i125 = i9;
     i126 = i12;
     i127 = i13;
     i128 = i15;
     i31 = 154;
    } else {
     i129 = i46;
     i130 = i9;
     i131 = i12;
     i132 = i13;
     i133 = i15;
     i31 = 155;
    }
    break;
   }
  case 18:
   {
    i134 = HEAP32[i5 + 104 >> 2] | 0;
    i135 = i9;
    i136 = i12;
    i137 = i13;
    i138 = i15;
    i139 = i6;
    i31 = 165;
    break;
   }
  case 19:
   {
    i140 = i9;
    i141 = i12;
    i142 = i13;
    i143 = i15;
    i144 = i6;
    i31 = 202;
    break;
   }
  case 20:
   {
    i145 = i9;
    i146 = i12;
    i147 = i13;
    i148 = i15;
    i149 = i6;
    i31 = 203;
    break;
   }
  case 21:
   {
    i150 = HEAP32[i5 + 72 >> 2] | 0;
    i151 = i9;
    i152 = i12;
    i153 = i13;
    i154 = i15;
    i155 = i6;
    i31 = 221;
    break;
   }
  case 22:
   {
    i156 = i9;
    i157 = i12;
    i158 = i13;
    i159 = i15;
    i160 = i6;
    i31 = 228;
    break;
   }
  case 23:
   {
    i161 = HEAP32[i5 + 72 >> 2] | 0;
    i162 = i9;
    i163 = i12;
    i164 = i13;
    i165 = i15;
    i166 = i6;
    i31 = 240;
    break;
   }
  case 24:
   {
    i167 = i9;
    i168 = i12;
    i169 = i13;
    i170 = i15;
    i171 = i6;
    i31 = 246;
    break;
   }
  case 25:
   {
    if (!i14) {
     i16 = 0;
     i17 = i9;
     i18 = i12;
     i19 = i13;
     i20 = i15;
     i21 = i7;
     i22 = i8;
     i23 = i6;
     break L17;
    }
    HEAP8[i8 >> 0] = HEAP32[i5 + 64 >> 2];
    HEAP32[i5 >> 2] = 20;
    i33 = i9;
    i34 = i12;
    i35 = i13;
    i36 = i14 + -1 | 0;
    i37 = i15;
    i38 = i7;
    i39 = i8 + 1 | 0;
    i40 = i6;
    break;
   }
  case 26:
   {
    if (HEAP32[i5 + 8 >> 2] | 0) {
     if (i9 >>> 0 < 32) {
      i46 = i9;
      i43 = i12;
      i47 = i13;
      i41 = i15;
      while (1) {
       if (!i43) {
        i16 = i14;
        i17 = i46;
        i18 = 0;
        i19 = i47;
        i20 = i41;
        i21 = i7;
        i22 = i8;
        i23 = i6;
        break L17;
       }
       i44 = i43 + -1 | 0;
       i42 = i41 + 1 | 0;
       i45 = (HEAPU8[i41 >> 0] << i46) + i47 | 0;
       i32 = i46 + 8 | 0;
       if (i32 >>> 0 < 32) {
        i46 = i32;
        i43 = i44;
        i47 = i45;
        i41 = i42;
       } else {
        i172 = i32;
        i173 = i44;
        i174 = i45;
        i175 = i42;
        break;
       }
      }
     } else {
      i172 = i9;
      i173 = i12;
      i174 = i13;
      i175 = i15;
     }
     i41 = i7 - i14 | 0;
     HEAP32[i1 + 20 >> 2] = (HEAP32[i1 + 20 >> 2] | 0) + i41;
     HEAP32[i5 + 28 >> 2] = (HEAP32[i5 + 28 >> 2] | 0) + i41;
     if ((i7 | 0) == (i14 | 0)) i176 = HEAP32[i5 + 24 >> 2] | 0; else {
      i47 = HEAP32[i5 + 24 >> 2] | 0;
      i43 = i8 + (0 - i41) | 0;
      if (!(HEAP32[i5 + 16 >> 2] | 0)) i177 = _adler32(i47, i43, i41) | 0; else i177 = _crc32(i47, i43, i41) | 0;
      HEAP32[i5 + 24 >> 2] = i177;
      HEAP32[i1 + 48 >> 2] = i177;
      i176 = i177;
     }
     i41 = (HEAP32[i5 + 16 >> 2] | 0) == 0;
     i43 = _llvm_bswap_i32(i174 | 0) | 0;
     if (((i41 ? i43 : i174) | 0) == (i176 | 0)) {
      i178 = 0;
      i179 = i173;
      i180 = 0;
      i181 = i175;
      i182 = i14;
     } else {
      HEAP32[i1 + 24 >> 2] = 14709;
      HEAP32[i5 >> 2] = 29;
      i33 = i172;
      i34 = i173;
      i35 = i174;
      i36 = i14;
      i37 = i175;
      i38 = i14;
      i39 = i8;
      i40 = i6;
      break L19;
     }
    } else {
     i178 = i9;
     i179 = i12;
     i180 = i13;
     i181 = i15;
     i182 = i7;
    }
    HEAP32[i5 >> 2] = 27;
    i183 = i178;
    i184 = i179;
    i185 = i180;
    i186 = i181;
    i187 = i182;
    i31 = 276;
    break;
   }
  case 27:
   {
    i183 = i9;
    i184 = i12;
    i185 = i13;
    i186 = i15;
    i187 = i7;
    i31 = 276;
    break;
   }
  default:
   {
    i31 = 296;
    break L17;
   }
  } while (0);
  if ((i31 | 0) == 47) while (1) {
   i31 = 0;
   if (!i58) {
    i16 = i14;
    i17 = i57;
    i18 = 0;
    i19 = i59;
    i20 = i60;
    i21 = i7;
    i22 = i8;
    i23 = i6;
    break L17;
   }
   i43 = i58 + -1 | 0;
   i41 = i60 + 1 | 0;
   i47 = (HEAPU8[i60 >> 0] << i57) + i59 | 0;
   i57 = i57 + 8 | 0;
   if (i57 >>> 0 >= 32) {
    i61 = i43;
    i62 = i47;
    i63 = i41;
    i31 = 49;
    break;
   } else {
    i58 = i43;
    i59 = i47;
    i60 = i41;
    i31 = 47;
   }
  } else if ((i31 | 0) == 121) {
   i31 = 0;
   if (!(HEAP32[i5 + 12 >> 2] | 0)) {
    i188 = i94;
    i189 = i95;
    i190 = i96;
    i191 = i14;
    i192 = i97;
    i193 = i8;
    i31 = 122;
    break;
   }
   i41 = _adler32(0, 0, 0) | 0;
   HEAP32[i5 + 24 >> 2] = i41;
   HEAP32[i1 + 48 >> 2] = i41;
   HEAP32[i5 >> 2] = 11;
   i98 = i94;
   i99 = i95;
   i100 = i96;
   i101 = i97;
   i31 = 124;
  } else if ((i31 | 0) == 143) {
   i31 = 0;
   HEAP32[i5 >> 2] = 15;
   i115 = i111;
   i116 = i112;
   i117 = i113;
   i118 = i114;
   i31 = 144;
  } else if ((i31 | 0) == 154) {
   i31 = 0;
   i41 = i124;
   i47 = i125;
   i43 = i126;
   i46 = i127;
   i42 = i128;
   while (1) {
    if (i47 >>> 0 < 3) {
     i45 = i47;
     i44 = i43;
     i32 = i46;
     i48 = i42;
     while (1) {
      if (!i44) {
       i16 = i14;
       i17 = i45;
       i18 = 0;
       i19 = i32;
       i20 = i48;
       i21 = i7;
       i22 = i8;
       i23 = i6;
       break L17;
      }
      i106 = i44 + -1 | 0;
      i194 = i48 + 1 | 0;
      i195 = (HEAPU8[i48 >> 0] << i45) + i32 | 0;
      i196 = i45 + 8 | 0;
      if (i196 >>> 0 < 3) {
       i45 = i196;
       i44 = i106;
       i32 = i195;
       i48 = i194;
      } else {
       i197 = i196;
       i198 = i106;
       i199 = i195;
       i200 = i194;
       break;
      }
     }
    } else {
     i197 = i47;
     i198 = i43;
     i199 = i46;
     i200 = i42;
    }
    i48 = i41 + 1 | 0;
    HEAP32[i5 + 104 >> 2] = i48;
    HEAP16[i5 + 112 + (HEAPU16[13088 + (i41 << 1) >> 1] << 1) >> 1] = i199 & 7;
    i32 = i199 >>> 3;
    i44 = i197 + -3 | 0;
    if (i48 >>> 0 < i123 >>> 0) {
     i41 = i48;
     i47 = i44;
     i43 = i198;
     i46 = i32;
     i42 = i200;
    } else {
     i129 = i48;
     i130 = i44;
     i131 = i198;
     i132 = i32;
     i133 = i200;
     i31 = 155;
     break;
    }
   }
  } else if ((i31 | 0) == 276) {
   i31 = 0;
   if (!(HEAP32[i5 + 8 >> 2] | 0)) {
    i201 = i183;
    i202 = i184;
    i203 = i185;
    i204 = i14;
    i205 = i186;
    i206 = i187;
    i207 = i8;
    i31 = 283;
    break;
   }
   if (!(HEAP32[i5 + 16 >> 2] | 0)) {
    i201 = i183;
    i202 = i184;
    i203 = i185;
    i204 = i14;
    i205 = i186;
    i206 = i187;
    i207 = i8;
    i31 = 283;
    break;
   }
   if (i183 >>> 0 < 32) {
    i42 = i183;
    i46 = i184;
    i43 = i185;
    i47 = i186;
    while (1) {
     if (!i46) {
      i16 = i14;
      i17 = i42;
      i18 = 0;
      i19 = i43;
      i20 = i47;
      i21 = i187;
      i22 = i8;
      i23 = i6;
      break L17;
     }
     i41 = i46 + -1 | 0;
     i32 = i47 + 1 | 0;
     i44 = (HEAPU8[i47 >> 0] << i42) + i43 | 0;
     i48 = i42 + 8 | 0;
     if (i48 >>> 0 < 32) {
      i42 = i48;
      i46 = i41;
      i43 = i44;
      i47 = i32;
     } else {
      i208 = i48;
      i209 = i41;
      i210 = i44;
      i211 = i32;
      break;
     }
    }
   } else {
    i208 = i183;
    i209 = i184;
    i210 = i185;
    i211 = i186;
   }
   if ((i210 | 0) == (HEAP32[i5 + 28 >> 2] | 0)) {
    i201 = 0;
    i202 = i209;
    i203 = 0;
    i204 = i14;
    i205 = i211;
    i206 = i187;
    i207 = i8;
    i31 = 283;
    break;
   }
   HEAP32[i1 + 24 >> 2] = 14730;
   HEAP32[i5 >> 2] = 29;
   i33 = i208;
   i34 = i209;
   i35 = i210;
   i36 = i14;
   i37 = i211;
   i38 = i187;
   i39 = i8;
   i40 = i6;
  }
  do if ((i31 | 0) == 49) {
   i31 = 0;
   i47 = HEAP32[i5 + 32 >> 2] | 0;
   if (i47) HEAP32[i47 + 4 >> 2] = i62;
   if (HEAP32[i5 + 16 >> 2] & 512) {
    HEAP8[i3 >> 0] = i62;
    HEAP8[i3 + 1 >> 0] = i62 >>> 8;
    HEAP8[i3 + 2 >> 0] = i62 >>> 16;
    HEAP8[i3 + 3 >> 0] = i62 >>> 24;
    HEAP32[i5 + 24 >> 2] = _crc32(HEAP32[i5 + 24 >> 2] | 0, i3, 4) | 0;
   }
   HEAP32[i5 >> 2] = 3;
   i64 = 0;
   i65 = i61;
   i66 = 0;
   i67 = i63;
   i31 = 55;
  } else if ((i31 | 0) == 124) {
   i31 = 0;
   if ((i2 + -5 | 0) >>> 0 < 2) {
    i16 = i14;
    i17 = i98;
    i18 = i99;
    i19 = i100;
    i20 = i101;
    i21 = i7;
    i22 = i8;
    i23 = i6;
    break L17;
   } else {
    i102 = i98;
    i103 = i99;
    i104 = i100;
    i105 = i101;
    i31 = 125;
   }
  } else if ((i31 | 0) == 144) {
   i31 = 0;
   i47 = HEAP32[i5 + 64 >> 2] | 0;
   if (!i47) {
    HEAP32[i5 >> 2] = 11;
    i33 = i115;
    i34 = i116;
    i35 = i117;
    i36 = i14;
    i37 = i118;
    i38 = i7;
    i39 = i8;
    i40 = i6;
    break;
   }
   i43 = i47 >>> 0 > i116 >>> 0 ? i116 : i47;
   i47 = i43 >>> 0 > i14 >>> 0 ? i14 : i43;
   if (!i47) {
    i16 = i14;
    i17 = i115;
    i18 = i116;
    i19 = i117;
    i20 = i118;
    i21 = i7;
    i22 = i8;
    i23 = i6;
    break L17;
   }
   _memcpy(i8 | 0, i118 | 0, i47 | 0) | 0;
   HEAP32[i5 + 64 >> 2] = (HEAP32[i5 + 64 >> 2] | 0) - i47;
   i33 = i115;
   i34 = i116 - i47 | 0;
   i35 = i117;
   i36 = i14 - i47 | 0;
   i37 = i118 + i47 | 0;
   i38 = i7;
   i39 = i8 + i47 | 0;
   i40 = i6;
  } else if ((i31 | 0) == 155) {
   i31 = 0;
   if (i129 >>> 0 < 19) {
    i47 = i129;
    do {
     HEAP16[i5 + 112 + (HEAPU16[13088 + (i47 << 1) >> 1] << 1) >> 1] = 0;
     i47 = i47 + 1 | 0;
    } while ((i47 | 0) != 19);
    HEAP32[i5 + 104 >> 2] = 19;
   }
   HEAP32[i5 + 108 >> 2] = i5 + 1328;
   HEAP32[i5 + 76 >> 2] = i5 + 1328;
   HEAP32[i5 + 84 >> 2] = 7;
   i47 = _inflate_table(0, i5 + 112 | 0, 19, i5 + 108 | 0, i5 + 84 | 0, i5 + 752 | 0) | 0;
   if (!i47) {
    HEAP32[i5 + 104 >> 2] = 0;
    HEAP32[i5 >> 2] = 18;
    i134 = 0;
    i135 = i130;
    i136 = i131;
    i137 = i132;
    i138 = i133;
    i139 = 0;
    i31 = 165;
    break;
   } else {
    HEAP32[i1 + 24 >> 2] = 14571;
    HEAP32[i5 >> 2] = 29;
    i33 = i130;
    i34 = i131;
    i35 = i132;
    i36 = i14;
    i37 = i133;
    i38 = i7;
    i39 = i8;
    i40 = i47;
    break;
   }
  } while (0);
  L162 : do if ((i31 | 0) == 55) while (1) {
   i31 = 0;
   if (!i65) {
    i16 = i14;
    i17 = i64;
    i18 = 0;
    i19 = i66;
    i20 = i67;
    i21 = i7;
    i22 = i8;
    i23 = i6;
    break L17;
   }
   i47 = i65 + -1 | 0;
   i43 = i67 + 1 | 0;
   i46 = (HEAPU8[i67 >> 0] << i64) + i66 | 0;
   i64 = i64 + 8 | 0;
   if (i64 >>> 0 >= 16) {
    i68 = i47;
    i69 = i46;
    i70 = i43;
    i31 = 57;
    break;
   } else {
    i65 = i47;
    i66 = i46;
    i67 = i43;
    i31 = 55;
   }
  } else if ((i31 | 0) == 125) {
   i31 = 0;
   if (HEAP32[i5 + 4 >> 2] | 0) {
    i43 = i102 & 7;
    HEAP32[i5 >> 2] = 26;
    i33 = i102 - i43 | 0;
    i34 = i103;
    i35 = i104 >>> i43;
    i36 = i14;
    i37 = i105;
    i38 = i7;
    i39 = i8;
    i40 = i6;
    break;
   }
   if (i102 >>> 0 < 3) {
    i43 = i102;
    i46 = i103;
    i47 = i104;
    i42 = i105;
    while (1) {
     if (!i46) {
      i16 = i14;
      i17 = i43;
      i18 = 0;
      i19 = i47;
      i20 = i42;
      i21 = i7;
      i22 = i8;
      i23 = i6;
      break L17;
     }
     i32 = i46 + -1 | 0;
     i44 = i42 + 1 | 0;
     i41 = (HEAPU8[i42 >> 0] << i43) + i47 | 0;
     i48 = i43 + 8 | 0;
     if (i48 >>> 0 < 3) {
      i43 = i48;
      i46 = i32;
      i47 = i41;
      i42 = i44;
     } else {
      i212 = i48;
      i213 = i32;
      i214 = i41;
      i215 = i44;
      break;
     }
    }
   } else {
    i212 = i102;
    i213 = i103;
    i214 = i104;
    i215 = i105;
   }
   HEAP32[i5 + 4 >> 2] = i214 & 1;
   switch (i214 >>> 1 & 3 | 0) {
   case 0:
    {
     HEAP32[i5 >> 2] = 13;
     break;
    }
   case 1:
    {
     HEAP32[i5 + 76 >> 2] = 10912;
     HEAP32[i5 + 84 >> 2] = 9;
     HEAP32[i5 + 80 >> 2] = 12960;
     HEAP32[i5 + 88 >> 2] = 5;
     HEAP32[i5 >> 2] = 19;
     if ((i2 | 0) == 6) {
      i216 = i212;
      i217 = i213;
      i218 = i214;
      i219 = i14;
      i220 = i215;
      i221 = i7;
      i222 = i8;
      i223 = i6;
      i31 = 133;
      break L17;
     }
     break;
    }
   case 2:
    {
     HEAP32[i5 >> 2] = 16;
     break;
    }
   case 3:
    {
     HEAP32[i1 + 24 >> 2] = 14487;
     HEAP32[i5 >> 2] = 29;
     break;
    }
   default:
    {}
   }
   i33 = i212 + -3 | 0;
   i34 = i213;
   i35 = i214 >>> 3;
   i36 = i14;
   i37 = i215;
   i38 = i7;
   i39 = i8;
   i40 = i6;
  } else if ((i31 | 0) == 165) {
   i31 = 0;
   i42 = HEAP32[i5 + 96 >> 2] | 0;
   i47 = HEAP32[i5 + 100 >> 2] | 0;
   do if (i134 >>> 0 < (i47 + i42 | 0) >>> 0) {
    i46 = i134;
    i43 = i47;
    i44 = i42;
    i41 = i135;
    i32 = i136;
    i48 = i137;
    i45 = i138;
    L183 : while (1) {
     i194 = (1 << HEAP32[i5 + 84 >> 2]) + -1 | 0;
     i195 = i194 & i48;
     i106 = HEAP32[i5 + 76 >> 2] | 0;
     i196 = HEAPU8[i106 + (i195 << 2) + 1 >> 0] | 0;
     if (i196 >>> 0 > i41 >>> 0) {
      i224 = i41;
      i225 = i32;
      i226 = i48;
      i227 = i45;
      while (1) {
       if (!i225) {
        i16 = i14;
        i17 = i224;
        i18 = 0;
        i19 = i226;
        i20 = i227;
        i21 = i7;
        i22 = i8;
        i23 = i139;
        break L17;
       }
       i228 = i225 + -1 | 0;
       i229 = i227 + 1 | 0;
       i230 = (HEAPU8[i227 >> 0] << i224) + i226 | 0;
       i231 = i224 + 8 | 0;
       i232 = HEAPU8[i106 + ((i194 & i230) << 2) + 1 >> 0] | 0;
       if (i232 >>> 0 > i231 >>> 0) {
        i224 = i231;
        i225 = i228;
        i226 = i230;
        i227 = i229;
       } else {
        i233 = i232;
        i234 = i194 & i230;
        i235 = i231;
        i236 = i228;
        i237 = i230;
        i238 = i229;
        break;
       }
      }
     } else {
      i233 = i196;
      i234 = i195;
      i235 = i41;
      i236 = i32;
      i237 = i48;
      i238 = i45;
     }
     i194 = HEAP16[i106 + (i234 << 2) + 2 >> 1] | 0;
     if ((i194 & 65535) >= 16) {
      switch (i194 << 16 >> 16) {
      case 16:
       {
        i227 = i233 + 2 | 0;
        if (i235 >>> 0 < i227 >>> 0) {
         i226 = i235;
         i225 = i236;
         i224 = i237;
         i229 = i238;
         while (1) {
          if (!i225) {
           i16 = i14;
           i17 = i226;
           i18 = 0;
           i19 = i224;
           i20 = i229;
           i21 = i7;
           i22 = i8;
           i23 = i139;
           break L17;
          }
          i230 = i225 + -1 | 0;
          i228 = i229 + 1 | 0;
          i231 = (HEAPU8[i229 >> 0] << i226) + i224 | 0;
          i232 = i226 + 8 | 0;
          if (i232 >>> 0 < i227 >>> 0) {
           i226 = i232;
           i225 = i230;
           i224 = i231;
           i229 = i228;
          } else {
           i239 = i232;
           i240 = i230;
           i241 = i231;
           i242 = i228;
           break;
          }
         }
        } else {
         i239 = i235;
         i240 = i236;
         i241 = i237;
         i242 = i238;
        }
        i243 = i241 >>> i233;
        i244 = i239 - i233 | 0;
        if (!i46) {
         i245 = i240;
         i246 = i242;
         i31 = 182;
         break L183;
        }
        i247 = i244 + -2 | 0;
        i248 = (i243 & 3) + 3 | 0;
        i249 = i240;
        i250 = i243 >>> 2;
        i251 = HEAPU16[i5 + 112 + (i46 + -1 << 1) >> 1] | 0;
        i252 = i242;
        break;
       }
      case 17:
       {
        i229 = i233 + 3 | 0;
        if (i235 >>> 0 < i229 >>> 0) {
         i224 = i235;
         i225 = i236;
         i226 = i237;
         i227 = i238;
         while (1) {
          if (!i225) {
           i16 = i14;
           i17 = i224;
           i18 = 0;
           i19 = i226;
           i20 = i227;
           i21 = i7;
           i22 = i8;
           i23 = i139;
           break L17;
          }
          i106 = i225 + -1 | 0;
          i195 = i227 + 1 | 0;
          i196 = (HEAPU8[i227 >> 0] << i224) + i226 | 0;
          i228 = i224 + 8 | 0;
          if (i228 >>> 0 < i229 >>> 0) {
           i224 = i228;
           i225 = i106;
           i226 = i196;
           i227 = i195;
          } else {
           i253 = i228;
           i254 = i106;
           i255 = i196;
           i256 = i195;
           break;
          }
         }
        } else {
         i253 = i235;
         i254 = i236;
         i255 = i237;
         i256 = i238;
        }
        i227 = i255 >>> i233;
        i247 = -3 - i233 + i253 | 0;
        i248 = (i227 & 7) + 3 | 0;
        i249 = i254;
        i250 = i227 >>> 3;
        i251 = 0;
        i252 = i256;
        break;
       }
      default:
       {
        i227 = i233 + 7 | 0;
        if (i235 >>> 0 < i227 >>> 0) {
         i226 = i235;
         i225 = i236;
         i224 = i237;
         i229 = i238;
         while (1) {
          if (!i225) {
           i16 = i14;
           i17 = i226;
           i18 = 0;
           i19 = i224;
           i20 = i229;
           i21 = i7;
           i22 = i8;
           i23 = i139;
           break L17;
          }
          i195 = i225 + -1 | 0;
          i196 = i229 + 1 | 0;
          i106 = (HEAPU8[i229 >> 0] << i226) + i224 | 0;
          i228 = i226 + 8 | 0;
          if (i228 >>> 0 < i227 >>> 0) {
           i226 = i228;
           i225 = i195;
           i224 = i106;
           i229 = i196;
          } else {
           i257 = i228;
           i258 = i195;
           i259 = i106;
           i260 = i196;
           break;
          }
         }
        } else {
         i257 = i235;
         i258 = i236;
         i259 = i237;
         i260 = i238;
        }
        i229 = i259 >>> i233;
        i247 = -7 - i233 + i257 | 0;
        i248 = (i229 & 127) + 11 | 0;
        i249 = i258;
        i250 = i229 >>> 7;
        i251 = 0;
        i252 = i260;
       }
      }
      if ((i46 + i248 | 0) >>> 0 > (i43 + i44 | 0) >>> 0) {
       i261 = i247;
       i262 = i249;
       i263 = i250;
       i264 = i252;
       i31 = 192;
       break;
      }
      i229 = i251 & 65535;
      i224 = i248 + -1 | 0;
      HEAP32[i5 + 104 >> 2] = i46 + 1;
      HEAP16[i5 + 112 + (i46 << 1) >> 1] = i229;
      if (!i224) {
       i265 = i247;
       i266 = i249;
       i267 = i250;
       i268 = i252;
      } else {
       i225 = i224;
       do {
        i224 = HEAP32[i5 + 104 >> 2] | 0;
        i225 = i225 + -1 | 0;
        HEAP32[i5 + 104 >> 2] = i224 + 1;
        HEAP16[i5 + 112 + (i224 << 1) >> 1] = i229;
       } while ((i225 | 0) != 0);
       i265 = i247;
       i266 = i249;
       i267 = i250;
       i268 = i252;
      }
     } else {
      if (i235 >>> 0 < i233 >>> 0) {
       i225 = i235;
       i229 = i236;
       i224 = i237;
       i226 = i238;
       while (1) {
        if (!i229) {
         i16 = i14;
         i17 = i225;
         i18 = 0;
         i19 = i224;
         i20 = i226;
         i21 = i7;
         i22 = i8;
         i23 = i139;
         break L17;
        }
        i227 = i229 + -1 | 0;
        i196 = i226 + 1 | 0;
        i106 = (HEAPU8[i226 >> 0] << i225) + i224 | 0;
        i195 = i225 + 8 | 0;
        if (i195 >>> 0 < i233 >>> 0) {
         i225 = i195;
         i229 = i227;
         i224 = i106;
         i226 = i196;
        } else {
         i269 = i195;
         i270 = i227;
         i271 = i106;
         i272 = i196;
         break;
        }
       }
      } else {
       i269 = i235;
       i270 = i236;
       i271 = i237;
       i272 = i238;
      }
      HEAP32[i5 + 104 >> 2] = i46 + 1;
      HEAP16[i5 + 112 + (i46 << 1) >> 1] = i194;
      i265 = i269 - i233 | 0;
      i266 = i270;
      i267 = i271 >>> i233;
      i268 = i272;
     }
     i46 = HEAP32[i5 + 104 >> 2] | 0;
     i273 = HEAP32[i5 + 96 >> 2] | 0;
     i43 = HEAP32[i5 + 100 >> 2] | 0;
     if (i46 >>> 0 >= (i43 + i273 | 0) >>> 0) {
      i274 = i265;
      i275 = i266;
      i276 = i267;
      i277 = i268;
      i31 = 194;
      break;
     } else {
      i44 = i273;
      i41 = i265;
      i32 = i266;
      i48 = i267;
      i45 = i268;
     }
    }
    if ((i31 | 0) == 182) {
     i31 = 0;
     HEAP32[i1 + 24 >> 2] = 14596;
     HEAP32[i5 >> 2] = 29;
     i33 = i244;
     i34 = i245;
     i35 = i243;
     i36 = i14;
     i37 = i246;
     i38 = i7;
     i39 = i8;
     i40 = i139;
     break L162;
    } else if ((i31 | 0) == 192) {
     i31 = 0;
     HEAP32[i1 + 24 >> 2] = 14596;
     HEAP32[i5 >> 2] = 29;
     i33 = i261;
     i34 = i262;
     i35 = i263;
     i36 = i14;
     i37 = i264;
     i38 = i7;
     i39 = i8;
     i40 = i139;
     break L162;
    } else if ((i31 | 0) == 194) {
     i31 = 0;
     if ((HEAP32[i5 >> 2] | 0) == 29) {
      i33 = i274;
      i34 = i275;
      i35 = i276;
      i36 = i14;
      i37 = i277;
      i38 = i7;
      i39 = i8;
      i40 = i139;
      break L162;
     } else {
      i278 = i273;
      i279 = i274;
      i280 = i275;
      i281 = i276;
      i282 = i277;
      break;
     }
    }
   } else {
    i278 = i42;
    i279 = i135;
    i280 = i136;
    i281 = i137;
    i282 = i138;
   } while (0);
   if (!(HEAP16[i5 + 624 >> 1] | 0)) {
    HEAP32[i1 + 24 >> 2] = 14622;
    HEAP32[i5 >> 2] = 29;
    i33 = i279;
    i34 = i280;
    i35 = i281;
    i36 = i14;
    i37 = i282;
    i38 = i7;
    i39 = i8;
    i40 = i139;
    break;
   }
   HEAP32[i5 + 108 >> 2] = i5 + 1328;
   HEAP32[i5 + 76 >> 2] = i5 + 1328;
   HEAP32[i5 + 84 >> 2] = 9;
   i42 = _inflate_table(1, i5 + 112 | 0, i278, i5 + 108 | 0, i5 + 84 | 0, i5 + 752 | 0) | 0;
   if (i42) {
    HEAP32[i1 + 24 >> 2] = 14659;
    HEAP32[i5 >> 2] = 29;
    i33 = i279;
    i34 = i280;
    i35 = i281;
    i36 = i14;
    i37 = i282;
    i38 = i7;
    i39 = i8;
    i40 = i42;
    break;
   }
   HEAP32[i5 + 80 >> 2] = HEAP32[i5 + 108 >> 2];
   HEAP32[i5 + 88 >> 2] = 6;
   i42 = _inflate_table(2, i5 + 112 + (HEAP32[i5 + 96 >> 2] << 1) | 0, HEAP32[i5 + 100 >> 2] | 0, i5 + 108 | 0, i5 + 88 | 0, i5 + 752 | 0) | 0;
   if (!i42) {
    HEAP32[i5 >> 2] = 19;
    if ((i2 | 0) == 6) {
     i16 = i14;
     i17 = i279;
     i18 = i280;
     i19 = i281;
     i20 = i282;
     i21 = i7;
     i22 = i8;
     i23 = 0;
     break L17;
    } else {
     i140 = i279;
     i141 = i280;
     i142 = i281;
     i143 = i282;
     i144 = 0;
     i31 = 202;
     break;
    }
   } else {
    HEAP32[i1 + 24 >> 2] = 14687;
    HEAP32[i5 >> 2] = 29;
    i33 = i279;
    i34 = i280;
    i35 = i281;
    i36 = i14;
    i37 = i282;
    i38 = i7;
    i39 = i8;
    i40 = i42;
    break;
   }
  } while (0);
  if ((i31 | 0) == 57) {
   i31 = 0;
   i42 = HEAP32[i5 + 32 >> 2] | 0;
   if (i42) {
    HEAP32[i42 + 8 >> 2] = i69 & 255;
    HEAP32[i42 + 12 >> 2] = i69 >>> 8;
   }
   if (HEAP32[i5 + 16 >> 2] & 512) {
    HEAP8[i3 >> 0] = i69;
    HEAP8[i3 + 1 >> 0] = i69 >>> 8;
    HEAP32[i5 + 24 >> 2] = _crc32(HEAP32[i5 + 24 >> 2] | 0, i3, 2) | 0;
   }
   HEAP32[i5 >> 2] = 4;
   i71 = 0;
   i72 = i68;
   i73 = 0;
   i74 = i70;
   i31 = 62;
  } else if ((i31 | 0) == 202) {
   i31 = 0;
   HEAP32[i5 >> 2] = 20;
   i145 = i140;
   i146 = i141;
   i147 = i142;
   i148 = i143;
   i149 = i144;
   i31 = 203;
  }
  do if ((i31 | 0) == 62) {
   i31 = 0;
   i42 = HEAP32[i5 + 16 >> 2] | 0;
   if (!(i42 & 1024)) {
    i47 = HEAP32[i5 + 32 >> 2] | 0;
    if (!i47) {
     i283 = i71;
     i284 = i72;
     i285 = i73;
     i286 = i74;
    } else {
     HEAP32[i47 + 16 >> 2] = 0;
     i283 = i71;
     i284 = i72;
     i285 = i73;
     i286 = i74;
    }
   } else {
    if (i71 >>> 0 < 16) {
     i47 = i71;
     i45 = i72;
     i48 = i73;
     i32 = i74;
     while (1) {
      if (!i45) {
       i16 = i14;
       i17 = i47;
       i18 = 0;
       i19 = i48;
       i20 = i32;
       i21 = i7;
       i22 = i8;
       i23 = i6;
       break L17;
      }
      i41 = i45 + -1 | 0;
      i44 = i32 + 1 | 0;
      i43 = (HEAPU8[i32 >> 0] << i47) + i48 | 0;
      i47 = i47 + 8 | 0;
      if (i47 >>> 0 >= 16) {
       i287 = i41;
       i288 = i43;
       i289 = i44;
       break;
      } else {
       i45 = i41;
       i48 = i43;
       i32 = i44;
      }
     }
    } else {
     i287 = i72;
     i288 = i73;
     i289 = i74;
    }
    HEAP32[i5 + 64 >> 2] = i288;
    i32 = HEAP32[i5 + 32 >> 2] | 0;
    if (i32) HEAP32[i32 + 20 >> 2] = i288;
    if (!(i42 & 512)) {
     i283 = 0;
     i284 = i287;
     i285 = 0;
     i286 = i289;
    } else {
     HEAP8[i3 >> 0] = i288;
     HEAP8[i3 + 1 >> 0] = i288 >>> 8;
     HEAP32[i5 + 24 >> 2] = _crc32(HEAP32[i5 + 24 >> 2] | 0, i3, 2) | 0;
     i283 = 0;
     i284 = i287;
     i285 = 0;
     i286 = i289;
    }
   }
   HEAP32[i5 >> 2] = 5;
   i75 = i283;
   i76 = i284;
   i77 = i285;
   i78 = i286;
   i31 = 73;
  } else if ((i31 | 0) == 203) {
   i31 = 0;
   if (i14 >>> 0 > 257 & i146 >>> 0 > 5) {
    HEAP32[i1 + 12 >> 2] = i8;
    HEAP32[i1 + 16 >> 2] = i14;
    HEAP32[i1 >> 2] = i148;
    HEAP32[i1 + 4 >> 2] = i146;
    HEAP32[i5 + 56 >> 2] = i147;
    HEAP32[i5 + 60 >> 2] = i145;
    _inflate_fast(i1, i7);
    i32 = HEAP32[i1 + 12 >> 2] | 0;
    i48 = HEAP32[i1 + 16 >> 2] | 0;
    i45 = HEAP32[i1 >> 2] | 0;
    i47 = HEAP32[i1 + 4 >> 2] | 0;
    i44 = HEAP32[i5 + 56 >> 2] | 0;
    i43 = HEAP32[i5 + 60 >> 2] | 0;
    if ((HEAP32[i5 >> 2] | 0) != 11) {
     i33 = i43;
     i34 = i47;
     i35 = i44;
     i36 = i48;
     i37 = i45;
     i38 = i7;
     i39 = i32;
     i40 = i149;
     break;
    }
    HEAP32[i5 + 7108 >> 2] = -1;
    i33 = i43;
    i34 = i47;
    i35 = i44;
    i36 = i48;
    i37 = i45;
    i38 = i7;
    i39 = i32;
    i40 = i149;
    break;
   }
   HEAP32[i5 + 7108 >> 2] = 0;
   i32 = (1 << HEAP32[i5 + 84 >> 2]) + -1 | 0;
   i45 = i32 & i147;
   i48 = HEAP32[i5 + 76 >> 2] | 0;
   i44 = HEAP8[i48 + (i45 << 2) + 1 >> 0] | 0;
   if ((i44 & 255) >>> 0 > i145 >>> 0) {
    i47 = i145;
    i43 = i146;
    i41 = i147;
    i46 = i148;
    while (1) {
     if (!i43) {
      i16 = i14;
      i17 = i47;
      i18 = 0;
      i19 = i41;
      i20 = i46;
      i21 = i7;
      i22 = i8;
      i23 = i149;
      break L17;
     }
     i226 = i43 + -1 | 0;
     i224 = i46 + 1 | 0;
     i229 = (HEAPU8[i46 >> 0] << i47) + i41 | 0;
     i225 = i47 + 8 | 0;
     i196 = HEAP8[i48 + ((i32 & i229) << 2) + 1 >> 0] | 0;
     if ((i196 & 255) >>> 0 > i225 >>> 0) {
      i47 = i225;
      i43 = i226;
      i41 = i229;
      i46 = i224;
     } else {
      i290 = i196;
      i291 = i196 & 255;
      i292 = i32 & i229;
      i293 = i225;
      i294 = i226;
      i295 = i229;
      i296 = i224;
      break;
     }
    }
   } else {
    i290 = i44;
    i291 = i44 & 255;
    i292 = i45;
    i293 = i145;
    i294 = i146;
    i295 = i147;
    i296 = i148;
   }
   i32 = HEAP8[i48 + (i292 << 2) >> 0] | 0;
   i46 = HEAP16[i48 + (i292 << 2) + 2 >> 1] | 0;
   if (i32 << 24 >> 24 != 0 & (i32 & 240 | 0) == 0) {
    i41 = (1 << i291 + (i32 & 255)) + -1 | 0;
    i43 = ((i295 & i41) >>> i291) + (i46 & 65535) | 0;
    i47 = HEAP8[i48 + (i43 << 2) + 1 >> 0] | 0;
    if (((i47 & 255) + i291 | 0) >>> 0 > i293 >>> 0) {
     i42 = i293;
     i224 = i294;
     i229 = i295;
     i226 = i296;
     while (1) {
      if (!i224) {
       i16 = i14;
       i17 = i42;
       i18 = 0;
       i19 = i229;
       i20 = i226;
       i21 = i7;
       i22 = i8;
       i23 = i149;
       break L17;
      }
      i225 = i224 + -1 | 0;
      i196 = i226 + 1 | 0;
      i106 = (HEAPU8[i226 >> 0] << i42) + i229 | 0;
      i227 = i42 + 8 | 0;
      i195 = ((i106 & i41) >>> i291) + (i46 & 65535) | 0;
      i228 = HEAP8[i48 + (i195 << 2) + 1 >> 0] | 0;
      if (((i228 & 255) + i291 | 0) >>> 0 > i227 >>> 0) {
       i42 = i227;
       i224 = i225;
       i229 = i106;
       i226 = i196;
      } else {
       i297 = i195;
       i298 = i228;
       i299 = i227;
       i300 = i225;
       i301 = i106;
       i302 = i196;
       break;
      }
     }
    } else {
     i297 = i43;
     i298 = i47;
     i299 = i293;
     i300 = i294;
     i301 = i295;
     i302 = i296;
    }
    i226 = HEAP16[i48 + (i297 << 2) + 2 >> 1] | 0;
    i229 = HEAP8[i48 + (i297 << 2) >> 0] | 0;
    HEAP32[i5 + 7108 >> 2] = i291;
    i303 = i291;
    i304 = i299 - i291 | 0;
    i305 = i300;
    i306 = i229;
    i307 = i298;
    i308 = i226;
    i309 = i301 >>> i291;
    i310 = i302;
   } else {
    i303 = 0;
    i304 = i293;
    i305 = i294;
    i306 = i32;
    i307 = i290;
    i308 = i46;
    i309 = i295;
    i310 = i296;
   }
   i226 = i307 & 255;
   i229 = i309 >>> i226;
   i224 = i304 - i226 | 0;
   HEAP32[i5 + 7108 >> 2] = i303 + i226;
   HEAP32[i5 + 64 >> 2] = i308 & 65535;
   i226 = i306 & 255;
   if (!(i306 << 24 >> 24)) {
    HEAP32[i5 >> 2] = 25;
    i33 = i224;
    i34 = i305;
    i35 = i229;
    i36 = i14;
    i37 = i310;
    i38 = i7;
    i39 = i8;
    i40 = i149;
    break;
   }
   if (i226 & 32) {
    HEAP32[i5 + 7108 >> 2] = -1;
    HEAP32[i5 >> 2] = 11;
    i33 = i224;
    i34 = i305;
    i35 = i229;
    i36 = i14;
    i37 = i310;
    i38 = i7;
    i39 = i8;
    i40 = i149;
    break;
   }
   if (!(i226 & 64)) {
    HEAP32[i5 + 72 >> 2] = i226 & 15;
    HEAP32[i5 >> 2] = 21;
    i150 = i226 & 15;
    i151 = i224;
    i152 = i305;
    i153 = i229;
    i154 = i310;
    i155 = i149;
    i31 = 221;
    break;
   } else {
    HEAP32[i1 + 24 >> 2] = 14805;
    HEAP32[i5 >> 2] = 29;
    i33 = i224;
    i34 = i305;
    i35 = i229;
    i36 = i14;
    i37 = i310;
    i38 = i7;
    i39 = i8;
    i40 = i149;
    break;
   }
  } while (0);
  if ((i31 | 0) == 73) {
   i31 = 0;
   i229 = HEAP32[i5 + 16 >> 2] | 0;
   if (i229 & 1024) {
    i224 = HEAP32[i5 + 64 >> 2] | 0;
    i226 = i224 >>> 0 > i76 >>> 0 ? i76 : i224;
    if (!i226) {
     i311 = i224;
     i312 = i76;
     i313 = i78;
    } else {
     i42 = HEAP32[i5 + 32 >> 2] | 0;
     if ((i42 | 0) != 0 ? (i41 = HEAP32[i42 + 16 >> 2] | 0, (i41 | 0) != 0) : 0) {
      i45 = (HEAP32[i42 + 20 >> 2] | 0) - i224 | 0;
      i224 = HEAP32[i42 + 24 >> 2] | 0;
      _memcpy(i41 + i45 | 0, i78 | 0, ((i45 + i226 | 0) >>> 0 > i224 >>> 0 ? i224 - i45 | 0 : i226) | 0) | 0;
      i314 = HEAP32[i5 + 16 >> 2] | 0;
     } else i314 = i229;
     if (i314 & 512) HEAP32[i5 + 24 >> 2] = _crc32(HEAP32[i5 + 24 >> 2] | 0, i78, i226) | 0;
     i229 = (HEAP32[i5 + 64 >> 2] | 0) - i226 | 0;
     HEAP32[i5 + 64 >> 2] = i229;
     i311 = i229;
     i312 = i76 - i226 | 0;
     i313 = i78 + i226 | 0;
    }
    if (!i311) {
     i315 = i312;
     i316 = i313;
    } else {
     i16 = i14;
     i17 = i75;
     i18 = i312;
     i19 = i77;
     i20 = i313;
     i21 = i7;
     i22 = i8;
     i23 = i6;
     break;
    }
   } else {
    i315 = i76;
    i316 = i78;
   }
   HEAP32[i5 + 64 >> 2] = 0;
   HEAP32[i5 >> 2] = 6;
   i79 = i75;
   i80 = i315;
   i81 = i77;
   i82 = i316;
   i31 = 83;
  } else if ((i31 | 0) == 221) {
   i31 = 0;
   if (!i150) {
    i317 = HEAP32[i5 + 64 >> 2] | 0;
    i318 = i151;
    i319 = i152;
    i320 = i153;
    i321 = i154;
   } else {
    if (i151 >>> 0 < i150 >>> 0) {
     i226 = i151;
     i229 = i152;
     i45 = i153;
     i224 = i154;
     while (1) {
      if (!i229) {
       i16 = i14;
       i17 = i226;
       i18 = 0;
       i19 = i45;
       i20 = i224;
       i21 = i7;
       i22 = i8;
       i23 = i155;
       break L17;
      }
      i41 = i229 + -1 | 0;
      i42 = i224 + 1 | 0;
      i44 = (HEAPU8[i224 >> 0] << i226) + i45 | 0;
      i196 = i226 + 8 | 0;
      if (i196 >>> 0 < i150 >>> 0) {
       i226 = i196;
       i229 = i41;
       i45 = i44;
       i224 = i42;
      } else {
       i322 = i196;
       i323 = i41;
       i324 = i44;
       i325 = i42;
       break;
      }
     }
    } else {
     i322 = i151;
     i323 = i152;
     i324 = i153;
     i325 = i154;
    }
    i224 = (HEAP32[i5 + 64 >> 2] | 0) + ((1 << i150) + -1 & i324) | 0;
    HEAP32[i5 + 64 >> 2] = i224;
    HEAP32[i5 + 7108 >> 2] = (HEAP32[i5 + 7108 >> 2] | 0) + i150;
    i317 = i224;
    i318 = i322 - i150 | 0;
    i319 = i323;
    i320 = i324 >>> i150;
    i321 = i325;
   }
   HEAP32[i5 + 7112 >> 2] = i317;
   HEAP32[i5 >> 2] = 22;
   i156 = i318;
   i157 = i319;
   i158 = i320;
   i159 = i321;
   i160 = i155;
   i31 = 228;
  }
  do if ((i31 | 0) == 83) {
   i31 = 0;
   if (!(HEAP32[i5 + 16 >> 2] & 2048)) {
    i224 = HEAP32[i5 + 32 >> 2] | 0;
    if (!i224) {
     i326 = i80;
     i327 = i82;
    } else {
     HEAP32[i224 + 28 >> 2] = 0;
     i326 = i80;
     i327 = i82;
    }
   } else {
    if (!i80) {
     i16 = i14;
     i17 = i79;
     i18 = 0;
     i19 = i81;
     i20 = i82;
     i21 = i7;
     i22 = i8;
     i23 = i6;
     break L17;
    } else i328 = 0;
    while (1) {
     i329 = i328 + 1 | 0;
     i330 = HEAP8[i82 + i328 >> 0] | 0;
     i224 = HEAP32[i5 + 32 >> 2] | 0;
     if (((i224 | 0) != 0 ? (i45 = HEAP32[i224 + 28 >> 2] | 0, (i45 | 0) != 0) : 0) ? (i229 = HEAP32[i5 + 64 >> 2] | 0, i229 >>> 0 < (HEAP32[i224 + 32 >> 2] | 0) >>> 0) : 0) {
      HEAP32[i5 + 64 >> 2] = i229 + 1;
      HEAP8[i45 + i229 >> 0] = i330;
     }
     if (i330 << 24 >> 24 != 0 & i80 >>> 0 > i329 >>> 0) i328 = i329; else break;
    }
    if (HEAP32[i5 + 16 >> 2] & 512) HEAP32[i5 + 24 >> 2] = _crc32(HEAP32[i5 + 24 >> 2] | 0, i82, i329) | 0;
    i46 = i80 - i329 | 0;
    i32 = i82 + i329 | 0;
    if (i330 << 24 >> 24) {
     i16 = i14;
     i17 = i79;
     i18 = i46;
     i19 = i81;
     i20 = i32;
     i21 = i7;
     i22 = i8;
     i23 = i6;
     break L17;
    } else {
     i326 = i46;
     i327 = i32;
    }
   }
   HEAP32[i5 + 64 >> 2] = 0;
   HEAP32[i5 >> 2] = 7;
   i83 = i79;
   i84 = i326;
   i85 = i81;
   i86 = i327;
   i31 = 96;
  } else if ((i31 | 0) == 228) {
   i31 = 0;
   i32 = (1 << HEAP32[i5 + 88 >> 2]) + -1 | 0;
   i46 = i32 & i158;
   i48 = HEAP32[i5 + 80 >> 2] | 0;
   i47 = HEAP8[i48 + (i46 << 2) + 1 >> 0] | 0;
   if ((i47 & 255) >>> 0 > i156 >>> 0) {
    i43 = i156;
    i229 = i157;
    i45 = i158;
    i224 = i159;
    while (1) {
     if (!i229) {
      i16 = i14;
      i17 = i43;
      i18 = 0;
      i19 = i45;
      i20 = i224;
      i21 = i7;
      i22 = i8;
      i23 = i160;
      break L17;
     }
     i226 = i229 + -1 | 0;
     i42 = i224 + 1 | 0;
     i44 = (HEAPU8[i224 >> 0] << i43) + i45 | 0;
     i41 = i43 + 8 | 0;
     i196 = HEAP8[i48 + ((i32 & i44) << 2) + 1 >> 0] | 0;
     if ((i196 & 255) >>> 0 > i41 >>> 0) {
      i43 = i41;
      i229 = i226;
      i45 = i44;
      i224 = i42;
     } else {
      i331 = i196;
      i332 = i196 & 255;
      i333 = i32 & i44;
      i334 = i41;
      i335 = i226;
      i336 = i44;
      i337 = i42;
      break;
     }
    }
   } else {
    i331 = i47;
    i332 = i47 & 255;
    i333 = i46;
    i334 = i156;
    i335 = i157;
    i336 = i158;
    i337 = i159;
   }
   i32 = HEAP8[i48 + (i333 << 2) >> 0] | 0;
   i224 = HEAP16[i48 + (i333 << 2) + 2 >> 1] | 0;
   if (!(i32 & 240)) {
    i45 = (1 << i332 + (i32 & 255)) + -1 | 0;
    i229 = ((i336 & i45) >>> i332) + (i224 & 65535) | 0;
    i43 = HEAP8[i48 + (i229 << 2) + 1 >> 0] | 0;
    if (((i43 & 255) + i332 | 0) >>> 0 > i334 >>> 0) {
     i42 = i334;
     i44 = i335;
     i226 = i336;
     i41 = i337;
     while (1) {
      if (!i44) {
       i16 = i14;
       i17 = i42;
       i18 = 0;
       i19 = i226;
       i20 = i41;
       i21 = i7;
       i22 = i8;
       i23 = i160;
       break L17;
      }
      i196 = i44 + -1 | 0;
      i106 = i41 + 1 | 0;
      i225 = (HEAPU8[i41 >> 0] << i42) + i226 | 0;
      i227 = i42 + 8 | 0;
      i228 = ((i225 & i45) >>> i332) + (i224 & 65535) | 0;
      i195 = HEAP8[i48 + (i228 << 2) + 1 >> 0] | 0;
      if (((i195 & 255) + i332 | 0) >>> 0 > i227 >>> 0) {
       i42 = i227;
       i44 = i196;
       i226 = i225;
       i41 = i106;
      } else {
       i338 = i228;
       i339 = i195;
       i340 = i227;
       i341 = i196;
       i342 = i225;
       i343 = i106;
       break;
      }
     }
    } else {
     i338 = i229;
     i339 = i43;
     i340 = i334;
     i341 = i335;
     i342 = i336;
     i343 = i337;
    }
    i41 = HEAP16[i48 + (i338 << 2) + 2 >> 1] | 0;
    i226 = HEAP8[i48 + (i338 << 2) >> 0] | 0;
    i44 = (HEAP32[i5 + 7108 >> 2] | 0) + i332 | 0;
    HEAP32[i5 + 7108 >> 2] = i44;
    i344 = i44;
    i345 = i340 - i332 | 0;
    i346 = i341;
    i347 = i226;
    i348 = i339;
    i349 = i41;
    i350 = i342 >>> i332;
    i351 = i343;
   } else {
    i344 = HEAP32[i5 + 7108 >> 2] | 0;
    i345 = i334;
    i346 = i335;
    i347 = i32;
    i348 = i331;
    i349 = i224;
    i350 = i336;
    i351 = i337;
   }
   i41 = i348 & 255;
   i226 = i350 >>> i41;
   i44 = i345 - i41 | 0;
   HEAP32[i5 + 7108 >> 2] = i344 + i41;
   i41 = i347 & 255;
   if (!(i41 & 64)) {
    HEAP32[i5 + 68 >> 2] = i349 & 65535;
    HEAP32[i5 + 72 >> 2] = i41 & 15;
    HEAP32[i5 >> 2] = 23;
    i161 = i41 & 15;
    i162 = i44;
    i163 = i346;
    i164 = i226;
    i165 = i351;
    i166 = i160;
    i31 = 240;
    break;
   } else {
    HEAP32[i1 + 24 >> 2] = 14783;
    HEAP32[i5 >> 2] = 29;
    i33 = i44;
    i34 = i346;
    i35 = i226;
    i36 = i14;
    i37 = i351;
    i38 = i7;
    i39 = i8;
    i40 = i160;
    break;
   }
  } while (0);
  if ((i31 | 0) == 96) {
   i31 = 0;
   if (!(HEAP32[i5 + 16 >> 2] & 4096)) {
    i226 = HEAP32[i5 + 32 >> 2] | 0;
    if (!i226) {
     i352 = i84;
     i353 = i86;
    } else {
     HEAP32[i226 + 36 >> 2] = 0;
     i352 = i84;
     i353 = i86;
    }
   } else {
    if (!i84) {
     i16 = i14;
     i17 = i83;
     i18 = 0;
     i19 = i85;
     i20 = i86;
     i21 = i7;
     i22 = i8;
     i23 = i6;
     break;
    } else i354 = 0;
    while (1) {
     i355 = i354 + 1 | 0;
     i356 = HEAP8[i86 + i354 >> 0] | 0;
     i226 = HEAP32[i5 + 32 >> 2] | 0;
     if (((i226 | 0) != 0 ? (i44 = HEAP32[i226 + 36 >> 2] | 0, (i44 | 0) != 0) : 0) ? (i41 = HEAP32[i5 + 64 >> 2] | 0, i41 >>> 0 < (HEAP32[i226 + 40 >> 2] | 0) >>> 0) : 0) {
      HEAP32[i5 + 64 >> 2] = i41 + 1;
      HEAP8[i44 + i41 >> 0] = i356;
     }
     if (i356 << 24 >> 24 != 0 & i84 >>> 0 > i355 >>> 0) i354 = i355; else break;
    }
    if (HEAP32[i5 + 16 >> 2] & 512) HEAP32[i5 + 24 >> 2] = _crc32(HEAP32[i5 + 24 >> 2] | 0, i86, i355) | 0;
    i41 = i84 - i355 | 0;
    i44 = i86 + i355 | 0;
    if (i356 << 24 >> 24) {
     i16 = i14;
     i17 = i83;
     i18 = i41;
     i19 = i85;
     i20 = i44;
     i21 = i7;
     i22 = i8;
     i23 = i6;
     break;
    } else {
     i352 = i41;
     i353 = i44;
    }
   }
   HEAP32[i5 >> 2] = 8;
   i87 = i83;
   i88 = i352;
   i89 = i85;
   i90 = i353;
   i31 = 109;
  } else if ((i31 | 0) == 240) {
   i31 = 0;
   if (!i161) {
    i357 = i162;
    i358 = i163;
    i359 = i164;
    i360 = i165;
   } else {
    if (i162 >>> 0 < i161 >>> 0) {
     i44 = i162;
     i41 = i163;
     i226 = i164;
     i42 = i165;
     while (1) {
      if (!i41) {
       i16 = i14;
       i17 = i44;
       i18 = 0;
       i19 = i226;
       i20 = i42;
       i21 = i7;
       i22 = i8;
       i23 = i166;
       break L17;
      }
      i45 = i41 + -1 | 0;
      i46 = i42 + 1 | 0;
      i47 = (HEAPU8[i42 >> 0] << i44) + i226 | 0;
      i106 = i44 + 8 | 0;
      if (i106 >>> 0 < i161 >>> 0) {
       i44 = i106;
       i41 = i45;
       i226 = i47;
       i42 = i46;
      } else {
       i361 = i106;
       i362 = i45;
       i363 = i47;
       i364 = i46;
       break;
      }
     }
    } else {
     i361 = i162;
     i362 = i163;
     i363 = i164;
     i364 = i165;
    }
    HEAP32[i5 + 68 >> 2] = (HEAP32[i5 + 68 >> 2] | 0) + ((1 << i161) + -1 & i363);
    HEAP32[i5 + 7108 >> 2] = (HEAP32[i5 + 7108 >> 2] | 0) + i161;
    i357 = i361 - i161 | 0;
    i358 = i362;
    i359 = i363 >>> i161;
    i360 = i364;
   }
   HEAP32[i5 >> 2] = 24;
   i167 = i357;
   i168 = i358;
   i169 = i359;
   i170 = i360;
   i171 = i166;
   i31 = 246;
  }
  do if ((i31 | 0) == 109) {
   i31 = 0;
   i42 = HEAP32[i5 + 16 >> 2] | 0;
   if (i42 & 512) {
    if (i87 >>> 0 < 16) {
     i226 = i87;
     i41 = i88;
     i44 = i89;
     i46 = i90;
     while (1) {
      if (!i41) {
       i16 = i14;
       i17 = i226;
       i18 = 0;
       i19 = i44;
       i20 = i46;
       i21 = i7;
       i22 = i8;
       i23 = i6;
       break L17;
      }
      i47 = i41 + -1 | 0;
      i45 = i46 + 1 | 0;
      i106 = (HEAPU8[i46 >> 0] << i226) + i44 | 0;
      i225 = i226 + 8 | 0;
      if (i225 >>> 0 < 16) {
       i226 = i225;
       i41 = i47;
       i44 = i106;
       i46 = i45;
      } else {
       i365 = i225;
       i366 = i47;
       i367 = i106;
       i368 = i45;
       break;
      }
     }
    } else {
     i365 = i87;
     i366 = i88;
     i367 = i89;
     i368 = i90;
    }
    if ((i367 | 0) == (HEAP32[i5 + 24 >> 2] & 65535 | 0)) {
     i369 = 0;
     i370 = i366;
     i371 = 0;
     i372 = i368;
    } else {
     HEAP32[i1 + 24 >> 2] = 14467;
     HEAP32[i5 >> 2] = 29;
     i33 = i365;
     i34 = i366;
     i35 = i367;
     i36 = i14;
     i37 = i368;
     i38 = i7;
     i39 = i8;
     i40 = i6;
     break;
    }
   } else {
    i369 = i87;
    i370 = i88;
    i371 = i89;
    i372 = i90;
   }
   i46 = HEAP32[i5 + 32 >> 2] | 0;
   if (i46) {
    HEAP32[i46 + 44 >> 2] = i42 >>> 9 & 1;
    HEAP32[i46 + 48 >> 2] = 1;
   }
   i46 = _crc32(0, 0, 0) | 0;
   HEAP32[i5 + 24 >> 2] = i46;
   HEAP32[i1 + 48 >> 2] = i46;
   HEAP32[i5 >> 2] = 11;
   i33 = i369;
   i34 = i370;
   i35 = i371;
   i36 = i14;
   i37 = i372;
   i38 = i7;
   i39 = i8;
   i40 = i6;
  } else if ((i31 | 0) == 246) {
   i31 = 0;
   if (!i14) {
    i16 = 0;
    i17 = i167;
    i18 = i168;
    i19 = i169;
    i20 = i170;
    i21 = i7;
    i22 = i8;
    i23 = i171;
    break L17;
   }
   i46 = i7 - i14 | 0;
   i44 = HEAP32[i5 + 68 >> 2] | 0;
   if (i44 >>> 0 > i46 >>> 0) {
    if ((i44 - i46 | 0) >>> 0 > (HEAP32[i5 + 44 >> 2] | 0) >>> 0 ? (HEAP32[i5 + 7104 >> 2] | 0) != 0 : 0) {
     HEAP32[i1 + 24 >> 2] = 14753;
     HEAP32[i5 >> 2] = 29;
     i33 = i167;
     i34 = i168;
     i35 = i169;
     i36 = i14;
     i37 = i170;
     i38 = i7;
     i39 = i8;
     i40 = i171;
     break;
    }
    i41 = HEAP32[i5 + 48 >> 2] | 0;
    if ((i44 - i46 | 0) >>> 0 > i41 >>> 0) {
     i373 = i44 - i46 - i41 | 0;
     i374 = (HEAP32[i5 + 52 >> 2] | 0) + ((HEAP32[i5 + 40 >> 2] | 0) - (i44 - i46 - i41)) | 0;
    } else {
     i373 = i44 - i46 | 0;
     i374 = (HEAP32[i5 + 52 >> 2] | 0) + (i41 - (i44 - i46)) | 0;
    }
    i46 = HEAP32[i5 + 64 >> 2] | 0;
    i375 = i46;
    i376 = i373 >>> 0 > i46 >>> 0 ? i46 : i373;
    i377 = i374;
   } else {
    i46 = HEAP32[i5 + 64 >> 2] | 0;
    i375 = i46;
    i376 = i46;
    i377 = i8 + (0 - i44) | 0;
   }
   i44 = i376 >>> 0 > i14 >>> 0 ? i14 : i376;
   HEAP32[i5 + 64 >> 2] = i375 - i44;
   i46 = i376 >>> 0 > i14 >>> 0 ? i14 : i376;
   i41 = i44;
   i226 = i377;
   i224 = i8;
   while (1) {
    HEAP8[i224 >> 0] = HEAP8[i226 >> 0] | 0;
    i41 = i41 + -1 | 0;
    if (!i41) break; else {
     i226 = i226 + 1 | 0;
     i224 = i224 + 1 | 0;
    }
   }
   i224 = i14 - i44 | 0;
   i226 = i8 + i46 | 0;
   if (!(HEAP32[i5 + 64 >> 2] | 0)) {
    HEAP32[i5 >> 2] = 20;
    i33 = i167;
    i34 = i168;
    i35 = i169;
    i36 = i224;
    i37 = i170;
    i38 = i7;
    i39 = i226;
    i40 = i171;
   } else {
    i33 = i167;
    i34 = i168;
    i35 = i169;
    i36 = i224;
    i37 = i170;
    i38 = i7;
    i39 = i226;
    i40 = i171;
   }
  } while (0);
  i11 = HEAP32[i5 >> 2] | 0;
  i9 = i33;
  i12 = i34;
  i13 = i35;
  i14 = i36;
  i15 = i37;
  i7 = i38;
  i8 = i39;
  i6 = i40;
 }
 if ((i31 | 0) == 122) {
  HEAP32[i1 + 12 >> 2] = i193;
  HEAP32[i1 + 16 >> 2] = i191;
  HEAP32[i1 >> 2] = i192;
  HEAP32[i1 + 4 >> 2] = i189;
  HEAP32[i5 + 56 >> 2] = i190;
  HEAP32[i5 + 60 >> 2] = i188;
  i4 = 2;
  STACKTOP = i3;
  return i4 | 0;
 } else if ((i31 | 0) == 133) {
  i16 = i219;
  i17 = i216 + -3 | 0;
  i18 = i217;
  i19 = i218 >>> 3;
  i20 = i220;
  i21 = i221;
  i22 = i222;
  i23 = i223;
 } else if ((i31 | 0) == 283) {
  HEAP32[i5 >> 2] = 28;
  i16 = i204;
  i17 = i201;
  i18 = i202;
  i19 = i203;
  i20 = i205;
  i21 = i206;
  i22 = i207;
  i23 = 1;
 } else if ((i31 | 0) == 284) {
  i16 = i27;
  i17 = i24;
  i18 = i25;
  i19 = i26;
  i20 = i28;
  i21 = i29;
  i22 = i30;
  i23 = -3;
 } else if ((i31 | 0) == 296) {
  i4 = -2;
  STACKTOP = i3;
  return i4 | 0;
 } else if ((i31 | 0) == 297) {
  STACKTOP = i3;
  return i4 | 0;
 }
 HEAP32[i1 + 12 >> 2] = i22;
 HEAP32[i1 + 16 >> 2] = i16;
 HEAP32[i1 >> 2] = i20;
 HEAP32[i1 + 4 >> 2] = i18;
 HEAP32[i5 + 56 >> 2] = i19;
 HEAP32[i5 + 60 >> 2] = i17;
 if ((HEAP32[i5 + 40 >> 2] | 0) == 0 ? (HEAP32[i5 >> 2] | 0) >>> 0 > 25 | (i21 | 0) == (i16 | 0) : 0) {
  i378 = i18;
  i379 = i16;
 } else i31 = 287;
 do if ((i31 | 0) == 287) {
  if (!(_updatewindow(i1, i21) | 0)) {
   i378 = HEAP32[i1 + 4 >> 2] | 0;
   i379 = HEAP32[i1 + 16 >> 2] | 0;
   break;
  }
  HEAP32[i5 >> 2] = 30;
  i4 = -4;
  STACKTOP = i3;
  return i4 | 0;
 } while (0);
 i31 = i21 - i379 | 0;
 HEAP32[i1 + 8 >> 2] = i10 - i378 + (HEAP32[i1 + 8 >> 2] | 0);
 HEAP32[i1 + 20 >> 2] = (HEAP32[i1 + 20 >> 2] | 0) + i31;
 HEAP32[i5 + 28 >> 2] = (HEAP32[i5 + 28 >> 2] | 0) + i31;
 if ((i21 | 0) != (i379 | 0) & (HEAP32[i5 + 8 >> 2] | 0) != 0) {
  i16 = HEAP32[i5 + 24 >> 2] | 0;
  i18 = (HEAP32[i1 + 12 >> 2] | 0) + (0 - i31) | 0;
  if (!(HEAP32[i5 + 16 >> 2] | 0)) i380 = _adler32(i16, i18, i31) | 0; else i380 = _crc32(i16, i18, i31) | 0;
  HEAP32[i5 + 24 >> 2] = i380;
  HEAP32[i1 + 48 >> 2] = i380;
 }
 i380 = HEAP32[i5 >> 2] | 0;
 HEAP32[i1 + 44 >> 2] = ((HEAP32[i5 + 4 >> 2] | 0) != 0 ? 64 : 0) + (HEAP32[i5 + 60 >> 2] | 0) + ((i380 | 0) == 11 ? 128 : 0) + ((i380 | 0) == 19 | (i380 | 0) == 14 ? 256 : 0);
 i4 = (i23 | 0) == 0 & ((i2 | 0) == 4 | (i10 | 0) == (i378 | 0) & (i21 | 0) == (i379 | 0)) ? -5 : i23;
 STACKTOP = i3;
 return i4 | 0;
}

function _malloc(i1) {
 i1 = i1 | 0;
 var i2 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0, i24 = 0, i25 = 0, i26 = 0, i27 = 0, i28 = 0, i29 = 0, i30 = 0, i31 = 0, i32 = 0, i33 = 0, i34 = 0, i35 = 0, i36 = 0, i37 = 0, i38 = 0, i39 = 0, i40 = 0, i41 = 0, i42 = 0, i43 = 0, i44 = 0, i45 = 0, i46 = 0, i47 = 0, i48 = 0, i49 = 0, i50 = 0, i51 = 0, i52 = 0, i53 = 0, i54 = 0, i55 = 0, i56 = 0, i57 = 0, i58 = 0, i59 = 0, i60 = 0, i61 = 0, i62 = 0, i63 = 0, i64 = 0, i65 = 0, i66 = 0, i67 = 0, i68 = 0, i69 = 0, i70 = 0, i71 = 0, i72 = 0, i73 = 0, i74 = 0, i75 = 0, i76 = 0, i77 = 0, i78 = 0, i79 = 0, i80 = 0, i81 = 0, i82 = 0, i83 = 0, i84 = 0, i85 = 0, i86 = 0, i87 = 0, i88 = 0, i89 = 0, i90 = 0, i91 = 0, i92 = 0, i93 = 0;
 do if (i1 >>> 0 < 245) {
  i2 = i1 >>> 0 < 11 ? 16 : i1 + 11 & -8;
  i3 = HEAP32[2257] | 0;
  if (i3 >>> (i2 >>> 3) & 3) {
   i4 = (i3 >>> (i2 >>> 3) & 1 ^ 1) + (i2 >>> 3) << 1;
   i5 = HEAP32[9068 + (i4 + 2 << 2) >> 2] | 0;
   i6 = HEAP32[i5 + 8 >> 2] | 0;
   do if ((9068 + (i4 << 2) | 0) == (i6 | 0)) HEAP32[2257] = i3 & ~(1 << (i3 >>> (i2 >>> 3) & 1 ^ 1) + (i2 >>> 3)); else {
    if (i6 >>> 0 >= (HEAP32[2261] | 0) >>> 0 ? (HEAP32[i6 + 12 >> 2] | 0) == (i5 | 0) : 0) {
     HEAP32[i6 + 12 >> 2] = 9068 + (i4 << 2);
     HEAP32[9068 + (i4 + 2 << 2) >> 2] = i6;
     break;
    }
    _abort();
   } while (0);
   i6 = (i3 >>> (i2 >>> 3) & 1 ^ 1) + (i2 >>> 3) << 3;
   HEAP32[i5 + 4 >> 2] = i6 | 3;
   HEAP32[i5 + (i6 | 4) >> 2] = HEAP32[i5 + (i6 | 4) >> 2] | 1;
   i7 = i5 + 8 | 0;
   break;
  }
  i6 = HEAP32[2259] | 0;
  if (i2 >>> 0 > i6 >>> 0) {
   if (i3 >>> (i2 >>> 3)) {
    i4 = i3 >>> (i2 >>> 3) << (i2 >>> 3) & (2 << (i2 >>> 3) | 0 - (2 << (i2 >>> 3)));
    i8 = ((i4 & 0 - i4) + -1 | 0) >>> (((i4 & 0 - i4) + -1 | 0) >>> 12 & 16);
    i9 = i8 >>> (i8 >>> 5 & 8) >>> (i8 >>> (i8 >>> 5 & 8) >>> 2 & 4);
    i10 = (i8 >>> 5 & 8 | ((i4 & 0 - i4) + -1 | 0) >>> 12 & 16 | i8 >>> (i8 >>> 5 & 8) >>> 2 & 4 | i9 >>> 1 & 2 | i9 >>> (i9 >>> 1 & 2) >>> 1 & 1) + (i9 >>> (i9 >>> 1 & 2) >>> (i9 >>> (i9 >>> 1 & 2) >>> 1 & 1)) | 0;
    i9 = HEAP32[9068 + ((i10 << 1) + 2 << 2) >> 2] | 0;
    i8 = HEAP32[i9 + 8 >> 2] | 0;
    do if ((9068 + (i10 << 1 << 2) | 0) == (i8 | 0)) {
     HEAP32[2257] = i3 & ~(1 << i10);
     i11 = i6;
    } else {
     if (i8 >>> 0 >= (HEAP32[2261] | 0) >>> 0 ? (HEAP32[i8 + 12 >> 2] | 0) == (i9 | 0) : 0) {
      HEAP32[i8 + 12 >> 2] = 9068 + (i10 << 1 << 2);
      HEAP32[9068 + ((i10 << 1) + 2 << 2) >> 2] = i8;
      i11 = HEAP32[2259] | 0;
      break;
     }
     _abort();
    } while (0);
    HEAP32[i9 + 4 >> 2] = i2 | 3;
    HEAP32[i9 + (i2 | 4) >> 2] = (i10 << 3) - i2 | 1;
    HEAP32[i9 + (i10 << 3) >> 2] = (i10 << 3) - i2;
    if (i11) {
     i8 = HEAP32[2262] | 0;
     i6 = i11 >>> 3;
     i3 = HEAP32[2257] | 0;
     if (i3 & 1 << i6) {
      i5 = HEAP32[9068 + ((i6 << 1) + 2 << 2) >> 2] | 0;
      if (i5 >>> 0 < (HEAP32[2261] | 0) >>> 0) _abort(); else {
       i12 = 9068 + ((i6 << 1) + 2 << 2) | 0;
       i13 = i5;
      }
     } else {
      HEAP32[2257] = i3 | 1 << i6;
      i12 = 9068 + ((i6 << 1) + 2 << 2) | 0;
      i13 = 9068 + (i6 << 1 << 2) | 0;
     }
     HEAP32[i12 >> 2] = i8;
     HEAP32[i13 + 12 >> 2] = i8;
     HEAP32[i8 + 8 >> 2] = i13;
     HEAP32[i8 + 12 >> 2] = 9068 + (i6 << 1 << 2);
    }
    HEAP32[2259] = (i10 << 3) - i2;
    HEAP32[2262] = i9 + i2;
    i7 = i9 + 8 | 0;
    break;
   }
   i6 = HEAP32[2258] | 0;
   if (i6) {
    i8 = ((i6 & 0 - i6) + -1 | 0) >>> (((i6 & 0 - i6) + -1 | 0) >>> 12 & 16);
    i3 = i8 >>> (i8 >>> 5 & 8) >>> (i8 >>> (i8 >>> 5 & 8) >>> 2 & 4);
    i5 = HEAP32[9332 + ((i8 >>> 5 & 8 | ((i6 & 0 - i6) + -1 | 0) >>> 12 & 16 | i8 >>> (i8 >>> 5 & 8) >>> 2 & 4 | i3 >>> 1 & 2 | i3 >>> (i3 >>> 1 & 2) >>> 1 & 1) + (i3 >>> (i3 >>> 1 & 2) >>> (i3 >>> (i3 >>> 1 & 2) >>> 1 & 1)) << 2) >> 2] | 0;
    i3 = (HEAP32[i5 + 4 >> 2] & -8) - i2 | 0;
    i8 = i5;
    i6 = i5;
    while (1) {
     i5 = HEAP32[i8 + 16 >> 2] | 0;
     if (!i5) {
      i4 = HEAP32[i8 + 20 >> 2] | 0;
      if (!i4) {
       i14 = i3;
       i15 = i6;
       break;
      } else i16 = i4;
     } else i16 = i5;
     i5 = (HEAP32[i16 + 4 >> 2] & -8) - i2 | 0;
     i4 = i5 >>> 0 < i3 >>> 0;
     i3 = i4 ? i5 : i3;
     i8 = i16;
     i6 = i4 ? i16 : i6;
    }
    i6 = HEAP32[2261] | 0;
    if (i15 >>> 0 >= i6 >>> 0 ? i15 >>> 0 < (i15 + i2 | 0) >>> 0 : 0) {
     i8 = HEAP32[i15 + 24 >> 2] | 0;
     i3 = HEAP32[i15 + 12 >> 2] | 0;
     do if ((i3 | 0) == (i15 | 0)) {
      i9 = HEAP32[i15 + 20 >> 2] | 0;
      if (!i9) {
       i10 = HEAP32[i15 + 16 >> 2] | 0;
       if (!i10) {
        i17 = 0;
        break;
       } else {
        i18 = i10;
        i19 = i15 + 16 | 0;
       }
      } else {
       i18 = i9;
       i19 = i15 + 20 | 0;
      }
      while (1) {
       i9 = i18 + 20 | 0;
       i10 = HEAP32[i9 >> 2] | 0;
       if (i10) {
        i18 = i10;
        i19 = i9;
        continue;
       }
       i9 = i18 + 16 | 0;
       i10 = HEAP32[i9 >> 2] | 0;
       if (!i10) {
        i20 = i18;
        i21 = i19;
        break;
       } else {
        i18 = i10;
        i19 = i9;
       }
      }
      if (i21 >>> 0 < i6 >>> 0) _abort(); else {
       HEAP32[i21 >> 2] = 0;
       i17 = i20;
       break;
      }
     } else {
      i9 = HEAP32[i15 + 8 >> 2] | 0;
      if ((i9 >>> 0 >= i6 >>> 0 ? (HEAP32[i9 + 12 >> 2] | 0) == (i15 | 0) : 0) ? (HEAP32[i3 + 8 >> 2] | 0) == (i15 | 0) : 0) {
       HEAP32[i9 + 12 >> 2] = i3;
       HEAP32[i3 + 8 >> 2] = i9;
       i17 = i3;
       break;
      }
      _abort();
     } while (0);
     do if (i8) {
      i3 = HEAP32[i15 + 28 >> 2] | 0;
      if ((i15 | 0) == (HEAP32[9332 + (i3 << 2) >> 2] | 0)) {
       HEAP32[9332 + (i3 << 2) >> 2] = i17;
       if (!i17) {
        HEAP32[2258] = HEAP32[2258] & ~(1 << i3);
        break;
       }
      } else {
       if (i8 >>> 0 < (HEAP32[2261] | 0) >>> 0) _abort();
       if ((HEAP32[i8 + 16 >> 2] | 0) == (i15 | 0)) HEAP32[i8 + 16 >> 2] = i17; else HEAP32[i8 + 20 >> 2] = i17;
       if (!i17) break;
      }
      i3 = HEAP32[2261] | 0;
      if (i17 >>> 0 < i3 >>> 0) _abort();
      HEAP32[i17 + 24 >> 2] = i8;
      i6 = HEAP32[i15 + 16 >> 2] | 0;
      do if (i6) if (i6 >>> 0 < i3 >>> 0) _abort(); else {
       HEAP32[i17 + 16 >> 2] = i6;
       HEAP32[i6 + 24 >> 2] = i17;
       break;
      } while (0);
      i6 = HEAP32[i15 + 20 >> 2] | 0;
      if (i6) if (i6 >>> 0 < (HEAP32[2261] | 0) >>> 0) _abort(); else {
       HEAP32[i17 + 20 >> 2] = i6;
       HEAP32[i6 + 24 >> 2] = i17;
       break;
      }
     } while (0);
     if (i14 >>> 0 < 16) {
      HEAP32[i15 + 4 >> 2] = i14 + i2 | 3;
      HEAP32[i15 + (i14 + i2 + 4) >> 2] = HEAP32[i15 + (i14 + i2 + 4) >> 2] | 1;
     } else {
      HEAP32[i15 + 4 >> 2] = i2 | 3;
      HEAP32[i15 + (i2 | 4) >> 2] = i14 | 1;
      HEAP32[i15 + (i14 + i2) >> 2] = i14;
      i8 = HEAP32[2259] | 0;
      if (i8) {
       i6 = HEAP32[2262] | 0;
       i3 = HEAP32[2257] | 0;
       if (i3 & 1 << (i8 >>> 3)) {
        i9 = HEAP32[9068 + ((i8 >>> 3 << 1) + 2 << 2) >> 2] | 0;
        if (i9 >>> 0 < (HEAP32[2261] | 0) >>> 0) _abort(); else {
         i22 = 9068 + ((i8 >>> 3 << 1) + 2 << 2) | 0;
         i23 = i9;
        }
       } else {
        HEAP32[2257] = i3 | 1 << (i8 >>> 3);
        i22 = 9068 + ((i8 >>> 3 << 1) + 2 << 2) | 0;
        i23 = 9068 + (i8 >>> 3 << 1 << 2) | 0;
       }
       HEAP32[i22 >> 2] = i6;
       HEAP32[i23 + 12 >> 2] = i6;
       HEAP32[i6 + 8 >> 2] = i23;
       HEAP32[i6 + 12 >> 2] = 9068 + (i8 >>> 3 << 1 << 2);
      }
      HEAP32[2259] = i14;
      HEAP32[2262] = i15 + i2;
     }
     i7 = i15 + 8 | 0;
     break;
    }
    _abort();
   } else {
    i24 = i2;
    i25 = 154;
   }
  } else {
   i24 = i2;
   i25 = 154;
  }
 } else if (i1 >>> 0 <= 4294967231) {
  i8 = i1 + 11 & -8;
  i6 = HEAP32[2258] | 0;
  if (i6) {
   if ((i1 + 11 | 0) >>> 8) if (i8 >>> 0 > 16777215) i26 = 31; else {
    i3 = (i1 + 11 | 0) >>> 8 << ((((i1 + 11 | 0) >>> 8) + 1048320 | 0) >>> 16 & 8);
    i9 = 14 - ((i3 + 520192 | 0) >>> 16 & 4 | (((i1 + 11 | 0) >>> 8) + 1048320 | 0) >>> 16 & 8 | ((i3 << ((i3 + 520192 | 0) >>> 16 & 4)) + 245760 | 0) >>> 16 & 2) + (i3 << ((i3 + 520192 | 0) >>> 16 & 4) << (((i3 << ((i3 + 520192 | 0) >>> 16 & 4)) + 245760 | 0) >>> 16 & 2) >>> 15) | 0;
    i26 = i8 >>> (i9 + 7 | 0) & 1 | i9 << 1;
   } else i26 = 0;
   i9 = HEAP32[9332 + (i26 << 2) >> 2] | 0;
   L110 : do if (!i9) {
    i27 = 0 - i8 | 0;
    i28 = 0;
    i29 = 0;
    i25 = 86;
   } else {
    i3 = 0 - i8 | 0;
    i10 = 0;
    i4 = i8 << ((i26 | 0) == 31 ? 0 : 25 - (i26 >>> 1) | 0);
    i5 = i9;
    i30 = 0;
    while (1) {
     i31 = HEAP32[i5 + 4 >> 2] & -8;
     if ((i31 - i8 | 0) >>> 0 < i3 >>> 0) if ((i31 | 0) == (i8 | 0)) {
      i32 = i31 - i8 | 0;
      i33 = i5;
      i34 = i5;
      i25 = 90;
      break L110;
     } else {
      i35 = i31 - i8 | 0;
      i36 = i5;
     } else {
      i35 = i3;
      i36 = i30;
     }
     i31 = HEAP32[i5 + 20 >> 2] | 0;
     i5 = HEAP32[i5 + 16 + (i4 >>> 31 << 2) >> 2] | 0;
     i37 = (i31 | 0) == 0 | (i31 | 0) == (i5 | 0) ? i10 : i31;
     if (!i5) {
      i27 = i35;
      i28 = i37;
      i29 = i36;
      i25 = 86;
      break;
     } else {
      i3 = i35;
      i10 = i37;
      i4 = i4 << 1;
      i30 = i36;
     }
    }
   } while (0);
   if ((i25 | 0) == 86) {
    if ((i28 | 0) == 0 & (i29 | 0) == 0) {
     i9 = 2 << i26;
     if (!(i6 & (i9 | 0 - i9))) {
      i24 = i8;
      i25 = 154;
      break;
     }
     i2 = (i6 & (i9 | 0 - i9) & 0 - (i6 & (i9 | 0 - i9))) + -1 | 0;
     i9 = i2 >>> (i2 >>> 12 & 16) >>> (i2 >>> (i2 >>> 12 & 16) >>> 5 & 8);
     i30 = i9 >>> (i9 >>> 2 & 4) >>> (i9 >>> (i9 >>> 2 & 4) >>> 1 & 2);
     i38 = HEAP32[9332 + ((i2 >>> (i2 >>> 12 & 16) >>> 5 & 8 | i2 >>> 12 & 16 | i9 >>> 2 & 4 | i9 >>> (i9 >>> 2 & 4) >>> 1 & 2 | i30 >>> 1 & 1) + (i30 >>> (i30 >>> 1 & 1)) << 2) >> 2] | 0;
     i39 = 0;
    } else {
     i38 = i28;
     i39 = i29;
    }
    if (!i38) {
     i40 = i27;
     i41 = i39;
    } else {
     i32 = i27;
     i33 = i38;
     i34 = i39;
     i25 = 90;
    }
   }
   if ((i25 | 0) == 90) while (1) {
    i25 = 0;
    i30 = (HEAP32[i33 + 4 >> 2] & -8) - i8 | 0;
    i9 = i30 >>> 0 < i32 >>> 0;
    i2 = i9 ? i30 : i32;
    i30 = i9 ? i33 : i34;
    i9 = HEAP32[i33 + 16 >> 2] | 0;
    if (i9) {
     i32 = i2;
     i33 = i9;
     i34 = i30;
     i25 = 90;
     continue;
    }
    i33 = HEAP32[i33 + 20 >> 2] | 0;
    if (!i33) {
     i40 = i2;
     i41 = i30;
     break;
    } else {
     i32 = i2;
     i34 = i30;
     i25 = 90;
    }
   }
   if ((i41 | 0) != 0 ? i40 >>> 0 < ((HEAP32[2259] | 0) - i8 | 0) >>> 0 : 0) {
    i6 = HEAP32[2261] | 0;
    if (i41 >>> 0 >= i6 >>> 0 ? (i30 = i41 + i8 | 0, i41 >>> 0 < i30 >>> 0) : 0) {
     i2 = HEAP32[i41 + 24 >> 2] | 0;
     i9 = HEAP32[i41 + 12 >> 2] | 0;
     do if ((i9 | 0) == (i41 | 0)) {
      i4 = i41 + 20 | 0;
      i10 = HEAP32[i4 >> 2] | 0;
      if (!i10) {
       i3 = i41 + 16 | 0;
       i5 = HEAP32[i3 >> 2] | 0;
       if (!i5) {
        i42 = 0;
        break;
       } else {
        i43 = i5;
        i44 = i3;
       }
      } else {
       i43 = i10;
       i44 = i4;
      }
      while (1) {
       i4 = i43 + 20 | 0;
       i10 = HEAP32[i4 >> 2] | 0;
       if (i10) {
        i43 = i10;
        i44 = i4;
        continue;
       }
       i4 = i43 + 16 | 0;
       i10 = HEAP32[i4 >> 2] | 0;
       if (!i10) {
        i45 = i43;
        i46 = i44;
        break;
       } else {
        i43 = i10;
        i44 = i4;
       }
      }
      if (i46 >>> 0 < i6 >>> 0) _abort(); else {
       HEAP32[i46 >> 2] = 0;
       i42 = i45;
       break;
      }
     } else {
      i4 = HEAP32[i41 + 8 >> 2] | 0;
      if ((i4 >>> 0 >= i6 >>> 0 ? (HEAP32[i4 + 12 >> 2] | 0) == (i41 | 0) : 0) ? (HEAP32[i9 + 8 >> 2] | 0) == (i41 | 0) : 0) {
       HEAP32[i4 + 12 >> 2] = i9;
       HEAP32[i9 + 8 >> 2] = i4;
       i42 = i9;
       break;
      }
      _abort();
     } while (0);
     do if (i2) {
      i9 = HEAP32[i41 + 28 >> 2] | 0;
      if ((i41 | 0) == (HEAP32[9332 + (i9 << 2) >> 2] | 0)) {
       HEAP32[9332 + (i9 << 2) >> 2] = i42;
       if (!i42) {
        HEAP32[2258] = HEAP32[2258] & ~(1 << i9);
        break;
       }
      } else {
       if (i2 >>> 0 < (HEAP32[2261] | 0) >>> 0) _abort();
       if ((HEAP32[i2 + 16 >> 2] | 0) == (i41 | 0)) HEAP32[i2 + 16 >> 2] = i42; else HEAP32[i2 + 20 >> 2] = i42;
       if (!i42) break;
      }
      i9 = HEAP32[2261] | 0;
      if (i42 >>> 0 < i9 >>> 0) _abort();
      HEAP32[i42 + 24 >> 2] = i2;
      i6 = HEAP32[i41 + 16 >> 2] | 0;
      do if (i6) if (i6 >>> 0 < i9 >>> 0) _abort(); else {
       HEAP32[i42 + 16 >> 2] = i6;
       HEAP32[i6 + 24 >> 2] = i42;
       break;
      } while (0);
      i6 = HEAP32[i41 + 20 >> 2] | 0;
      if (i6) if (i6 >>> 0 < (HEAP32[2261] | 0) >>> 0) _abort(); else {
       HEAP32[i42 + 20 >> 2] = i6;
       HEAP32[i6 + 24 >> 2] = i42;
       break;
      }
     } while (0);
     L179 : do if (i40 >>> 0 >= 16) {
      HEAP32[i41 + 4 >> 2] = i8 | 3;
      HEAP32[i41 + (i8 | 4) >> 2] = i40 | 1;
      HEAP32[i41 + (i40 + i8) >> 2] = i40;
      i2 = i40 >>> 3;
      if (i40 >>> 0 < 256) {
       i6 = HEAP32[2257] | 0;
       if (i6 & 1 << i2) {
        i9 = HEAP32[9068 + ((i2 << 1) + 2 << 2) >> 2] | 0;
        if (i9 >>> 0 < (HEAP32[2261] | 0) >>> 0) _abort(); else {
         i47 = 9068 + ((i2 << 1) + 2 << 2) | 0;
         i48 = i9;
        }
       } else {
        HEAP32[2257] = i6 | 1 << i2;
        i47 = 9068 + ((i2 << 1) + 2 << 2) | 0;
        i48 = 9068 + (i2 << 1 << 2) | 0;
       }
       HEAP32[i47 >> 2] = i30;
       HEAP32[i48 + 12 >> 2] = i30;
       HEAP32[i41 + (i8 + 8) >> 2] = i48;
       HEAP32[i41 + (i8 + 12) >> 2] = 9068 + (i2 << 1 << 2);
       break;
      }
      i2 = i40 >>> 8;
      if (i2) if (i40 >>> 0 > 16777215) i49 = 31; else {
       i6 = i2 << ((i2 + 1048320 | 0) >>> 16 & 8) << (((i2 << ((i2 + 1048320 | 0) >>> 16 & 8)) + 520192 | 0) >>> 16 & 4);
       i9 = 14 - (((i2 << ((i2 + 1048320 | 0) >>> 16 & 8)) + 520192 | 0) >>> 16 & 4 | (i2 + 1048320 | 0) >>> 16 & 8 | (i6 + 245760 | 0) >>> 16 & 2) + (i6 << ((i6 + 245760 | 0) >>> 16 & 2) >>> 15) | 0;
       i49 = i40 >>> (i9 + 7 | 0) & 1 | i9 << 1;
      } else i49 = 0;
      i9 = 9332 + (i49 << 2) | 0;
      HEAP32[i41 + (i8 + 28) >> 2] = i49;
      HEAP32[i41 + (i8 + 20) >> 2] = 0;
      HEAP32[i41 + (i8 + 16) >> 2] = 0;
      i6 = HEAP32[2258] | 0;
      i2 = 1 << i49;
      if (!(i6 & i2)) {
       HEAP32[2258] = i6 | i2;
       HEAP32[i9 >> 2] = i30;
       HEAP32[i41 + (i8 + 24) >> 2] = i9;
       HEAP32[i41 + (i8 + 12) >> 2] = i30;
       HEAP32[i41 + (i8 + 8) >> 2] = i30;
       break;
      }
      i2 = HEAP32[i9 >> 2] | 0;
      L197 : do if ((HEAP32[i2 + 4 >> 2] & -8 | 0) != (i40 | 0)) {
       i9 = i40 << ((i49 | 0) == 31 ? 0 : 25 - (i49 >>> 1) | 0);
       i6 = i2;
       while (1) {
        i50 = i6 + 16 + (i9 >>> 31 << 2) | 0;
        i4 = HEAP32[i50 >> 2] | 0;
        if (!i4) {
         i51 = i6;
         break;
        }
        if ((HEAP32[i4 + 4 >> 2] & -8 | 0) == (i40 | 0)) {
         i52 = i4;
         break L197;
        } else {
         i9 = i9 << 1;
         i6 = i4;
        }
       }
       if (i50 >>> 0 < (HEAP32[2261] | 0) >>> 0) _abort(); else {
        HEAP32[i50 >> 2] = i30;
        HEAP32[i41 + (i8 + 24) >> 2] = i51;
        HEAP32[i41 + (i8 + 12) >> 2] = i30;
        HEAP32[i41 + (i8 + 8) >> 2] = i30;
        break L179;
       }
      } else i52 = i2; while (0);
      i2 = i52 + 8 | 0;
      i6 = HEAP32[i2 >> 2] | 0;
      i9 = HEAP32[2261] | 0;
      if (i6 >>> 0 >= i9 >>> 0 & i52 >>> 0 >= i9 >>> 0) {
       HEAP32[i6 + 12 >> 2] = i30;
       HEAP32[i2 >> 2] = i30;
       HEAP32[i41 + (i8 + 8) >> 2] = i6;
       HEAP32[i41 + (i8 + 12) >> 2] = i52;
       HEAP32[i41 + (i8 + 24) >> 2] = 0;
       break;
      } else _abort();
     } else {
      i6 = i40 + i8 | 0;
      HEAP32[i41 + 4 >> 2] = i6 | 3;
      i2 = i41 + (i6 + 4) | 0;
      HEAP32[i2 >> 2] = HEAP32[i2 >> 2] | 1;
     } while (0);
     i7 = i41 + 8 | 0;
     break;
    }
    _abort();
   } else {
    i24 = i8;
    i25 = 154;
   }
  } else {
   i24 = i8;
   i25 = 154;
  }
 } else {
  i24 = -1;
  i25 = 154;
 } while (0);
 L212 : do if ((i25 | 0) == 154) {
  i41 = HEAP32[2259] | 0;
  if (i41 >>> 0 >= i24 >>> 0) {
   i40 = i41 - i24 | 0;
   i52 = HEAP32[2262] | 0;
   if (i40 >>> 0 > 15) {
    HEAP32[2262] = i52 + i24;
    HEAP32[2259] = i40;
    HEAP32[i52 + (i24 + 4) >> 2] = i40 | 1;
    HEAP32[i52 + i41 >> 2] = i40;
    HEAP32[i52 + 4 >> 2] = i24 | 3;
   } else {
    HEAP32[2259] = 0;
    HEAP32[2262] = 0;
    HEAP32[i52 + 4 >> 2] = i41 | 3;
    HEAP32[i52 + (i41 + 4) >> 2] = HEAP32[i52 + (i41 + 4) >> 2] | 1;
   }
   i7 = i52 + 8 | 0;
   break;
  }
  i52 = HEAP32[2260] | 0;
  if (i52 >>> 0 > i24 >>> 0) {
   i41 = i52 - i24 | 0;
   HEAP32[2260] = i41;
   i52 = HEAP32[2263] | 0;
   HEAP32[2263] = i52 + i24;
   HEAP32[i52 + (i24 + 4) >> 2] = i41 | 1;
   HEAP32[i52 + 4 >> 2] = i24 | 3;
   i7 = i52 + 8 | 0;
   break;
  }
  if (!(HEAP32[2375] | 0)) _init_mparams();
  i52 = i24 + 48 | 0;
  i41 = HEAP32[2377] | 0;
  i40 = i24 + 47 | 0;
  i51 = i41 + i40 & 0 - i41;
  if (i51 >>> 0 > i24 >>> 0) {
   i50 = HEAP32[2367] | 0;
   if ((i50 | 0) != 0 ? (i49 = HEAP32[2365] | 0, (i49 + i51 | 0) >>> 0 <= i49 >>> 0 | (i49 + i51 | 0) >>> 0 > i50 >>> 0) : 0) {
    i7 = 0;
    break;
   }
   L231 : do if (!(HEAP32[2368] & 4)) {
    i50 = HEAP32[2263] | 0;
    L233 : do if (i50) {
     i49 = 9476;
     while (1) {
      i48 = HEAP32[i49 >> 2] | 0;
      if (i48 >>> 0 <= i50 >>> 0 ? (i53 = i49 + 4 | 0, (i48 + (HEAP32[i53 >> 2] | 0) | 0) >>> 0 > i50 >>> 0) : 0) {
       i54 = i49;
       break;
      }
      i49 = HEAP32[i49 + 8 >> 2] | 0;
      if (!i49) {
       i25 = 172;
       break L233;
      }
     }
     i49 = i41 + i40 - (HEAP32[2260] | 0) & 0 - i41;
     if (i49 >>> 0 < 2147483647) {
      i48 = _sbrk(i49 | 0) | 0;
      i47 = (i48 | 0) == ((HEAP32[i54 >> 2] | 0) + (HEAP32[i53 >> 2] | 0) | 0);
      i42 = i47 ? i49 : 0;
      if (i47) if ((i48 | 0) == (-1 | 0)) i55 = i42; else {
       i56 = i48;
       i57 = i42;
       i25 = 192;
       break L231;
      } else {
       i58 = i48;
       i59 = i49;
       i60 = i42;
       i25 = 182;
      }
     } else i55 = 0;
    } else i25 = 172; while (0);
    do if ((i25 | 0) == 172) {
     i50 = _sbrk(0) | 0;
     if ((i50 | 0) != (-1 | 0)) {
      i42 = HEAP32[2376] | 0;
      if (!(i42 + -1 & i50)) i61 = i51; else i61 = i51 - i50 + (i42 + -1 + i50 & 0 - i42) | 0;
      i42 = HEAP32[2365] | 0;
      i49 = i42 + i61 | 0;
      if (i61 >>> 0 > i24 >>> 0 & i61 >>> 0 < 2147483647) {
       i48 = HEAP32[2367] | 0;
       if ((i48 | 0) != 0 ? i49 >>> 0 <= i42 >>> 0 | i49 >>> 0 > i48 >>> 0 : 0) {
        i55 = 0;
        break;
       }
       i48 = _sbrk(i61 | 0) | 0;
       i49 = (i48 | 0) == (i50 | 0) ? i61 : 0;
       if ((i48 | 0) == (i50 | 0)) {
        i56 = i50;
        i57 = i49;
        i25 = 192;
        break L231;
       } else {
        i58 = i48;
        i59 = i61;
        i60 = i49;
        i25 = 182;
       }
      } else i55 = 0;
     } else i55 = 0;
    } while (0);
    L253 : do if ((i25 | 0) == 182) {
     i49 = 0 - i59 | 0;
     do if (i52 >>> 0 > i59 >>> 0 & (i59 >>> 0 < 2147483647 & (i58 | 0) != (-1 | 0)) ? (i48 = HEAP32[2377] | 0, i50 = i40 - i59 + i48 & 0 - i48, i50 >>> 0 < 2147483647) : 0) if ((_sbrk(i50 | 0) | 0) == (-1 | 0)) {
      _sbrk(i49 | 0) | 0;
      i55 = i60;
      break L253;
     } else {
      i62 = i50 + i59 | 0;
      break;
     } else i62 = i59; while (0);
     if ((i58 | 0) == (-1 | 0)) i55 = i60; else {
      i56 = i58;
      i57 = i62;
      i25 = 192;
      break L231;
     }
    } while (0);
    HEAP32[2368] = HEAP32[2368] | 4;
    i63 = i55;
    i25 = 189;
   } else {
    i63 = 0;
    i25 = 189;
   } while (0);
   if ((((i25 | 0) == 189 ? i51 >>> 0 < 2147483647 : 0) ? (i40 = _sbrk(i51 | 0) | 0, i52 = _sbrk(0) | 0, i40 >>> 0 < i52 >>> 0 & ((i40 | 0) != (-1 | 0) & (i52 | 0) != (-1 | 0))) : 0) ? (i41 = (i52 - i40 | 0) >>> 0 > (i24 + 40 | 0) >>> 0, i41) : 0) {
    i56 = i40;
    i57 = i41 ? i52 - i40 | 0 : i63;
    i25 = 192;
   }
   if ((i25 | 0) == 192) {
    i40 = (HEAP32[2365] | 0) + i57 | 0;
    HEAP32[2365] = i40;
    if (i40 >>> 0 > (HEAP32[2366] | 0) >>> 0) HEAP32[2366] = i40;
    i40 = HEAP32[2263] | 0;
    L272 : do if (i40) {
     i52 = 9476;
     do {
      i64 = HEAP32[i52 >> 2] | 0;
      i65 = i52 + 4 | 0;
      i66 = HEAP32[i65 >> 2] | 0;
      if ((i56 | 0) == (i64 + i66 | 0)) {
       i67 = i52;
       i25 = 202;
       break;
      }
      i52 = HEAP32[i52 + 8 >> 2] | 0;
     } while ((i52 | 0) != 0);
     if (((i25 | 0) == 202 ? (HEAP32[i67 + 12 >> 2] & 8 | 0) == 0 : 0) ? i40 >>> 0 < i56 >>> 0 & i40 >>> 0 >= i64 >>> 0 : 0) {
      HEAP32[i65 >> 2] = i66 + i57;
      i52 = (HEAP32[2260] | 0) + i57 | 0;
      i41 = (i40 + 8 & 7 | 0) == 0 ? 0 : 0 - (i40 + 8) & 7;
      HEAP32[2263] = i40 + i41;
      HEAP32[2260] = i52 - i41;
      HEAP32[i40 + (i41 + 4) >> 2] = i52 - i41 | 1;
      HEAP32[i40 + (i52 + 4) >> 2] = 40;
      HEAP32[2264] = HEAP32[2379];
      break;
     }
     i52 = HEAP32[2261] | 0;
     if (i56 >>> 0 < i52 >>> 0) {
      HEAP32[2261] = i56;
      i68 = i56;
     } else i68 = i52;
     i52 = i56 + i57 | 0;
     i41 = 9476;
     while (1) {
      if ((HEAP32[i41 >> 2] | 0) == (i52 | 0)) {
       i69 = i41;
       i70 = i41;
       i25 = 210;
       break;
      }
      i41 = HEAP32[i41 + 8 >> 2] | 0;
      if (!i41) {
       i71 = 9476;
       break;
      }
     }
     if ((i25 | 0) == 210) if (!(HEAP32[i70 + 12 >> 2] & 8)) {
      HEAP32[i69 >> 2] = i56;
      HEAP32[i70 + 4 >> 2] = (HEAP32[i70 + 4 >> 2] | 0) + i57;
      i41 = i56 + 8 | 0;
      i52 = (i41 & 7 | 0) == 0 ? 0 : 0 - i41 & 7;
      i41 = i56 + (i57 + 8) | 0;
      i8 = (i41 & 7 | 0) == 0 ? 0 : 0 - i41 & 7;
      i41 = i56 + (i8 + i57) | 0;
      i49 = i52 + i24 | 0;
      i50 = i56 + i49 | 0;
      i48 = i41 - (i56 + i52) - i24 | 0;
      HEAP32[i56 + (i52 + 4) >> 2] = i24 | 3;
      L297 : do if ((i41 | 0) != (i40 | 0)) {
       if ((i41 | 0) == (HEAP32[2262] | 0)) {
        i42 = (HEAP32[2259] | 0) + i48 | 0;
        HEAP32[2259] = i42;
        HEAP32[2262] = i50;
        HEAP32[i56 + (i49 + 4) >> 2] = i42 | 1;
        HEAP32[i56 + (i42 + i49) >> 2] = i42;
        break;
       }
       i42 = i57 + 4 | 0;
       i47 = HEAP32[i56 + (i42 + i8) >> 2] | 0;
       if ((i47 & 3 | 0) == 1) {
        L305 : do if (i47 >>> 0 >= 256) {
         i45 = HEAP32[i56 + ((i8 | 24) + i57) >> 2] | 0;
         i46 = HEAP32[i56 + (i57 + 12 + i8) >> 2] | 0;
         L324 : do if ((i46 | 0) == (i41 | 0)) {
          i44 = i56 + (i42 + (i8 | 16)) | 0;
          i43 = HEAP32[i44 >> 2] | 0;
          if (!i43) {
           i34 = i56 + ((i8 | 16) + i57) | 0;
           i32 = HEAP32[i34 >> 2] | 0;
           if (!i32) {
            i72 = 0;
            break;
           } else {
            i73 = i32;
            i74 = i34;
           }
          } else {
           i73 = i43;
           i74 = i44;
          }
          while (1) {
           i44 = i73 + 20 | 0;
           i43 = HEAP32[i44 >> 2] | 0;
           if (i43) {
            i73 = i43;
            i74 = i44;
            continue;
           }
           i44 = i73 + 16 | 0;
           i43 = HEAP32[i44 >> 2] | 0;
           if (!i43) {
            i75 = i73;
            i76 = i74;
            break;
           } else {
            i73 = i43;
            i74 = i44;
           }
          }
          if (i76 >>> 0 < i68 >>> 0) _abort(); else {
           HEAP32[i76 >> 2] = 0;
           i72 = i75;
           break;
          }
         } else {
          i44 = HEAP32[i56 + ((i8 | 8) + i57) >> 2] | 0;
          do if (i44 >>> 0 >= i68 >>> 0) {
           if ((HEAP32[i44 + 12 >> 2] | 0) != (i41 | 0)) break;
           if ((HEAP32[i46 + 8 >> 2] | 0) != (i41 | 0)) break;
           HEAP32[i44 + 12 >> 2] = i46;
           HEAP32[i46 + 8 >> 2] = i44;
           i72 = i46;
           break L324;
          } while (0);
          _abort();
         } while (0);
         if (!i45) break;
         i46 = HEAP32[i56 + (i57 + 28 + i8) >> 2] | 0;
         do if ((i41 | 0) != (HEAP32[9332 + (i46 << 2) >> 2] | 0)) {
          if (i45 >>> 0 < (HEAP32[2261] | 0) >>> 0) _abort();
          if ((HEAP32[i45 + 16 >> 2] | 0) == (i41 | 0)) HEAP32[i45 + 16 >> 2] = i72; else HEAP32[i45 + 20 >> 2] = i72;
          if (!i72) break L305;
         } else {
          HEAP32[9332 + (i46 << 2) >> 2] = i72;
          if (i72) break;
          HEAP32[2258] = HEAP32[2258] & ~(1 << i46);
          break L305;
         } while (0);
         i46 = HEAP32[2261] | 0;
         if (i72 >>> 0 < i46 >>> 0) _abort();
         HEAP32[i72 + 24 >> 2] = i45;
         i44 = HEAP32[i56 + ((i8 | 16) + i57) >> 2] | 0;
         do if (i44) if (i44 >>> 0 < i46 >>> 0) _abort(); else {
          HEAP32[i72 + 16 >> 2] = i44;
          HEAP32[i44 + 24 >> 2] = i72;
          break;
         } while (0);
         i44 = HEAP32[i56 + (i42 + (i8 | 16)) >> 2] | 0;
         if (!i44) break;
         if (i44 >>> 0 < (HEAP32[2261] | 0) >>> 0) _abort(); else {
          HEAP32[i72 + 20 >> 2] = i44;
          HEAP32[i44 + 24 >> 2] = i72;
          break;
         }
        } else {
         i44 = HEAP32[i56 + ((i8 | 8) + i57) >> 2] | 0;
         i46 = HEAP32[i56 + (i57 + 12 + i8) >> 2] | 0;
         do if ((i44 | 0) != (9068 + (i47 >>> 3 << 1 << 2) | 0)) {
          if (i44 >>> 0 >= i68 >>> 0 ? (HEAP32[i44 + 12 >> 2] | 0) == (i41 | 0) : 0) break;
          _abort();
         } while (0);
         if ((i46 | 0) == (i44 | 0)) {
          HEAP32[2257] = HEAP32[2257] & ~(1 << (i47 >>> 3));
          break;
         }
         do if ((i46 | 0) == (9068 + (i47 >>> 3 << 1 << 2) | 0)) i77 = i46 + 8 | 0; else {
          if (i46 >>> 0 >= i68 >>> 0 ? (HEAP32[i46 + 8 >> 2] | 0) == (i41 | 0) : 0) {
           i77 = i46 + 8 | 0;
           break;
          }
          _abort();
         } while (0);
         HEAP32[i44 + 12 >> 2] = i46;
         HEAP32[i77 >> 2] = i44;
        } while (0);
        i78 = i56 + ((i47 & -8 | i8) + i57) | 0;
        i79 = (i47 & -8) + i48 | 0;
       } else {
        i78 = i41;
        i79 = i48;
       }
       i42 = i78 + 4 | 0;
       HEAP32[i42 >> 2] = HEAP32[i42 >> 2] & -2;
       HEAP32[i56 + (i49 + 4) >> 2] = i79 | 1;
       HEAP32[i56 + (i79 + i49) >> 2] = i79;
       i42 = i79 >>> 3;
       if (i79 >>> 0 < 256) {
        i45 = HEAP32[2257] | 0;
        do if (!(i45 & 1 << i42)) {
         HEAP32[2257] = i45 | 1 << i42;
         i80 = 9068 + ((i42 << 1) + 2 << 2) | 0;
         i81 = 9068 + (i42 << 1 << 2) | 0;
        } else {
         i43 = HEAP32[9068 + ((i42 << 1) + 2 << 2) >> 2] | 0;
         if (i43 >>> 0 >= (HEAP32[2261] | 0) >>> 0) {
          i80 = 9068 + ((i42 << 1) + 2 << 2) | 0;
          i81 = i43;
          break;
         }
         _abort();
        } while (0);
        HEAP32[i80 >> 2] = i50;
        HEAP32[i81 + 12 >> 2] = i50;
        HEAP32[i56 + (i49 + 8) >> 2] = i81;
        HEAP32[i56 + (i49 + 12) >> 2] = 9068 + (i42 << 1 << 2);
        break;
       }
       i45 = i79 >>> 8;
       do if (!i45) i82 = 0; else {
        if (i79 >>> 0 > 16777215) {
         i82 = 31;
         break;
        }
        i47 = i45 << ((i45 + 1048320 | 0) >>> 16 & 8) << (((i45 << ((i45 + 1048320 | 0) >>> 16 & 8)) + 520192 | 0) >>> 16 & 4);
        i43 = 14 - (((i45 << ((i45 + 1048320 | 0) >>> 16 & 8)) + 520192 | 0) >>> 16 & 4 | (i45 + 1048320 | 0) >>> 16 & 8 | (i47 + 245760 | 0) >>> 16 & 2) + (i47 << ((i47 + 245760 | 0) >>> 16 & 2) >>> 15) | 0;
        i82 = i79 >>> (i43 + 7 | 0) & 1 | i43 << 1;
       } while (0);
       i45 = 9332 + (i82 << 2) | 0;
       HEAP32[i56 + (i49 + 28) >> 2] = i82;
       HEAP32[i56 + (i49 + 20) >> 2] = 0;
       HEAP32[i56 + (i49 + 16) >> 2] = 0;
       i42 = HEAP32[2258] | 0;
       i43 = 1 << i82;
       if (!(i42 & i43)) {
        HEAP32[2258] = i42 | i43;
        HEAP32[i45 >> 2] = i50;
        HEAP32[i56 + (i49 + 24) >> 2] = i45;
        HEAP32[i56 + (i49 + 12) >> 2] = i50;
        HEAP32[i56 + (i49 + 8) >> 2] = i50;
        break;
       }
       i43 = HEAP32[i45 >> 2] | 0;
       L385 : do if ((HEAP32[i43 + 4 >> 2] & -8 | 0) != (i79 | 0)) {
        i45 = i79 << ((i82 | 0) == 31 ? 0 : 25 - (i82 >>> 1) | 0);
        i42 = i43;
        while (1) {
         i83 = i42 + 16 + (i45 >>> 31 << 2) | 0;
         i47 = HEAP32[i83 >> 2] | 0;
         if (!i47) {
          i84 = i42;
          break;
         }
         if ((HEAP32[i47 + 4 >> 2] & -8 | 0) == (i79 | 0)) {
          i85 = i47;
          break L385;
         } else {
          i45 = i45 << 1;
          i42 = i47;
         }
        }
        if (i83 >>> 0 < (HEAP32[2261] | 0) >>> 0) _abort(); else {
         HEAP32[i83 >> 2] = i50;
         HEAP32[i56 + (i49 + 24) >> 2] = i84;
         HEAP32[i56 + (i49 + 12) >> 2] = i50;
         HEAP32[i56 + (i49 + 8) >> 2] = i50;
         break L297;
        }
       } else i85 = i43; while (0);
       i43 = i85 + 8 | 0;
       i42 = HEAP32[i43 >> 2] | 0;
       i45 = HEAP32[2261] | 0;
       if (i42 >>> 0 >= i45 >>> 0 & i85 >>> 0 >= i45 >>> 0) {
        HEAP32[i42 + 12 >> 2] = i50;
        HEAP32[i43 >> 2] = i50;
        HEAP32[i56 + (i49 + 8) >> 2] = i42;
        HEAP32[i56 + (i49 + 12) >> 2] = i85;
        HEAP32[i56 + (i49 + 24) >> 2] = 0;
        break;
       } else _abort();
      } else {
       i42 = (HEAP32[2260] | 0) + i48 | 0;
       HEAP32[2260] = i42;
       HEAP32[2263] = i50;
       HEAP32[i56 + (i49 + 4) >> 2] = i42 | 1;
      } while (0);
      i7 = i56 + (i52 | 8) | 0;
      break L212;
     } else i71 = 9476;
     while (1) {
      i86 = HEAP32[i71 >> 2] | 0;
      if (i86 >>> 0 <= i40 >>> 0 ? (i87 = HEAP32[i71 + 4 >> 2] | 0, (i86 + i87 | 0) >>> 0 > i40 >>> 0) : 0) break;
      i71 = HEAP32[i71 + 8 >> 2] | 0;
     }
     i52 = i86 + (i87 + -47 + ((i86 + (i87 + -39) & 7 | 0) == 0 ? 0 : 0 - (i86 + (i87 + -39)) & 7)) | 0;
     i49 = i52 >>> 0 < (i40 + 16 | 0) >>> 0 ? i40 : i52;
     i52 = i56 + 8 | 0;
     i50 = (i52 & 7 | 0) == 0 ? 0 : 0 - i52 & 7;
     i52 = i57 + -40 - i50 | 0;
     HEAP32[2263] = i56 + i50;
     HEAP32[2260] = i52;
     HEAP32[i56 + (i50 + 4) >> 2] = i52 | 1;
     HEAP32[i56 + (i57 + -36) >> 2] = 40;
     HEAP32[2264] = HEAP32[2379];
     HEAP32[i49 + 4 >> 2] = 27;
     HEAP32[i49 + 8 >> 2] = HEAP32[2369];
     HEAP32[i49 + 8 + 4 >> 2] = HEAP32[2370];
     HEAP32[i49 + 8 + 8 >> 2] = HEAP32[2371];
     HEAP32[i49 + 8 + 12 >> 2] = HEAP32[2372];
     HEAP32[2369] = i56;
     HEAP32[2370] = i57;
     HEAP32[2372] = 0;
     HEAP32[2371] = i49 + 8;
     HEAP32[i49 + 28 >> 2] = 7;
     if ((i49 + 32 | 0) >>> 0 < (i86 + i87 | 0) >>> 0) {
      i52 = i49 + 28 | 0;
      do {
       i50 = i52;
       i52 = i52 + 4 | 0;
       HEAP32[i52 >> 2] = 7;
      } while ((i50 + 8 | 0) >>> 0 < (i86 + i87 | 0) >>> 0);
     }
     if ((i49 | 0) != (i40 | 0)) {
      HEAP32[i49 + 4 >> 2] = HEAP32[i49 + 4 >> 2] & -2;
      HEAP32[i40 + 4 >> 2] = i49 - i40 | 1;
      HEAP32[i49 >> 2] = i49 - i40;
      if ((i49 - i40 | 0) >>> 0 < 256) {
       i52 = HEAP32[2257] | 0;
       if (i52 & 1 << ((i49 - i40 | 0) >>> 3)) {
        i50 = HEAP32[9068 + (((i49 - i40 | 0) >>> 3 << 1) + 2 << 2) >> 2] | 0;
        if (i50 >>> 0 < (HEAP32[2261] | 0) >>> 0) _abort(); else {
         i88 = 9068 + (((i49 - i40 | 0) >>> 3 << 1) + 2 << 2) | 0;
         i89 = i50;
        }
       } else {
        HEAP32[2257] = i52 | 1 << ((i49 - i40 | 0) >>> 3);
        i88 = 9068 + (((i49 - i40 | 0) >>> 3 << 1) + 2 << 2) | 0;
        i89 = 9068 + ((i49 - i40 | 0) >>> 3 << 1 << 2) | 0;
       }
       HEAP32[i88 >> 2] = i40;
       HEAP32[i89 + 12 >> 2] = i40;
       HEAP32[i40 + 8 >> 2] = i89;
       HEAP32[i40 + 12 >> 2] = 9068 + ((i49 - i40 | 0) >>> 3 << 1 << 2);
       break;
      }
      if ((i49 - i40 | 0) >>> 8) if ((i49 - i40 | 0) >>> 0 > 16777215) i90 = 31; else {
       i52 = (i49 - i40 | 0) >>> 8 << ((((i49 - i40 | 0) >>> 8) + 1048320 | 0) >>> 16 & 8);
       i50 = 14 - ((i52 + 520192 | 0) >>> 16 & 4 | (((i49 - i40 | 0) >>> 8) + 1048320 | 0) >>> 16 & 8 | ((i52 << ((i52 + 520192 | 0) >>> 16 & 4)) + 245760 | 0) >>> 16 & 2) + (i52 << ((i52 + 520192 | 0) >>> 16 & 4) << (((i52 << ((i52 + 520192 | 0) >>> 16 & 4)) + 245760 | 0) >>> 16 & 2) >>> 15) | 0;
       i90 = (i49 - i40 | 0) >>> (i50 + 7 | 0) & 1 | i50 << 1;
      } else i90 = 0;
      i50 = 9332 + (i90 << 2) | 0;
      HEAP32[i40 + 28 >> 2] = i90;
      HEAP32[i40 + 20 >> 2] = 0;
      HEAP32[i40 + 16 >> 2] = 0;
      i52 = HEAP32[2258] | 0;
      i48 = 1 << i90;
      if (!(i52 & i48)) {
       HEAP32[2258] = i52 | i48;
       HEAP32[i50 >> 2] = i40;
       HEAP32[i40 + 24 >> 2] = i50;
       HEAP32[i40 + 12 >> 2] = i40;
       HEAP32[i40 + 8 >> 2] = i40;
       break;
      }
      i48 = HEAP32[i50 >> 2] | 0;
      L425 : do if ((HEAP32[i48 + 4 >> 2] & -8 | 0) != (i49 - i40 | 0)) {
       i50 = i49 - i40 << ((i90 | 0) == 31 ? 0 : 25 - (i90 >>> 1) | 0);
       i52 = i48;
       while (1) {
        i91 = i52 + 16 + (i50 >>> 31 << 2) | 0;
        i41 = HEAP32[i91 >> 2] | 0;
        if (!i41) {
         i92 = i52;
         break;
        }
        if ((HEAP32[i41 + 4 >> 2] & -8 | 0) == (i49 - i40 | 0)) {
         i93 = i41;
         break L425;
        } else {
         i50 = i50 << 1;
         i52 = i41;
        }
       }
       if (i91 >>> 0 < (HEAP32[2261] | 0) >>> 0) _abort(); else {
        HEAP32[i91 >> 2] = i40;
        HEAP32[i40 + 24 >> 2] = i92;
        HEAP32[i40 + 12 >> 2] = i40;
        HEAP32[i40 + 8 >> 2] = i40;
        break L272;
       }
      } else i93 = i48; while (0);
      i48 = i93 + 8 | 0;
      i49 = HEAP32[i48 >> 2] | 0;
      i52 = HEAP32[2261] | 0;
      if (i49 >>> 0 >= i52 >>> 0 & i93 >>> 0 >= i52 >>> 0) {
       HEAP32[i49 + 12 >> 2] = i40;
       HEAP32[i48 >> 2] = i40;
       HEAP32[i40 + 8 >> 2] = i49;
       HEAP32[i40 + 12 >> 2] = i93;
       HEAP32[i40 + 24 >> 2] = 0;
       break;
      } else _abort();
     }
    } else {
     i49 = HEAP32[2261] | 0;
     if ((i49 | 0) == 0 | i56 >>> 0 < i49 >>> 0) HEAP32[2261] = i56;
     HEAP32[2369] = i56;
     HEAP32[2370] = i57;
     HEAP32[2372] = 0;
     HEAP32[2266] = HEAP32[2375];
     HEAP32[2265] = -1;
     i49 = 0;
     do {
      i48 = i49 << 1;
      HEAP32[9068 + (i48 + 3 << 2) >> 2] = 9068 + (i48 << 2);
      HEAP32[9068 + (i48 + 2 << 2) >> 2] = 9068 + (i48 << 2);
      i49 = i49 + 1 | 0;
     } while ((i49 | 0) != 32);
     i49 = i56 + 8 | 0;
     i48 = (i49 & 7 | 0) == 0 ? 0 : 0 - i49 & 7;
     i49 = i57 + -40 - i48 | 0;
     HEAP32[2263] = i56 + i48;
     HEAP32[2260] = i49;
     HEAP32[i56 + (i48 + 4) >> 2] = i49 | 1;
     HEAP32[i56 + (i57 + -36) >> 2] = 40;
     HEAP32[2264] = HEAP32[2379];
    } while (0);
    i40 = HEAP32[2260] | 0;
    if (i40 >>> 0 > i24 >>> 0) {
     i51 = i40 - i24 | 0;
     HEAP32[2260] = i51;
     i40 = HEAP32[2263] | 0;
     HEAP32[2263] = i40 + i24;
     HEAP32[i40 + (i24 + 4) >> 2] = i51 | 1;
     HEAP32[i40 + 4 >> 2] = i24 | 3;
     i7 = i40 + 8 | 0;
     break;
    }
   }
   HEAP32[(___errno_location() | 0) >> 2] = 12;
   i7 = 0;
  } else i7 = 0;
 } while (0);
 return i7 | 0;
}

function _printf_core(i1, i2, i3, i4, i5) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 i5 = i5 | 0;
 var i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0, i24 = 0, i25 = 0, i26 = 0, i27 = 0, i28 = 0, i29 = 0, i30 = 0, i31 = 0, i32 = 0, i33 = 0, i34 = 0, i35 = 0, i36 = 0, i37 = 0, i38 = 0, i39 = 0, i40 = 0, i41 = 0, i42 = 0, i43 = 0, i44 = 0, i45 = 0, i46 = 0, i47 = 0, i48 = 0, i49 = 0, i50 = 0, i51 = 0, i52 = 0, i53 = 0, i54 = 0, i55 = 0, i56 = 0, i57 = 0, i58 = 0, i59 = 0, i60 = 0, i61 = 0, i62 = 0, i63 = 0, i64 = 0, i65 = 0, i66 = 0, i67 = 0, i68 = 0, i69 = 0, i70 = 0, i71 = 0, i72 = 0, i73 = 0, i74 = 0, i75 = 0, i76 = 0, i77 = 0, i78 = 0, i79 = 0, d80 = 0.0, d81 = 0.0, i82 = 0, i83 = 0, d84 = 0.0, d85 = 0.0, d86 = 0.0, i87 = 0, i88 = 0, i89 = 0, i90 = 0, i91 = 0, i92 = 0, i93 = 0, d94 = 0.0, i95 = 0, i96 = 0, i97 = 0, i98 = 0, i99 = 0, i100 = 0, i101 = 0, i102 = 0, i103 = 0, i104 = 0, i105 = 0, i106 = 0, i107 = 0, i108 = 0, i109 = 0, i110 = 0, i111 = 0, i112 = 0, i113 = 0, i114 = 0, i115 = 0, i116 = 0, i117 = 0, i118 = 0, i119 = 0, i120 = 0, d121 = 0.0, d122 = 0.0, d123 = 0.0, i124 = 0, i125 = 0, i126 = 0, i127 = 0, i128 = 0, i129 = 0, i130 = 0, i131 = 0, i132 = 0, i133 = 0, i134 = 0, i135 = 0, i136 = 0, i137 = 0, i138 = 0, i139 = 0, i140 = 0, i141 = 0, i142 = 0, i143 = 0, i144 = 0, i145 = 0, i146 = 0, i147 = 0, i148 = 0, i149 = 0, i150 = 0, i151 = 0, i152 = 0, i153 = 0, i154 = 0, i155 = 0, i156 = 0, i157 = 0, i158 = 0;
 i6 = STACKTOP;
 STACKTOP = STACKTOP + 624 | 0;
 i7 = i6 + 536 + 40 | 0;
 i8 = i6 + 576 + 12 | 0;
 i9 = i6 + 588 + 9 | 0;
 i10 = i2;
 i2 = 0;
 i11 = 0;
 i12 = 0;
 L1 : while (1) {
  do if ((i2 | 0) > -1) if ((i11 | 0) > (2147483647 - i2 | 0)) {
   HEAP32[(___errno_location() | 0) >> 2] = 75;
   i13 = -1;
   break;
  } else {
   i13 = i11 + i2 | 0;
   break;
  } else i13 = i2; while (0);
  i14 = HEAP8[i10 >> 0] | 0;
  if (!(i14 << 24 >> 24)) {
   i15 = i13;
   i16 = i12;
   i17 = 245;
   break;
  } else {
   i18 = i14;
   i19 = i10;
  }
  L9 : while (1) {
   switch (i18 << 24 >> 24) {
   case 37:
    {
     i20 = i19;
     i21 = i19;
     i17 = 9;
     break L9;
     break;
    }
   case 0:
    {
     i22 = i19;
     i23 = i19;
     break L9;
     break;
    }
   default:
    {}
   }
   i14 = i19 + 1 | 0;
   i18 = HEAP8[i14 >> 0] | 0;
   i19 = i14;
  }
  L12 : do if ((i17 | 0) == 9) while (1) {
   i17 = 0;
   if ((HEAP8[i20 + 1 >> 0] | 0) != 37) {
    i22 = i20;
    i23 = i21;
    break L12;
   }
   i14 = i21 + 1 | 0;
   i24 = i20 + 2 | 0;
   if ((HEAP8[i24 >> 0] | 0) == 37) {
    i20 = i24;
    i21 = i14;
    i17 = 9;
   } else {
    i22 = i24;
    i23 = i14;
    break;
   }
  } while (0);
  i14 = i23 - i10 | 0;
  if ((i1 | 0) != 0 ? (HEAP32[i1 >> 2] & 32 | 0) == 0 : 0) ___fwritex(i10, i14, i1) | 0;
  if ((i23 | 0) != (i10 | 0)) {
   i10 = i22;
   i2 = i13;
   i11 = i14;
   continue;
  }
  i24 = i22 + 1 | 0;
  i25 = HEAP8[i24 >> 0] | 0;
  if (((i25 << 24 >> 24) + -48 | 0) >>> 0 < 10) {
   i26 = (HEAP8[i22 + 2 >> 0] | 0) == 36;
   i27 = i26 ? i22 + 3 | 0 : i24;
   i28 = HEAP8[i27 >> 0] | 0;
   i29 = i26 ? (i25 << 24 >> 24) + -48 | 0 : -1;
   i30 = i26 ? 1 : i12;
   i31 = i27;
  } else {
   i28 = i25;
   i29 = -1;
   i30 = i12;
   i31 = i24;
  }
  i24 = i28 << 24 >> 24;
  L25 : do if ((i24 & -32 | 0) == 32) {
   i25 = i24;
   i27 = i28;
   i26 = 0;
   i32 = i31;
   while (1) {
    if (!(1 << i25 + -32 & 75913)) {
     i33 = i27;
     i34 = i26;
     i35 = i32;
     break L25;
    }
    i36 = 1 << (i27 << 24 >> 24) + -32 | i26;
    i37 = i32 + 1 | 0;
    i38 = HEAP8[i37 >> 0] | 0;
    i25 = i38 << 24 >> 24;
    if ((i25 & -32 | 0) != 32) {
     i33 = i38;
     i34 = i36;
     i35 = i37;
     break;
    } else {
     i27 = i38;
     i26 = i36;
     i32 = i37;
    }
   }
  } else {
   i33 = i28;
   i34 = 0;
   i35 = i31;
  } while (0);
  do if (i33 << 24 >> 24 == 42) {
   i24 = i35 + 1 | 0;
   i32 = (HEAP8[i24 >> 0] | 0) + -48 | 0;
   if (i32 >>> 0 < 10 ? (HEAP8[i35 + 2 >> 0] | 0) == 36 : 0) {
    HEAP32[i5 + (i32 << 2) >> 2] = 10;
    i39 = 1;
    i40 = i35 + 3 | 0;
    i41 = HEAP32[i4 + ((HEAP8[i24 >> 0] | 0) + -48 << 3) >> 2] | 0;
   } else {
    if (i30) {
     i42 = -1;
     break L1;
    }
    if (!i1) {
     i43 = i24;
     i44 = i34;
     i45 = 0;
     i46 = 0;
     break;
    }
    i32 = (HEAP32[i3 >> 2] | 0) + (4 - 1) & ~(4 - 1);
    i26 = HEAP32[i32 >> 2] | 0;
    HEAP32[i3 >> 2] = i32 + 4;
    i39 = 0;
    i40 = i24;
    i41 = i26;
   }
   if ((i41 | 0) < 0) {
    i43 = i40;
    i44 = i34 | 8192;
    i45 = i39;
    i46 = 0 - i41 | 0;
   } else {
    i43 = i40;
    i44 = i34;
    i45 = i39;
    i46 = i41;
   }
  } else {
   i26 = (i33 << 24 >> 24) + -48 | 0;
   if (i26 >>> 0 < 10) {
    i24 = i35;
    i32 = 0;
    i27 = i26;
    while (1) {
     i47 = (i32 * 10 | 0) + i27 | 0;
     i48 = i24 + 1 | 0;
     i27 = (HEAP8[i48 >> 0] | 0) + -48 | 0;
     if (i27 >>> 0 >= 10) break; else {
      i24 = i48;
      i32 = i47;
     }
    }
    if ((i47 | 0) < 0) {
     i42 = -1;
     break L1;
    } else {
     i43 = i48;
     i44 = i34;
     i45 = i30;
     i46 = i47;
    }
   } else {
    i43 = i35;
    i44 = i34;
    i45 = i30;
    i46 = 0;
   }
  } while (0);
  L46 : do if ((HEAP8[i43 >> 0] | 0) == 46) {
   i32 = i43 + 1 | 0;
   i24 = HEAP8[i32 >> 0] | 0;
   if (i24 << 24 >> 24 != 42) {
    if (((i24 << 24 >> 24) + -48 | 0) >>> 0 < 10) {
     i49 = i32;
     i50 = 0;
     i51 = (i24 << 24 >> 24) + -48 | 0;
    } else {
     i52 = i32;
     i53 = 0;
     break;
    }
    while (1) {
     i32 = (i50 * 10 | 0) + i51 | 0;
     i24 = i49 + 1 | 0;
     i51 = (HEAP8[i24 >> 0] | 0) + -48 | 0;
     if (i51 >>> 0 >= 10) {
      i52 = i24;
      i53 = i32;
      break L46;
     } else {
      i49 = i24;
      i50 = i32;
     }
    }
   }
   i32 = i43 + 2 | 0;
   i24 = (HEAP8[i32 >> 0] | 0) + -48 | 0;
   if (i24 >>> 0 < 10 ? (HEAP8[i43 + 3 >> 0] | 0) == 36 : 0) {
    HEAP32[i5 + (i24 << 2) >> 2] = 10;
    i52 = i43 + 4 | 0;
    i53 = HEAP32[i4 + ((HEAP8[i32 >> 0] | 0) + -48 << 3) >> 2] | 0;
    break;
   }
   if (i45) {
    i42 = -1;
    break L1;
   }
   if (i1) {
    i24 = (HEAP32[i3 >> 2] | 0) + (4 - 1) & ~(4 - 1);
    i27 = HEAP32[i24 >> 2] | 0;
    HEAP32[i3 >> 2] = i24 + 4;
    i52 = i32;
    i53 = i27;
   } else {
    i52 = i32;
    i53 = 0;
   }
  } else {
   i52 = i43;
   i53 = -1;
  } while (0);
  i32 = i52;
  i27 = 0;
  while (1) {
   i24 = (HEAP8[i32 >> 0] | 0) + -65 | 0;
   if (i24 >>> 0 > 57) {
    i42 = -1;
    break L1;
   }
   i54 = i32 + 1 | 0;
   i55 = HEAP8[17757 + (i27 * 58 | 0) + i24 >> 0] | 0;
   if (((i55 & 255) + -1 | 0) >>> 0 < 8) {
    i32 = i54;
    i27 = i55 & 255;
   } else {
    i56 = i32;
    i57 = i27;
    break;
   }
  }
  if (!(i55 << 24 >> 24)) {
   i42 = -1;
   break;
  }
  i27 = (i29 | 0) > -1;
  do if (i55 << 24 >> 24 == 19) if (i27) {
   i42 = -1;
   break L1;
  } else i17 = 52; else {
   if (i27) {
    HEAP32[i5 + (i29 << 2) >> 2] = i55 & 255;
    i32 = i4 + (i29 << 3) | 0;
    i24 = HEAP32[i32 + 4 >> 2] | 0;
    HEAP32[i6 >> 2] = HEAP32[i32 >> 2];
    HEAP32[i6 + 4 >> 2] = i24;
    i17 = 52;
    break;
   }
   if (!i1) {
    i42 = 0;
    break L1;
   }
   _pop_arg555(i6, i55 & 255, i3);
  } while (0);
  if ((i17 | 0) == 52 ? (i17 = 0, (i1 | 0) == 0) : 0) {
   i10 = i54;
   i2 = i13;
   i11 = i14;
   i12 = i45;
   continue;
  }
  i27 = HEAP8[i56 >> 0] | 0;
  i24 = (i57 | 0) != 0 & (i27 & 15 | 0) == 3 ? i27 & -33 : i27;
  i27 = i44 & -65537;
  i32 = (i44 & 8192 | 0) == 0 ? i44 : i27;
  L75 : do switch (i24 | 0) {
  case 110:
   {
    switch (i57 | 0) {
    case 0:
     {
      HEAP32[HEAP32[i6 >> 2] >> 2] = i13;
      i10 = i54;
      i2 = i13;
      i11 = i14;
      i12 = i45;
      continue L1;
      break;
     }
    case 1:
     {
      HEAP32[HEAP32[i6 >> 2] >> 2] = i13;
      i10 = i54;
      i2 = i13;
      i11 = i14;
      i12 = i45;
      continue L1;
      break;
     }
    case 2:
     {
      i26 = HEAP32[i6 >> 2] | 0;
      HEAP32[i26 >> 2] = i13;
      HEAP32[i26 + 4 >> 2] = ((i13 | 0) < 0) << 31 >> 31;
      i10 = i54;
      i2 = i13;
      i11 = i14;
      i12 = i45;
      continue L1;
      break;
     }
    case 3:
     {
      HEAP16[HEAP32[i6 >> 2] >> 1] = i13;
      i10 = i54;
      i2 = i13;
      i11 = i14;
      i12 = i45;
      continue L1;
      break;
     }
    case 4:
     {
      HEAP8[HEAP32[i6 >> 2] >> 0] = i13;
      i10 = i54;
      i2 = i13;
      i11 = i14;
      i12 = i45;
      continue L1;
      break;
     }
    case 6:
     {
      HEAP32[HEAP32[i6 >> 2] >> 2] = i13;
      i10 = i54;
      i2 = i13;
      i11 = i14;
      i12 = i45;
      continue L1;
      break;
     }
    case 7:
     {
      i26 = HEAP32[i6 >> 2] | 0;
      HEAP32[i26 >> 2] = i13;
      HEAP32[i26 + 4 >> 2] = ((i13 | 0) < 0) << 31 >> 31;
      i10 = i54;
      i2 = i13;
      i11 = i14;
      i12 = i45;
      continue L1;
      break;
     }
    default:
     {
      i10 = i54;
      i2 = i13;
      i11 = i14;
      i12 = i45;
      continue L1;
     }
    }
    break;
   }
  case 112:
   {
    i58 = i32 | 8;
    i59 = i53 >>> 0 > 8 ? i53 : 8;
    i60 = 120;
    i17 = 64;
    break;
   }
  case 88:
  case 120:
   {
    i58 = i32;
    i59 = i53;
    i60 = i24;
    i17 = 64;
    break;
   }
  case 111:
   {
    i26 = HEAP32[i6 >> 2] | 0;
    i25 = HEAP32[i6 + 4 >> 2] | 0;
    if ((i26 | 0) == 0 & (i25 | 0) == 0) i61 = i7; else {
     i37 = i7;
     i36 = i26;
     i26 = i25;
     while (1) {
      i25 = i37 + -1 | 0;
      HEAP8[i25 >> 0] = i36 & 7 | 48;
      i36 = _bitshift64Lshr(i36 | 0, i26 | 0, 3) | 0;
      i26 = tempRet0;
      if ((i36 | 0) == 0 & (i26 | 0) == 0) {
       i61 = i25;
       break;
      } else i37 = i25;
     }
    }
    if (!(i32 & 8)) {
     i62 = i61;
     i63 = i32;
     i64 = i53;
     i65 = 0;
     i66 = 18237;
     i17 = 77;
    } else {
     i37 = i7 - i61 + 1 | 0;
     i62 = i61;
     i63 = i32;
     i64 = (i53 | 0) < (i37 | 0) ? i37 : i53;
     i65 = 0;
     i66 = 18237;
     i17 = 77;
    }
    break;
   }
  case 105:
  case 100:
   {
    i37 = HEAP32[i6 >> 2] | 0;
    i26 = HEAP32[i6 + 4 >> 2] | 0;
    if ((i26 | 0) < 0) {
     i36 = _i64Subtract(0, 0, i37 | 0, i26 | 0) | 0;
     i25 = tempRet0;
     HEAP32[i6 >> 2] = i36;
     HEAP32[i6 + 4 >> 2] = i25;
     i67 = i36;
     i68 = i25;
     i69 = 1;
     i70 = 18237;
     i17 = 76;
     break L75;
    }
    if (!(i32 & 2048)) {
     i67 = i37;
     i68 = i26;
     i69 = i32 & 1;
     i70 = (i32 & 1 | 0) == 0 ? 18237 : 18239;
     i17 = 76;
    } else {
     i67 = i37;
     i68 = i26;
     i69 = 1;
     i70 = 18238;
     i17 = 76;
    }
    break;
   }
  case 117:
   {
    i67 = HEAP32[i6 >> 2] | 0;
    i68 = HEAP32[i6 + 4 >> 2] | 0;
    i69 = 0;
    i70 = 18237;
    i17 = 76;
    break;
   }
  case 99:
   {
    HEAP8[i6 + 536 + 39 >> 0] = HEAP32[i6 >> 2];
    i71 = i6 + 536 + 39 | 0;
    i72 = i27;
    i73 = 1;
    i74 = 0;
    i75 = 18237;
    i76 = i7;
    break;
   }
  case 109:
   {
    i77 = _strerror(HEAP32[(___errno_location() | 0) >> 2] | 0) | 0;
    i17 = 82;
    break;
   }
  case 115:
   {
    i26 = HEAP32[i6 >> 2] | 0;
    i77 = (i26 | 0) != 0 ? i26 : 18247;
    i17 = 82;
    break;
   }
  case 67:
   {
    HEAP32[i6 + 8 >> 2] = HEAP32[i6 >> 2];
    HEAP32[i6 + 8 + 4 >> 2] = 0;
    HEAP32[i6 >> 2] = i6 + 8;
    i78 = -1;
    i17 = 86;
    break;
   }
  case 83:
   {
    if (!i53) {
     _pad(i1, 32, i46, 0, i32);
     i79 = 0;
     i17 = 98;
    } else {
     i78 = i53;
     i17 = 86;
    }
    break;
   }
  case 65:
  case 71:
  case 70:
  case 69:
  case 97:
  case 103:
  case 102:
  case 101:
   {
    d80 = +HEAPF64[i6 >> 3];
    HEAP32[i6 + 16 >> 2] = 0;
    HEAPF64[tempDoublePtr >> 3] = d80;
    if ((HEAP32[tempDoublePtr + 4 >> 2] | 0) >= 0) if (!(i32 & 2048)) {
     d81 = d80;
     i82 = i32 & 1;
     i83 = (i32 & 1 | 0) == 0 ? 18255 : 18260;
    } else {
     d81 = d80;
     i82 = 1;
     i83 = 18257;
    } else {
     d81 = -d80;
     i82 = 1;
     i83 = 18254;
    }
    HEAPF64[tempDoublePtr >> 3] = d81;
    i26 = HEAP32[tempDoublePtr + 4 >> 2] & 2146435072;
    do if (i26 >>> 0 < 2146435072 | (i26 | 0) == 2146435072 & 0 < 0) {
     d80 = +_frexpl(d81, i6 + 16 | 0) * 2.0;
     if (d80 != 0.0) HEAP32[i6 + 16 >> 2] = (HEAP32[i6 + 16 >> 2] | 0) + -1;
     if ((i24 | 32 | 0) == 97) {
      i37 = (i24 & 32 | 0) == 0 ? i83 : i83 + 9 | 0;
      i25 = i82 | 2;
      i36 = 12 - i53 | 0;
      do if (!(i53 >>> 0 > 11 | (i36 | 0) == 0)) {
       i38 = i36;
       d84 = 8.0;
       while (1) {
        i38 = i38 + -1 | 0;
        d85 = d84 * 16.0;
        if (!i38) break; else d84 = d85;
       }
       if ((HEAP8[i37 >> 0] | 0) == 45) {
        d86 = -(d85 + (-d80 - d85));
        break;
       } else {
        d86 = d80 + d85 - d85;
        break;
       }
      } else d86 = d80; while (0);
      i36 = HEAP32[i6 + 16 >> 2] | 0;
      i38 = (i36 | 0) < 0 ? 0 - i36 | 0 : i36;
      i87 = _fmt_u(i38, ((i38 | 0) < 0) << 31 >> 31, i6 + 576 + 12 | 0) | 0;
      if ((i87 | 0) == (i6 + 576 + 12 | 0)) {
       HEAP8[i6 + 576 + 11 >> 0] = 48;
       i88 = i6 + 576 + 11 | 0;
      } else i88 = i87;
      HEAP8[i88 + -1 >> 0] = (i36 >> 31 & 2) + 43;
      i36 = i88 + -2 | 0;
      HEAP8[i36 >> 0] = i24 + 15;
      i87 = (i53 | 0) < 1;
      d84 = d86;
      i38 = i6 + 588 | 0;
      while (1) {
       i89 = ~~d84;
       i90 = i38 + 1 | 0;
       HEAP8[i38 >> 0] = HEAPU8[18221 + i89 >> 0] | i24 & 32;
       d84 = (d84 - +(i89 | 0)) * 16.0;
       do if ((i90 - (i6 + 588) | 0) == 1) {
        if ((i32 & 8 | 0) == 0 & (i87 & d84 == 0.0)) {
         i91 = i90;
         break;
        }
        HEAP8[i90 >> 0] = 46;
        i91 = i38 + 2 | 0;
       } else i91 = i90; while (0);
       if (!(d84 != 0.0)) {
        i92 = i91;
        break;
       } else i38 = i91;
      }
      i38 = ((i53 | 0) != 0 ? (-2 - (i6 + 588) + i92 | 0) < (i53 | 0) : 0) ? i8 + 2 + i53 - i36 | 0 : i8 - (i6 + 588) - i36 + i92 | 0;
      _pad(i1, 32, i46, i38 + i25 | 0, i32);
      if (!(HEAP32[i1 >> 2] & 32)) ___fwritex(i37, i25, i1) | 0;
      _pad(i1, 48, i46, i38 + i25 | 0, i32 ^ 65536);
      if (!(HEAP32[i1 >> 2] & 32)) ___fwritex(i6 + 588 | 0, i92 - (i6 + 588) | 0, i1) | 0;
      _pad(i1, 48, i38 - (i92 - (i6 + 588) + (i8 - i36)) | 0, 0, 0);
      if (!(HEAP32[i1 >> 2] & 32)) ___fwritex(i36, i8 - i36 | 0, i1) | 0;
      _pad(i1, 32, i46, i38 + i25 | 0, i32 ^ 8192);
      i93 = (i38 + i25 | 0) < (i46 | 0) ? i46 : i38 + i25 | 0;
      break;
     }
     i38 = (i53 | 0) < 0 ? 6 : i53;
     if (d80 != 0.0) {
      i87 = (HEAP32[i6 + 16 >> 2] | 0) + -28 | 0;
      HEAP32[i6 + 16 >> 2] = i87;
      d94 = d80 * 268435456.0;
      i95 = i87;
     } else {
      d94 = d80;
      i95 = HEAP32[i6 + 16 >> 2] | 0;
     }
     i87 = (i95 | 0) < 0 ? i6 + 24 | 0 : i6 + 24 + 288 | 0;
     d84 = d94;
     i90 = i87;
     while (1) {
      i89 = ~~d84 >>> 0;
      HEAP32[i90 >> 2] = i89;
      i96 = i90 + 4 | 0;
      d84 = (d84 - +(i89 >>> 0)) * 1.0e9;
      if (!(d84 != 0.0)) break; else i90 = i96;
     }
     i90 = HEAP32[i6 + 16 >> 2] | 0;
     if ((i90 | 0) > 0) {
      i25 = i90;
      i36 = i87;
      i37 = i96;
      while (1) {
       i89 = (i25 | 0) > 29 ? 29 : i25;
       i97 = i37 + -4 | 0;
       do if (i97 >>> 0 < i36 >>> 0) i98 = i36; else {
        i99 = 0;
        i100 = i97;
        while (1) {
         i101 = _bitshift64Shl(HEAP32[i100 >> 2] | 0, 0, i89 | 0) | 0;
         i102 = _i64Add(i101 | 0, tempRet0 | 0, i99 | 0, 0) | 0;
         i101 = tempRet0;
         i103 = ___uremdi3(i102 | 0, i101 | 0, 1e9, 0) | 0;
         HEAP32[i100 >> 2] = i103;
         i104 = ___udivdi3(i102 | 0, i101 | 0, 1e9, 0) | 0;
         i100 = i100 + -4 | 0;
         if (i100 >>> 0 < i36 >>> 0) break; else i99 = i104;
        }
        if (!i104) {
         i98 = i36;
         break;
        }
        i99 = i36 + -4 | 0;
        HEAP32[i99 >> 2] = i104;
        i98 = i99;
       } while (0);
       i97 = i37;
       while (1) {
        if (i97 >>> 0 <= i98 >>> 0) {
         i105 = i97;
         break;
        }
        i99 = i97 + -4 | 0;
        if (!(HEAP32[i99 >> 2] | 0)) i97 = i99; else {
         i105 = i97;
         break;
        }
       }
       i97 = (HEAP32[i6 + 16 >> 2] | 0) - i89 | 0;
       HEAP32[i6 + 16 >> 2] = i97;
       if ((i97 | 0) > 0) {
        i25 = i97;
        i36 = i98;
        i37 = i105;
       } else {
        i106 = i97;
        i107 = i98;
        i108 = i105;
        break;
       }
      }
     } else {
      i106 = i90;
      i107 = i87;
      i108 = i96;
     }
     if ((i106 | 0) < 0) {
      i37 = i106;
      i36 = i107;
      i25 = i108;
      while (1) {
       i97 = 0 - i37 | 0;
       i99 = (i97 | 0) > 9 ? 9 : i97;
       do if (i36 >>> 0 < i25 >>> 0) {
        i97 = 0;
        i100 = i36;
        while (1) {
         i101 = HEAP32[i100 >> 2] | 0;
         HEAP32[i100 >> 2] = (i101 >>> i99) + i97;
         i109 = Math_imul(i101 & (1 << i99) + -1, 1e9 >>> i99) | 0;
         i100 = i100 + 4 | 0;
         if (i100 >>> 0 >= i25 >>> 0) break; else i97 = i109;
        }
        i97 = (HEAP32[i36 >> 2] | 0) == 0 ? i36 + 4 | 0 : i36;
        if (!i109) {
         i110 = i97;
         i111 = i25;
         break;
        }
        HEAP32[i25 >> 2] = i109;
        i110 = i97;
        i111 = i25 + 4 | 0;
       } else {
        i110 = (HEAP32[i36 >> 2] | 0) == 0 ? i36 + 4 | 0 : i36;
        i111 = i25;
       } while (0);
       i89 = (i24 | 32 | 0) == 102 ? i87 : i110;
       i97 = (i111 - i89 >> 2 | 0) > (((i38 + 25 | 0) / 9 | 0) + 1 | 0) ? i89 + (((i38 + 25 | 0) / 9 | 0) + 1 << 2) | 0 : i111;
       i37 = (HEAP32[i6 + 16 >> 2] | 0) + i99 | 0;
       HEAP32[i6 + 16 >> 2] = i37;
       if ((i37 | 0) >= 0) {
        i112 = i110;
        i113 = i97;
        break;
       } else {
        i36 = i110;
        i25 = i97;
       }
      }
     } else {
      i112 = i107;
      i113 = i108;
     }
     do if (i112 >>> 0 < i113 >>> 0) {
      i25 = (i87 - i112 >> 2) * 9 | 0;
      i36 = HEAP32[i112 >> 2] | 0;
      if (i36 >>> 0 < 10) {
       i114 = i25;
       break;
      } else {
       i115 = i25;
       i116 = 10;
      }
      while (1) {
       i116 = i116 * 10 | 0;
       i25 = i115 + 1 | 0;
       if (i36 >>> 0 < i116 >>> 0) {
        i114 = i25;
        break;
       } else i115 = i25;
      }
     } else i114 = 0; while (0);
     i36 = i38 - ((i24 | 32 | 0) != 102 ? i114 : 0) + (((i38 | 0) != 0 & (i24 | 32 | 0) == 103) << 31 >> 31) | 0;
     if ((i36 | 0) < (((i113 - i87 >> 2) * 9 | 0) + -9 | 0)) {
      i99 = i87 + (((i36 + 9216 | 0) / 9 | 0) + -1023 << 2) | 0;
      if ((((i36 + 9216 | 0) % 9 | 0) + 1 | 0) < 9) {
       i25 = 10;
       i37 = ((i36 + 9216 | 0) % 9 | 0) + 1 | 0;
       while (1) {
        i90 = i25 * 10 | 0;
        i37 = i37 + 1 | 0;
        if ((i37 | 0) == 9) {
         i117 = i90;
         break;
        } else i25 = i90;
       }
      } else i117 = 10;
      i25 = HEAP32[i99 >> 2] | 0;
      i37 = (i25 >>> 0) % (i117 >>> 0) | 0;
      if ((i37 | 0) == 0 ? (i87 + (((i36 + 9216 | 0) / 9 | 0) + -1022 << 2) | 0) == (i113 | 0) : 0) {
       i118 = i112;
       i119 = i99;
       i120 = i114;
      } else i17 = 163;
      do if ((i17 | 0) == 163) {
       i17 = 0;
       d84 = (((i25 >>> 0) / (i117 >>> 0) | 0) & 1 | 0) == 0 ? 9007199254740992.0 : 9007199254740994.0;
       i90 = (i117 | 0) / 2 | 0;
       do if (i37 >>> 0 < i90 >>> 0) d121 = .5; else {
        if ((i37 | 0) == (i90 | 0) ? (i87 + (((i36 + 9216 | 0) / 9 | 0) + -1022 << 2) | 0) == (i113 | 0) : 0) {
         d121 = 1.0;
         break;
        }
        d121 = 1.5;
       } while (0);
       do if (!i82) {
        d122 = d84;
        d123 = d121;
       } else {
        if ((HEAP8[i83 >> 0] | 0) != 45) {
         d122 = d84;
         d123 = d121;
         break;
        }
        d122 = -d84;
        d123 = -d121;
       } while (0);
       HEAP32[i99 >> 2] = i25 - i37;
       if (!(d122 + d123 != d122)) {
        i118 = i112;
        i119 = i99;
        i120 = i114;
        break;
       }
       i90 = i25 - i37 + i117 | 0;
       HEAP32[i99 >> 2] = i90;
       if (i90 >>> 0 > 999999999) {
        i90 = i112;
        i97 = i99;
        while (1) {
         i89 = i97 + -4 | 0;
         HEAP32[i97 >> 2] = 0;
         if (i89 >>> 0 < i90 >>> 0) {
          i100 = i90 + -4 | 0;
          HEAP32[i100 >> 2] = 0;
          i124 = i100;
         } else i124 = i90;
         i100 = (HEAP32[i89 >> 2] | 0) + 1 | 0;
         HEAP32[i89 >> 2] = i100;
         if (i100 >>> 0 > 999999999) {
          i90 = i124;
          i97 = i89;
         } else {
          i125 = i124;
          i126 = i89;
          break;
         }
        }
       } else {
        i125 = i112;
        i126 = i99;
       }
       i97 = (i87 - i125 >> 2) * 9 | 0;
       i90 = HEAP32[i125 >> 2] | 0;
       if (i90 >>> 0 < 10) {
        i118 = i125;
        i119 = i126;
        i120 = i97;
        break;
       } else {
        i127 = i97;
        i128 = 10;
       }
       while (1) {
        i128 = i128 * 10 | 0;
        i97 = i127 + 1 | 0;
        if (i90 >>> 0 < i128 >>> 0) {
         i118 = i125;
         i119 = i126;
         i120 = i97;
         break;
        } else i127 = i97;
       }
      } while (0);
      i99 = i119 + 4 | 0;
      i129 = i118;
      i130 = i120;
      i131 = i113 >>> 0 > i99 >>> 0 ? i99 : i113;
     } else {
      i129 = i112;
      i130 = i114;
      i131 = i113;
     }
     i99 = 0 - i130 | 0;
     i37 = i131;
     while (1) {
      if (i37 >>> 0 <= i129 >>> 0) {
       i132 = 0;
       i133 = i37;
       break;
      }
      i25 = i37 + -4 | 0;
      if (!(HEAP32[i25 >> 2] | 0)) i37 = i25; else {
       i132 = 1;
       i133 = i37;
       break;
      }
     }
     do if ((i24 | 32 | 0) == 103) {
      if ((((i38 | 0) != 0 ^ 1) + i38 | 0) > (i130 | 0) & (i130 | 0) > -5) {
       i134 = i24 + -1 | 0;
       i135 = ((i38 | 0) != 0 ^ 1) + i38 + -1 - i130 | 0;
      } else {
       i134 = i24 + -2 | 0;
       i135 = ((i38 | 0) != 0 ^ 1) + i38 + -1 | 0;
      }
      if (i32 & 8) {
       i136 = i134;
       i137 = i135;
       i138 = i32 & 8;
       break;
      }
      do if (i132) {
       i37 = HEAP32[i133 + -4 >> 2] | 0;
       if (!i37) {
        i139 = 9;
        break;
       }
       if (!((i37 >>> 0) % 10 | 0)) {
        i140 = 10;
        i141 = 0;
       } else {
        i139 = 0;
        break;
       }
       while (1) {
        i140 = i140 * 10 | 0;
        i25 = i141 + 1 | 0;
        if ((i37 >>> 0) % (i140 >>> 0) | 0) {
         i139 = i25;
         break;
        } else i141 = i25;
       }
      } else i139 = 9; while (0);
      i37 = ((i133 - i87 >> 2) * 9 | 0) + -9 | 0;
      if ((i134 | 32 | 0) == 102) {
       i25 = i37 - i139 | 0;
       i36 = (i25 | 0) < 0 ? 0 : i25;
       i136 = i134;
       i137 = (i135 | 0) < (i36 | 0) ? i135 : i36;
       i138 = 0;
       break;
      } else {
       i36 = i37 + i130 - i139 | 0;
       i37 = (i36 | 0) < 0 ? 0 : i36;
       i136 = i134;
       i137 = (i135 | 0) < (i37 | 0) ? i135 : i37;
       i138 = 0;
       break;
      }
     } else {
      i136 = i24;
      i137 = i38;
      i138 = i32 & 8;
     } while (0);
     i38 = i137 | i138;
     i37 = (i136 | 32 | 0) == 102;
     if (i37) {
      i142 = (i130 | 0) > 0 ? i130 : 0;
      i143 = 0;
     } else {
      i36 = (i130 | 0) < 0 ? i99 : i130;
      i25 = _fmt_u(i36, ((i36 | 0) < 0) << 31 >> 31, i6 + 576 + 12 | 0) | 0;
      if ((i8 - i25 | 0) < 2) {
       i36 = i25;
       while (1) {
        i90 = i36 + -1 | 0;
        HEAP8[i90 >> 0] = 48;
        if ((i8 - i90 | 0) < 2) i36 = i90; else {
         i144 = i90;
         break;
        }
       }
      } else i144 = i25;
      HEAP8[i144 + -1 >> 0] = (i130 >> 31 & 2) + 43;
      i36 = i144 + -2 | 0;
      HEAP8[i36 >> 0] = i136;
      i142 = i8 - i36 | 0;
      i143 = i36;
     }
     i36 = i82 + 1 + i137 + ((i38 | 0) != 0 & 1) + i142 | 0;
     _pad(i1, 32, i46, i36, i32);
     if (!(HEAP32[i1 >> 2] & 32)) ___fwritex(i83, i82, i1) | 0;
     _pad(i1, 48, i46, i36, i32 ^ 65536);
     do if (i37) {
      i99 = i129 >>> 0 > i87 >>> 0 ? i87 : i129;
      i90 = i99;
      while (1) {
       i97 = _fmt_u(HEAP32[i90 >> 2] | 0, 0, i9) | 0;
       do if ((i90 | 0) == (i99 | 0)) {
        if ((i97 | 0) != (i9 | 0)) {
         i145 = i97;
         break;
        }
        HEAP8[i6 + 588 + 8 >> 0] = 48;
        i145 = i6 + 588 + 8 | 0;
       } else {
        if (i97 >>> 0 > (i6 + 588 | 0) >>> 0) i146 = i97; else {
         i145 = i97;
         break;
        }
        while (1) {
         i89 = i146 + -1 | 0;
         HEAP8[i89 >> 0] = 48;
         if (i89 >>> 0 > (i6 + 588 | 0) >>> 0) i146 = i89; else {
          i145 = i89;
          break;
         }
        }
       } while (0);
       if (!(HEAP32[i1 >> 2] & 32)) ___fwritex(i145, i9 - i145 | 0, i1) | 0;
       i147 = i90 + 4 | 0;
       if (i147 >>> 0 > i87 >>> 0) break; else i90 = i147;
      }
      do if (i38) {
       if (HEAP32[i1 >> 2] & 32) break;
       ___fwritex(18289, 1, i1) | 0;
      } while (0);
      if ((i137 | 0) > 0 & i147 >>> 0 < i133 >>> 0) {
       i90 = i137;
       i99 = i147;
       while (1) {
        i97 = _fmt_u(HEAP32[i99 >> 2] | 0, 0, i9) | 0;
        if (i97 >>> 0 > (i6 + 588 | 0) >>> 0) {
         i89 = i97;
         while (1) {
          i100 = i89 + -1 | 0;
          HEAP8[i100 >> 0] = 48;
          if (i100 >>> 0 > (i6 + 588 | 0) >>> 0) i89 = i100; else {
           i148 = i100;
           break;
          }
         }
        } else i148 = i97;
        if (!(HEAP32[i1 >> 2] & 32)) ___fwritex(i148, (i90 | 0) > 9 ? 9 : i90, i1) | 0;
        i99 = i99 + 4 | 0;
        i89 = i90 + -9 | 0;
        if (!((i90 | 0) > 9 & i99 >>> 0 < i133 >>> 0)) {
         i149 = i89;
         break;
        } else i90 = i89;
       }
      } else i149 = i137;
      _pad(i1, 48, i149 + 9 | 0, 9, 0);
     } else {
      i90 = i132 ? i133 : i129 + 4 | 0;
      if ((i137 | 0) > -1) {
       i99 = (i138 | 0) == 0;
       i89 = i137;
       i100 = i129;
       while (1) {
        i101 = _fmt_u(HEAP32[i100 >> 2] | 0, 0, i9) | 0;
        if ((i101 | 0) == (i9 | 0)) {
         HEAP8[i6 + 588 + 8 >> 0] = 48;
         i150 = i6 + 588 + 8 | 0;
        } else i150 = i101;
        do if ((i100 | 0) == (i129 | 0)) {
         i101 = i150 + 1 | 0;
         if (!(HEAP32[i1 >> 2] & 32)) ___fwritex(i150, 1, i1) | 0;
         if (i99 & (i89 | 0) < 1) {
          i151 = i101;
          break;
         }
         if (HEAP32[i1 >> 2] & 32) {
          i151 = i101;
          break;
         }
         ___fwritex(18289, 1, i1) | 0;
         i151 = i101;
        } else {
         if (i150 >>> 0 > (i6 + 588 | 0) >>> 0) i152 = i150; else {
          i151 = i150;
          break;
         }
         while (1) {
          i101 = i152 + -1 | 0;
          HEAP8[i101 >> 0] = 48;
          if (i101 >>> 0 > (i6 + 588 | 0) >>> 0) i152 = i101; else {
           i151 = i101;
           break;
          }
         }
        } while (0);
        i97 = i9 - i151 | 0;
        if (!(HEAP32[i1 >> 2] & 32)) ___fwritex(i151, (i89 | 0) > (i97 | 0) ? i97 : i89, i1) | 0;
        i101 = i89 - i97 | 0;
        i100 = i100 + 4 | 0;
        if (!(i100 >>> 0 < i90 >>> 0 & (i101 | 0) > -1)) {
         i153 = i101;
         break;
        } else i89 = i101;
       }
      } else i153 = i137;
      _pad(i1, 48, i153 + 18 | 0, 18, 0);
      if (HEAP32[i1 >> 2] & 32) break;
      ___fwritex(i143, i8 - i143 | 0, i1) | 0;
     } while (0);
     _pad(i1, 32, i46, i36, i32 ^ 8192);
     i93 = (i36 | 0) < (i46 | 0) ? i46 : i36;
    } else {
     i38 = d81 != d81 | 0.0 != 0.0;
     i87 = i38 ? 0 : i82;
     _pad(i1, 32, i46, i87 + 3 | 0, i27);
     i37 = HEAP32[i1 >> 2] | 0;
     if (!(i37 & 32)) {
      ___fwritex(i83, i87, i1) | 0;
      i154 = HEAP32[i1 >> 2] | 0;
     } else i154 = i37;
     if (!(i154 & 32)) ___fwritex(i38 ? ((i24 & 32 | 0) != 0 ? 18281 : 18285) : (i24 & 32 | 0) != 0 ? 18273 : 18277, 3, i1) | 0;
     _pad(i1, 32, i46, i87 + 3 | 0, i32 ^ 8192);
     i93 = (i87 + 3 | 0) < (i46 | 0) ? i46 : i87 + 3 | 0;
    } while (0);
    i10 = i54;
    i2 = i13;
    i11 = i93;
    i12 = i45;
    continue L1;
    break;
   }
  default:
   {
    i71 = i10;
    i72 = i32;
    i73 = i53;
    i74 = 0;
    i75 = 18237;
    i76 = i7;
   }
  } while (0);
  L313 : do if ((i17 | 0) == 64) {
   i17 = 0;
   i24 = HEAP32[i6 >> 2] | 0;
   i14 = HEAP32[i6 + 4 >> 2] | 0;
   i26 = i60 & 32;
   if (!((i24 | 0) == 0 & (i14 | 0) == 0)) {
    i87 = i7;
    i38 = i24;
    i24 = i14;
    while (1) {
     i155 = i87 + -1 | 0;
     HEAP8[i155 >> 0] = HEAPU8[18221 + (i38 & 15) >> 0] | i26;
     i38 = _bitshift64Lshr(i38 | 0, i24 | 0, 4) | 0;
     i24 = tempRet0;
     if ((i38 | 0) == 0 & (i24 | 0) == 0) break; else i87 = i155;
    }
    if ((i58 & 8 | 0) == 0 | (HEAP32[i6 >> 2] | 0) == 0 & (HEAP32[i6 + 4 >> 2] | 0) == 0) {
     i62 = i155;
     i63 = i58;
     i64 = i59;
     i65 = 0;
     i66 = 18237;
     i17 = 77;
    } else {
     i62 = i155;
     i63 = i58;
     i64 = i59;
     i65 = 2;
     i66 = 18237 + (i60 >> 4) | 0;
     i17 = 77;
    }
   } else {
    i62 = i7;
    i63 = i58;
    i64 = i59;
    i65 = 0;
    i66 = 18237;
    i17 = 77;
   }
  } else if ((i17 | 0) == 76) {
   i17 = 0;
   i62 = _fmt_u(i67, i68, i7) | 0;
   i63 = i32;
   i64 = i53;
   i65 = i69;
   i66 = i70;
   i17 = 77;
  } else if ((i17 | 0) == 82) {
   i17 = 0;
   i87 = _memchr(i77, 0, i53) | 0;
   i71 = i77;
   i72 = i27;
   i73 = (i87 | 0) == 0 ? i53 : i87 - i77 | 0;
   i74 = 0;
   i75 = 18237;
   i76 = (i87 | 0) == 0 ? i77 + i53 | 0 : i87;
  } else if ((i17 | 0) == 86) {
   i17 = 0;
   i87 = 0;
   i24 = 0;
   i38 = HEAP32[i6 >> 2] | 0;
   while (1) {
    i26 = HEAP32[i38 >> 2] | 0;
    if (!i26) {
     i156 = i87;
     i157 = i24;
     break;
    }
    i14 = _wctomb(i6 + 528 | 0, i26) | 0;
    if ((i14 | 0) < 0 | i14 >>> 0 > (i78 - i87 | 0) >>> 0) {
     i156 = i87;
     i157 = i14;
     break;
    }
    i26 = i14 + i87 | 0;
    if (i78 >>> 0 > i26 >>> 0) {
     i87 = i26;
     i24 = i14;
     i38 = i38 + 4 | 0;
    } else {
     i156 = i26;
     i157 = i14;
     break;
    }
   }
   if ((i157 | 0) < 0) {
    i42 = -1;
    break L1;
   }
   _pad(i1, 32, i46, i156, i32);
   if (!i156) {
    i79 = 0;
    i17 = 98;
   } else {
    i38 = 0;
    i24 = HEAP32[i6 >> 2] | 0;
    while (1) {
     i87 = HEAP32[i24 >> 2] | 0;
     if (!i87) {
      i79 = i156;
      i17 = 98;
      break L313;
     }
     i14 = _wctomb(i6 + 528 | 0, i87) | 0;
     i38 = i14 + i38 | 0;
     if ((i38 | 0) > (i156 | 0)) {
      i79 = i156;
      i17 = 98;
      break L313;
     }
     if (!(HEAP32[i1 >> 2] & 32)) ___fwritex(i6 + 528 | 0, i14, i1) | 0;
     if (i38 >>> 0 >= i156 >>> 0) {
      i79 = i156;
      i17 = 98;
      break;
     } else i24 = i24 + 4 | 0;
    }
   }
  } while (0);
  if ((i17 | 0) == 98) {
   i17 = 0;
   _pad(i1, 32, i46, i79, i32 ^ 8192);
   i10 = i54;
   i2 = i13;
   i11 = (i46 | 0) > (i79 | 0) ? i46 : i79;
   i12 = i45;
   continue;
  }
  if ((i17 | 0) == 77) {
   i17 = 0;
   i27 = (i64 | 0) > -1 ? i63 & -65537 : i63;
   i24 = (HEAP32[i6 >> 2] | 0) != 0 | (HEAP32[i6 + 4 >> 2] | 0) != 0;
   if ((i64 | 0) != 0 | i24) {
    i38 = (i24 & 1 ^ 1) + (i7 - i62) | 0;
    i71 = i62;
    i72 = i27;
    i73 = (i64 | 0) > (i38 | 0) ? i64 : i38;
    i74 = i65;
    i75 = i66;
    i76 = i7;
   } else {
    i71 = i7;
    i72 = i27;
    i73 = 0;
    i74 = i65;
    i75 = i66;
    i76 = i7;
   }
  }
  i27 = i76 - i71 | 0;
  i38 = (i73 | 0) < (i27 | 0) ? i27 : i73;
  i24 = i74 + i38 | 0;
  i14 = (i46 | 0) < (i24 | 0) ? i24 : i46;
  _pad(i1, 32, i14, i24, i72);
  if (!(HEAP32[i1 >> 2] & 32)) ___fwritex(i75, i74, i1) | 0;
  _pad(i1, 48, i14, i24, i72 ^ 65536);
  _pad(i1, 48, i38, i27, 0);
  if (!(HEAP32[i1 >> 2] & 32)) ___fwritex(i71, i27, i1) | 0;
  _pad(i1, 32, i14, i24, i72 ^ 8192);
  i10 = i54;
  i2 = i13;
  i11 = i14;
  i12 = i45;
 }
 L348 : do if ((i17 | 0) == 245) if (!i1) if (i16) {
  i45 = 1;
  while (1) {
   i12 = HEAP32[i5 + (i45 << 2) >> 2] | 0;
   if (!i12) {
    i158 = i45;
    break;
   }
   _pop_arg555(i4 + (i45 << 3) | 0, i12, i3);
   i45 = i45 + 1 | 0;
   if ((i45 | 0) >= 10) {
    i42 = 1;
    break L348;
   }
  }
  if ((i158 | 0) < 10) {
   i45 = i158;
   while (1) {
    if (HEAP32[i5 + (i45 << 2) >> 2] | 0) {
     i42 = -1;
     break L348;
    }
    i45 = i45 + 1 | 0;
    if ((i45 | 0) >= 10) {
     i42 = 1;
     break;
    }
   }
  } else i42 = 1;
 } else i42 = 0; else i42 = i15; while (0);
 STACKTOP = i6;
 return i42 | 0;
}

function _deflate(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0, i24 = 0, i25 = 0, i26 = 0, i27 = 0, i28 = 0, i29 = 0, i30 = 0, i31 = 0, i32 = 0, i33 = 0, i34 = 0, i35 = 0, i36 = 0, i37 = 0, i38 = 0, i39 = 0, i40 = 0, i41 = 0, i42 = 0, i43 = 0, i44 = 0, i45 = 0, i46 = 0, i47 = 0, i48 = 0, i49 = 0, i50 = 0, i51 = 0, i52 = 0, i53 = 0, i54 = 0, i55 = 0, i56 = 0, i57 = 0, i58 = 0;
 if (!i1) {
  i3 = -2;
  return i3 | 0;
 }
 i4 = HEAP32[i1 + 28 >> 2] | 0;
 if (i2 >>> 0 > 5 | (i4 | 0) == 0) {
  i3 = -2;
  return i3 | 0;
 }
 do if (HEAP32[i1 + 12 >> 2] | 0) {
  if ((HEAP32[i1 >> 2] | 0) == 0 ? (HEAP32[i1 + 4 >> 2] | 0) != 0 : 0) break;
  i5 = HEAP32[i4 + 4 >> 2] | 0;
  if (!((i2 | 0) != 4 & (i5 | 0) == 666)) {
   if (!(HEAP32[i1 + 16 >> 2] | 0)) {
    HEAP32[i1 + 24 >> 2] = HEAP32[193];
    i3 = -5;
    return i3 | 0;
   }
   HEAP32[i4 >> 2] = i1;
   i6 = HEAP32[i4 + 40 >> 2] | 0;
   HEAP32[i4 + 40 >> 2] = i2;
   do if ((i5 | 0) == 42) {
    if ((HEAP32[i4 + 24 >> 2] | 0) != 2) {
     i7 = (HEAP32[i4 + 48 >> 2] << 12) + -30720 | 0;
     if ((HEAP32[i4 + 136 >> 2] | 0) <= 1 ? (i8 = HEAP32[i4 + 132 >> 2] | 0, (i8 | 0) >= 2) : 0) if ((i8 | 0) < 6) i9 = 64; else i9 = (i8 | 0) == 6 ? 128 : 192; else i9 = 0;
     i8 = i9 | i7;
     i7 = (HEAP32[i4 + 108 >> 2] | 0) == 0 ? i8 : i8 | 32;
     HEAP32[i4 + 4 >> 2] = 113;
     i8 = HEAP32[i4 + 20 >> 2] | 0;
     HEAP32[i4 + 20 >> 2] = i8 + 1;
     HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i8 >> 0] = i7 >>> 8;
     i8 = HEAP32[i4 + 20 >> 2] | 0;
     HEAP32[i4 + 20 >> 2] = i8 + 1;
     HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i8 >> 0] = (i7 | ((i7 >>> 0) % 31 | 0)) ^ 31;
     if (HEAP32[i4 + 108 >> 2] | 0) {
      i7 = HEAP32[i1 + 48 >> 2] | 0;
      i8 = HEAP32[i4 + 20 >> 2] | 0;
      HEAP32[i4 + 20 >> 2] = i8 + 1;
      HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i8 >> 0] = i7 >>> 24;
      i8 = HEAP32[i4 + 20 >> 2] | 0;
      HEAP32[i4 + 20 >> 2] = i8 + 1;
      HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i8 >> 0] = i7 >>> 16;
      i7 = HEAP32[i1 + 48 >> 2] | 0;
      i8 = HEAP32[i4 + 20 >> 2] | 0;
      HEAP32[i4 + 20 >> 2] = i8 + 1;
      HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i8 >> 0] = i7 >>> 8;
      i8 = HEAP32[i4 + 20 >> 2] | 0;
      HEAP32[i4 + 20 >> 2] = i8 + 1;
      HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i8 >> 0] = i7;
     }
     HEAP32[i1 + 48 >> 2] = _adler32(0, 0, 0) | 0;
     i10 = HEAP32[i4 + 4 >> 2] | 0;
     i11 = 30;
     break;
    }
    HEAP32[i1 + 48 >> 2] = _crc32(0, 0, 0) | 0;
    i7 = HEAP32[i4 + 20 >> 2] | 0;
    HEAP32[i4 + 20 >> 2] = i7 + 1;
    HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i7 >> 0] = 31;
    i7 = HEAP32[i4 + 20 >> 2] | 0;
    HEAP32[i4 + 20 >> 2] = i7 + 1;
    HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i7 >> 0] = -117;
    i7 = HEAP32[i4 + 20 >> 2] | 0;
    HEAP32[i4 + 20 >> 2] = i7 + 1;
    HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i7 >> 0] = 8;
    i7 = HEAP32[i4 + 28 >> 2] | 0;
    if (!i7) {
     i8 = HEAP32[i4 + 20 >> 2] | 0;
     HEAP32[i4 + 20 >> 2] = i8 + 1;
     HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i8 >> 0] = 0;
     i8 = HEAP32[i4 + 20 >> 2] | 0;
     HEAP32[i4 + 20 >> 2] = i8 + 1;
     HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i8 >> 0] = 0;
     i8 = HEAP32[i4 + 20 >> 2] | 0;
     HEAP32[i4 + 20 >> 2] = i8 + 1;
     HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i8 >> 0] = 0;
     i8 = HEAP32[i4 + 20 >> 2] | 0;
     HEAP32[i4 + 20 >> 2] = i8 + 1;
     HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i8 >> 0] = 0;
     i8 = HEAP32[i4 + 20 >> 2] | 0;
     HEAP32[i4 + 20 >> 2] = i8 + 1;
     HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i8 >> 0] = 0;
     i8 = HEAP32[i4 + 132 >> 2] | 0;
     if ((i8 | 0) == 9) i12 = 2; else i12 = ((i8 | 0) < 2 ? 1 : (HEAP32[i4 + 136 >> 2] | 0) > 1) ? 4 : 0;
     i8 = HEAP32[i4 + 20 >> 2] | 0;
     HEAP32[i4 + 20 >> 2] = i8 + 1;
     HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i8 >> 0] = i12;
     i8 = HEAP32[i4 + 20 >> 2] | 0;
     HEAP32[i4 + 20 >> 2] = i8 + 1;
     HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i8 >> 0] = 3;
     HEAP32[i4 + 4 >> 2] = 113;
     break;
    }
    i8 = (((HEAP32[i7 + 44 >> 2] | 0) != 0 ? 2 : 0) | (HEAP32[i7 >> 2] | 0) != 0 | ((HEAP32[i7 + 16 >> 2] | 0) == 0 ? 0 : 4) | ((HEAP32[i7 + 28 >> 2] | 0) == 0 ? 0 : 8) | ((HEAP32[i7 + 36 >> 2] | 0) == 0 ? 0 : 16)) & 255;
    i7 = HEAP32[i4 + 20 >> 2] | 0;
    HEAP32[i4 + 20 >> 2] = i7 + 1;
    HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i7 >> 0] = i8;
    i8 = HEAP32[(HEAP32[i4 + 28 >> 2] | 0) + 4 >> 2] & 255;
    i7 = HEAP32[i4 + 20 >> 2] | 0;
    HEAP32[i4 + 20 >> 2] = i7 + 1;
    HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i7 >> 0] = i8;
    i8 = (HEAP32[(HEAP32[i4 + 28 >> 2] | 0) + 4 >> 2] | 0) >>> 8 & 255;
    i7 = HEAP32[i4 + 20 >> 2] | 0;
    HEAP32[i4 + 20 >> 2] = i7 + 1;
    HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i7 >> 0] = i8;
    i8 = (HEAP32[(HEAP32[i4 + 28 >> 2] | 0) + 4 >> 2] | 0) >>> 16 & 255;
    i7 = HEAP32[i4 + 20 >> 2] | 0;
    HEAP32[i4 + 20 >> 2] = i7 + 1;
    HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i7 >> 0] = i8;
    i8 = (HEAP32[(HEAP32[i4 + 28 >> 2] | 0) + 4 >> 2] | 0) >>> 24 & 255;
    i7 = HEAP32[i4 + 20 >> 2] | 0;
    HEAP32[i4 + 20 >> 2] = i7 + 1;
    HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i7 >> 0] = i8;
    i8 = HEAP32[i4 + 132 >> 2] | 0;
    if ((i8 | 0) == 9) i13 = 2; else i13 = ((i8 | 0) < 2 ? 1 : (HEAP32[i4 + 136 >> 2] | 0) > 1) ? 4 : 0;
    i8 = HEAP32[i4 + 20 >> 2] | 0;
    HEAP32[i4 + 20 >> 2] = i8 + 1;
    HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i8 >> 0] = i13;
    i8 = HEAP32[(HEAP32[i4 + 28 >> 2] | 0) + 12 >> 2] & 255;
    i7 = HEAP32[i4 + 20 >> 2] | 0;
    HEAP32[i4 + 20 >> 2] = i7 + 1;
    HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i7 >> 0] = i8;
    i8 = HEAP32[i4 + 28 >> 2] | 0;
    if (!(HEAP32[i8 + 16 >> 2] | 0)) i14 = i8; else {
     i7 = HEAP32[i8 + 20 >> 2] & 255;
     i8 = HEAP32[i4 + 20 >> 2] | 0;
     HEAP32[i4 + 20 >> 2] = i8 + 1;
     HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i8 >> 0] = i7;
     i7 = (HEAP32[(HEAP32[i4 + 28 >> 2] | 0) + 20 >> 2] | 0) >>> 8 & 255;
     i8 = HEAP32[i4 + 20 >> 2] | 0;
     HEAP32[i4 + 20 >> 2] = i8 + 1;
     HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i8 >> 0] = i7;
     i14 = HEAP32[i4 + 28 >> 2] | 0;
    }
    if (HEAP32[i14 + 44 >> 2] | 0) HEAP32[i1 + 48 >> 2] = _crc32(HEAP32[i1 + 48 >> 2] | 0, HEAP32[i4 + 8 >> 2] | 0, HEAP32[i4 + 20 >> 2] | 0) | 0;
    HEAP32[i4 + 32 >> 2] = 0;
    HEAP32[i4 + 4 >> 2] = 69;
    i15 = i4 + 28 | 0;
    i11 = 32;
   } else {
    i10 = i5;
    i11 = 30;
   } while (0);
   if ((i11 | 0) == 30) if ((i10 | 0) == 69) {
    i15 = i4 + 28 | 0;
    i11 = 32;
   } else {
    i16 = i10;
    i11 = 53;
   }
   do if ((i11 | 0) == 32) {
    i5 = HEAP32[i15 >> 2] | 0;
    if (!(HEAP32[i5 + 16 >> 2] | 0)) {
     HEAP32[i4 + 4 >> 2] = 73;
     i17 = i15;
     i18 = i5;
     i11 = 55;
     break;
    }
    i7 = HEAP32[i4 + 20 >> 2] | 0;
    i8 = HEAP32[i4 + 32 >> 2] | 0;
    L53 : do if (i8 >>> 0 < (HEAP32[i5 + 20 >> 2] & 65535) >>> 0) {
     i19 = i8;
     i20 = i7;
     i21 = i5;
     i22 = i7;
     while (1) {
      if ((i20 | 0) == (HEAP32[i4 + 12 >> 2] | 0)) {
       if (i20 >>> 0 > i22 >>> 0 & (HEAP32[i21 + 44 >> 2] | 0) != 0) HEAP32[i1 + 48 >> 2] = _crc32(HEAP32[i1 + 48 >> 2] | 0, (HEAP32[i4 + 8 >> 2] | 0) + i22 | 0, i20 - i22 | 0) | 0;
       i23 = HEAP32[i1 + 28 >> 2] | 0;
       i24 = HEAP32[i23 + 20 >> 2] | 0;
       i25 = HEAP32[i1 + 16 >> 2] | 0;
       i26 = i24 >>> 0 > i25 >>> 0 ? i25 : i24;
       if ((i26 | 0) != 0 ? (_memcpy(HEAP32[i1 + 12 >> 2] | 0, HEAP32[i23 + 16 >> 2] | 0, i26 | 0) | 0, HEAP32[i1 + 12 >> 2] = (HEAP32[i1 + 12 >> 2] | 0) + i26, i23 = HEAP32[i1 + 28 >> 2] | 0, HEAP32[i23 + 16 >> 2] = (HEAP32[i23 + 16 >> 2] | 0) + i26, HEAP32[i1 + 20 >> 2] = (HEAP32[i1 + 20 >> 2] | 0) + i26, HEAP32[i1 + 16 >> 2] = (HEAP32[i1 + 16 >> 2] | 0) - i26, i24 = HEAP32[i23 + 20 >> 2] | 0, HEAP32[i23 + 20 >> 2] = i24 - i26, (i24 | 0) == (i26 | 0)) : 0) HEAP32[i23 + 16 >> 2] = HEAP32[i23 + 8 >> 2];
       i27 = HEAP32[i4 + 20 >> 2] | 0;
       if ((i27 | 0) == (HEAP32[i4 + 12 >> 2] | 0)) break;
       i28 = HEAP32[i15 >> 2] | 0;
       i29 = HEAP32[i4 + 32 >> 2] | 0;
       i30 = i27;
       i31 = i27;
      } else {
       i28 = i21;
       i29 = i19;
       i30 = i20;
       i31 = i22;
      }
      i23 = HEAP8[(HEAP32[i28 + 16 >> 2] | 0) + i29 >> 0] | 0;
      HEAP32[i4 + 20 >> 2] = i30 + 1;
      HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i30 >> 0] = i23;
      i23 = (HEAP32[i4 + 32 >> 2] | 0) + 1 | 0;
      HEAP32[i4 + 32 >> 2] = i23;
      i26 = HEAP32[i15 >> 2] | 0;
      if (i23 >>> 0 >= (HEAP32[i26 + 20 >> 2] & 65535) >>> 0) {
       i32 = i26;
       i33 = i31;
       break L53;
      }
      i19 = i23;
      i20 = HEAP32[i4 + 20 >> 2] | 0;
      i21 = i26;
      i22 = i31;
     }
     i32 = HEAP32[i15 >> 2] | 0;
     i33 = i27;
    } else {
     i32 = i5;
     i33 = i7;
    } while (0);
    if ((HEAP32[i32 + 44 >> 2] | 0) != 0 ? (i7 = HEAP32[i4 + 20 >> 2] | 0, i7 >>> 0 > i33 >>> 0) : 0) {
     HEAP32[i1 + 48 >> 2] = _crc32(HEAP32[i1 + 48 >> 2] | 0, (HEAP32[i4 + 8 >> 2] | 0) + i33 | 0, i7 - i33 | 0) | 0;
     i34 = HEAP32[i15 >> 2] | 0;
    } else i34 = i32;
    if ((HEAP32[i4 + 32 >> 2] | 0) == (HEAP32[i34 + 20 >> 2] | 0)) {
     HEAP32[i4 + 32 >> 2] = 0;
     HEAP32[i4 + 4 >> 2] = 73;
     i17 = i15;
     i18 = i34;
     i11 = 55;
     break;
    } else {
     i16 = HEAP32[i4 + 4 >> 2] | 0;
     i11 = 53;
     break;
    }
   } while (0);
   if ((i11 | 0) == 53) if ((i16 | 0) == 73) {
    i17 = i4 + 28 | 0;
    i18 = HEAP32[i4 + 28 >> 2] | 0;
    i11 = 55;
   } else {
    i35 = i16;
    i11 = 73;
   }
   do if ((i11 | 0) == 55) {
    if (!(HEAP32[i18 + 28 >> 2] | 0)) {
     HEAP32[i4 + 4 >> 2] = 91;
     i36 = i17;
     i11 = 75;
     break;
    }
    i7 = HEAP32[i4 + 20 >> 2] | 0;
    i5 = i7;
    i8 = i7;
    while (1) {
     if ((i5 | 0) == (HEAP32[i4 + 12 >> 2] | 0)) {
      if (i5 >>> 0 > i8 >>> 0 ? (HEAP32[(HEAP32[i17 >> 2] | 0) + 44 >> 2] | 0) != 0 : 0) HEAP32[i1 + 48 >> 2] = _crc32(HEAP32[i1 + 48 >> 2] | 0, (HEAP32[i4 + 8 >> 2] | 0) + i8 | 0, i5 - i8 | 0) | 0;
      i7 = HEAP32[i1 + 28 >> 2] | 0;
      i22 = HEAP32[i7 + 20 >> 2] | 0;
      i21 = HEAP32[i1 + 16 >> 2] | 0;
      i20 = i22 >>> 0 > i21 >>> 0 ? i21 : i22;
      if ((i20 | 0) != 0 ? (_memcpy(HEAP32[i1 + 12 >> 2] | 0, HEAP32[i7 + 16 >> 2] | 0, i20 | 0) | 0, HEAP32[i1 + 12 >> 2] = (HEAP32[i1 + 12 >> 2] | 0) + i20, i7 = HEAP32[i1 + 28 >> 2] | 0, HEAP32[i7 + 16 >> 2] = (HEAP32[i7 + 16 >> 2] | 0) + i20, HEAP32[i1 + 20 >> 2] = (HEAP32[i1 + 20 >> 2] | 0) + i20, HEAP32[i1 + 16 >> 2] = (HEAP32[i1 + 16 >> 2] | 0) - i20, i22 = HEAP32[i7 + 20 >> 2] | 0, HEAP32[i7 + 20 >> 2] = i22 - i20, (i22 | 0) == (i20 | 0)) : 0) HEAP32[i7 + 16 >> 2] = HEAP32[i7 + 8 >> 2];
      i7 = HEAP32[i4 + 20 >> 2] | 0;
      if ((i7 | 0) == (HEAP32[i4 + 12 >> 2] | 0)) {
       i37 = i7;
       i38 = 1;
       break;
      } else {
       i39 = i7;
       i40 = i7;
      }
     } else {
      i39 = i5;
      i40 = i8;
     }
     i7 = HEAP32[i4 + 32 >> 2] | 0;
     HEAP32[i4 + 32 >> 2] = i7 + 1;
     i20 = HEAP8[(HEAP32[(HEAP32[i17 >> 2] | 0) + 28 >> 2] | 0) + i7 >> 0] | 0;
     HEAP32[i4 + 20 >> 2] = i39 + 1;
     HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i39 >> 0] = i20;
     if (!(i20 << 24 >> 24)) {
      i37 = i40;
      i38 = i20 & 255;
      break;
     }
     i5 = HEAP32[i4 + 20 >> 2] | 0;
     i8 = i40;
    }
    if ((HEAP32[(HEAP32[i17 >> 2] | 0) + 44 >> 2] | 0) != 0 ? (i8 = HEAP32[i4 + 20 >> 2] | 0, i8 >>> 0 > i37 >>> 0) : 0) HEAP32[i1 + 48 >> 2] = _crc32(HEAP32[i1 + 48 >> 2] | 0, (HEAP32[i4 + 8 >> 2] | 0) + i37 | 0, i8 - i37 | 0) | 0;
    if (!i38) {
     HEAP32[i4 + 32 >> 2] = 0;
     HEAP32[i4 + 4 >> 2] = 91;
     i36 = i17;
     i11 = 75;
     break;
    } else {
     i35 = HEAP32[i4 + 4 >> 2] | 0;
     i11 = 73;
     break;
    }
   } while (0);
   if ((i11 | 0) == 73) if ((i35 | 0) == 91) {
    i36 = i4 + 28 | 0;
    i11 = 75;
   } else {
    i41 = i35;
    i11 = 93;
   }
   do if ((i11 | 0) == 75) {
    if (!(HEAP32[(HEAP32[i36 >> 2] | 0) + 36 >> 2] | 0)) {
     HEAP32[i4 + 4 >> 2] = 103;
     i42 = i36;
     i11 = 95;
     break;
    }
    i8 = HEAP32[i4 + 20 >> 2] | 0;
    i5 = i8;
    i20 = i8;
    while (1) {
     if ((i5 | 0) == (HEAP32[i4 + 12 >> 2] | 0)) {
      if (i5 >>> 0 > i20 >>> 0 ? (HEAP32[(HEAP32[i36 >> 2] | 0) + 44 >> 2] | 0) != 0 : 0) HEAP32[i1 + 48 >> 2] = _crc32(HEAP32[i1 + 48 >> 2] | 0, (HEAP32[i4 + 8 >> 2] | 0) + i20 | 0, i5 - i20 | 0) | 0;
      i8 = HEAP32[i1 + 28 >> 2] | 0;
      i7 = HEAP32[i8 + 20 >> 2] | 0;
      i22 = HEAP32[i1 + 16 >> 2] | 0;
      i21 = i7 >>> 0 > i22 >>> 0 ? i22 : i7;
      if ((i21 | 0) != 0 ? (_memcpy(HEAP32[i1 + 12 >> 2] | 0, HEAP32[i8 + 16 >> 2] | 0, i21 | 0) | 0, HEAP32[i1 + 12 >> 2] = (HEAP32[i1 + 12 >> 2] | 0) + i21, i8 = HEAP32[i1 + 28 >> 2] | 0, HEAP32[i8 + 16 >> 2] = (HEAP32[i8 + 16 >> 2] | 0) + i21, HEAP32[i1 + 20 >> 2] = (HEAP32[i1 + 20 >> 2] | 0) + i21, HEAP32[i1 + 16 >> 2] = (HEAP32[i1 + 16 >> 2] | 0) - i21, i7 = HEAP32[i8 + 20 >> 2] | 0, HEAP32[i8 + 20 >> 2] = i7 - i21, (i7 | 0) == (i21 | 0)) : 0) HEAP32[i8 + 16 >> 2] = HEAP32[i8 + 8 >> 2];
      i8 = HEAP32[i4 + 20 >> 2] | 0;
      if ((i8 | 0) == (HEAP32[i4 + 12 >> 2] | 0)) {
       i43 = i8;
       i44 = 1;
       break;
      } else {
       i45 = i8;
       i46 = i8;
      }
     } else {
      i45 = i5;
      i46 = i20;
     }
     i8 = HEAP32[i4 + 32 >> 2] | 0;
     HEAP32[i4 + 32 >> 2] = i8 + 1;
     i21 = HEAP8[(HEAP32[(HEAP32[i36 >> 2] | 0) + 36 >> 2] | 0) + i8 >> 0] | 0;
     HEAP32[i4 + 20 >> 2] = i45 + 1;
     HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i45 >> 0] = i21;
     if (!(i21 << 24 >> 24)) {
      i43 = i46;
      i44 = i21 & 255;
      break;
     }
     i5 = HEAP32[i4 + 20 >> 2] | 0;
     i20 = i46;
    }
    if ((HEAP32[(HEAP32[i36 >> 2] | 0) + 44 >> 2] | 0) != 0 ? (i20 = HEAP32[i4 + 20 >> 2] | 0, i20 >>> 0 > i43 >>> 0) : 0) HEAP32[i1 + 48 >> 2] = _crc32(HEAP32[i1 + 48 >> 2] | 0, (HEAP32[i4 + 8 >> 2] | 0) + i43 | 0, i20 - i43 | 0) | 0;
    if (!i44) {
     HEAP32[i4 + 4 >> 2] = 103;
     i42 = i36;
     i11 = 95;
     break;
    } else {
     i41 = HEAP32[i4 + 4 >> 2] | 0;
     i11 = 93;
     break;
    }
   } while (0);
   if ((i11 | 0) == 93 ? (i41 | 0) == 103 : 0) {
    i42 = i4 + 28 | 0;
    i11 = 95;
   }
   do if ((i11 | 0) == 95) {
    if (!(HEAP32[(HEAP32[i42 >> 2] | 0) + 44 >> 2] | 0)) {
     HEAP32[i4 + 4 >> 2] = 113;
     break;
    }
    if ((((HEAP32[i4 + 20 >> 2] | 0) + 2 | 0) >>> 0 > (HEAP32[i4 + 12 >> 2] | 0) >>> 0 ? (i20 = HEAP32[i1 + 28 >> 2] | 0, i5 = HEAP32[i20 + 20 >> 2] | 0, i21 = HEAP32[i1 + 16 >> 2] | 0, i8 = i5 >>> 0 > i21 >>> 0 ? i21 : i5, (i8 | 0) != 0) : 0) ? (_memcpy(HEAP32[i1 + 12 >> 2] | 0, HEAP32[i20 + 16 >> 2] | 0, i8 | 0) | 0, HEAP32[i1 + 12 >> 2] = (HEAP32[i1 + 12 >> 2] | 0) + i8, i20 = HEAP32[i1 + 28 >> 2] | 0, HEAP32[i20 + 16 >> 2] = (HEAP32[i20 + 16 >> 2] | 0) + i8, HEAP32[i1 + 20 >> 2] = (HEAP32[i1 + 20 >> 2] | 0) + i8, HEAP32[i1 + 16 >> 2] = (HEAP32[i1 + 16 >> 2] | 0) - i8, i5 = HEAP32[i20 + 20 >> 2] | 0, HEAP32[i20 + 20 >> 2] = i5 - i8, (i5 | 0) == (i8 | 0)) : 0) HEAP32[i20 + 16 >> 2] = HEAP32[i20 + 8 >> 2];
    i20 = HEAP32[i4 + 20 >> 2] | 0;
    if ((i20 + 2 | 0) >>> 0 <= (HEAP32[i4 + 12 >> 2] | 0) >>> 0) {
     i8 = HEAP32[i1 + 48 >> 2] & 255;
     HEAP32[i4 + 20 >> 2] = i20 + 1;
     HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i20 >> 0] = i8;
     i8 = (HEAP32[i1 + 48 >> 2] | 0) >>> 8 & 255;
     i20 = HEAP32[i4 + 20 >> 2] | 0;
     HEAP32[i4 + 20 >> 2] = i20 + 1;
     HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i20 >> 0] = i8;
     HEAP32[i1 + 48 >> 2] = _crc32(0, 0, 0) | 0;
     HEAP32[i4 + 4 >> 2] = 113;
    }
   } while (0);
   if (!(HEAP32[i4 + 20 >> 2] | 0)) {
    if ((i2 | 0) != 4 & (i6 | 0) >= (i2 | 0) & (HEAP32[i1 + 4 >> 2] | 0) == 0) {
     HEAP32[i1 + 24 >> 2] = HEAP32[193];
     i3 = -5;
     return i3 | 0;
    }
   } else {
    i8 = HEAP32[i1 + 28 >> 2] | 0;
    i20 = HEAP32[i8 + 20 >> 2] | 0;
    i5 = HEAP32[i1 + 16 >> 2] | 0;
    i21 = i20 >>> 0 > i5 >>> 0 ? i5 : i20;
    if (i21) {
     _memcpy(HEAP32[i1 + 12 >> 2] | 0, HEAP32[i8 + 16 >> 2] | 0, i21 | 0) | 0;
     HEAP32[i1 + 12 >> 2] = (HEAP32[i1 + 12 >> 2] | 0) + i21;
     i8 = HEAP32[i1 + 28 >> 2] | 0;
     HEAP32[i8 + 16 >> 2] = (HEAP32[i8 + 16 >> 2] | 0) + i21;
     HEAP32[i1 + 20 >> 2] = (HEAP32[i1 + 20 >> 2] | 0) + i21;
     i20 = (HEAP32[i1 + 16 >> 2] | 0) - i21 | 0;
     HEAP32[i1 + 16 >> 2] = i20;
     i7 = HEAP32[i8 + 20 >> 2] | 0;
     HEAP32[i8 + 20 >> 2] = i7 - i21;
     if ((i7 | 0) == (i21 | 0)) {
      HEAP32[i8 + 16 >> 2] = HEAP32[i8 + 8 >> 2];
      i47 = i20;
     } else i47 = i20;
    } else i47 = i5;
    if (!i47) {
     HEAP32[i4 + 40 >> 2] = -1;
     i3 = 0;
     return i3 | 0;
    }
   }
   i5 = (HEAP32[i4 + 4 >> 2] | 0) == 666;
   i20 = (HEAP32[i1 + 4 >> 2] | 0) == 0;
   if (i5) if (i20) i11 = 115; else {
    HEAP32[i1 + 24 >> 2] = HEAP32[193];
    i3 = -5;
    return i3 | 0;
   } else if (i20) i11 = 115; else i11 = 116;
   if ((i11 | 0) == 115 ? !((HEAP32[i4 + 116 >> 2] | 0) == 0 & ((i2 | 0) == 0 | i5)) : 0) i11 = 116;
   do if ((i11 | 0) == 116) {
    L172 : do switch (HEAP32[i4 + 136 >> 2] | 0) {
    case 2:
     {
      while (1) {
       if ((HEAP32[i4 + 116 >> 2] | 0) == 0 ? (_fill_window(i4), (HEAP32[i4 + 116 >> 2] | 0) == 0) : 0) break;
       HEAP32[i4 + 96 >> 2] = 0;
       i5 = HEAP8[(HEAP32[i4 + 56 >> 2] | 0) + (HEAP32[i4 + 108 >> 2] | 0) >> 0] | 0;
       i20 = HEAP32[i4 + 5792 >> 2] | 0;
       HEAP16[(HEAP32[i4 + 5796 >> 2] | 0) + (i20 << 1) >> 1] = 0;
       HEAP32[i4 + 5792 >> 2] = i20 + 1;
       HEAP8[(HEAP32[i4 + 5784 >> 2] | 0) + i20 >> 0] = i5;
       HEAP16[i4 + 148 + ((i5 & 255) << 2) >> 1] = (HEAP16[i4 + 148 + ((i5 & 255) << 2) >> 1] | 0) + 1 << 16 >> 16;
       i5 = (HEAP32[i4 + 5792 >> 2] | 0) == ((HEAP32[i4 + 5788 >> 2] | 0) + -1 | 0);
       HEAP32[i4 + 116 >> 2] = (HEAP32[i4 + 116 >> 2] | 0) + -1;
       i20 = (HEAP32[i4 + 108 >> 2] | 0) + 1 | 0;
       HEAP32[i4 + 108 >> 2] = i20;
       if (!i5) continue;
       i5 = HEAP32[i4 + 92 >> 2] | 0;
       if ((i5 | 0) > -1) i48 = (HEAP32[i4 + 56 >> 2] | 0) + i5 | 0; else i48 = 0;
       __tr_flush_block(i4, i48, i20 - i5 | 0, 0);
       HEAP32[i4 + 92 >> 2] = HEAP32[i4 + 108 >> 2];
       i5 = HEAP32[i4 >> 2] | 0;
       i20 = HEAP32[i5 + 28 >> 2] | 0;
       i8 = HEAP32[i20 + 20 >> 2] | 0;
       i21 = HEAP32[i5 + 16 >> 2] | 0;
       i7 = i8 >>> 0 > i21 >>> 0 ? i21 : i8;
       if ((i7 | 0) != 0 ? (_memcpy(HEAP32[i5 + 12 >> 2] | 0, HEAP32[i20 + 16 >> 2] | 0, i7 | 0) | 0, HEAP32[i5 + 12 >> 2] = (HEAP32[i5 + 12 >> 2] | 0) + i7, i20 = HEAP32[i5 + 28 >> 2] | 0, HEAP32[i20 + 16 >> 2] = (HEAP32[i20 + 16 >> 2] | 0) + i7, HEAP32[i5 + 20 >> 2] = (HEAP32[i5 + 20 >> 2] | 0) + i7, HEAP32[i5 + 16 >> 2] = (HEAP32[i5 + 16 >> 2] | 0) - i7, i5 = HEAP32[i20 + 20 >> 2] | 0, HEAP32[i20 + 20 >> 2] = i5 - i7, (i5 | 0) == (i7 | 0)) : 0) HEAP32[i20 + 16 >> 2] = HEAP32[i20 + 8 >> 2];
       if (!(HEAP32[(HEAP32[i4 >> 2] | 0) + 16 >> 2] | 0)) break L172;
      }
      if (i2) {
       i20 = HEAP32[i4 + 92 >> 2] | 0;
       if ((i20 | 0) > -1) i49 = (HEAP32[i4 + 56 >> 2] | 0) + i20 | 0; else i49 = 0;
       __tr_flush_block(i4, i49, (HEAP32[i4 + 108 >> 2] | 0) - i20 | 0, (i2 | 0) == 4 & 1);
       HEAP32[i4 + 92 >> 2] = HEAP32[i4 + 108 >> 2];
       i20 = HEAP32[i4 >> 2] | 0;
       i7 = HEAP32[i20 + 28 >> 2] | 0;
       i5 = HEAP32[i7 + 20 >> 2] | 0;
       i8 = HEAP32[i20 + 16 >> 2] | 0;
       i21 = i5 >>> 0 > i8 >>> 0 ? i8 : i5;
       if ((i21 | 0) != 0 ? (_memcpy(HEAP32[i20 + 12 >> 2] | 0, HEAP32[i7 + 16 >> 2] | 0, i21 | 0) | 0, HEAP32[i20 + 12 >> 2] = (HEAP32[i20 + 12 >> 2] | 0) + i21, i7 = HEAP32[i20 + 28 >> 2] | 0, HEAP32[i7 + 16 >> 2] = (HEAP32[i7 + 16 >> 2] | 0) + i21, HEAP32[i20 + 20 >> 2] = (HEAP32[i20 + 20 >> 2] | 0) + i21, HEAP32[i20 + 16 >> 2] = (HEAP32[i20 + 16 >> 2] | 0) - i21, i20 = HEAP32[i7 + 20 >> 2] | 0, HEAP32[i7 + 20 >> 2] = i20 - i21, (i20 | 0) == (i21 | 0)) : 0) HEAP32[i7 + 16 >> 2] = HEAP32[i7 + 8 >> 2];
       if (!(HEAP32[(HEAP32[i4 >> 2] | 0) + 16 >> 2] | 0)) {
        i50 = (i2 | 0) == 4 ? 2 : 0;
        i11 = 175;
        break L172;
       } else {
        i50 = (i2 | 0) == 4 ? 3 : 1;
        i11 = 175;
        break L172;
       }
      }
      break;
     }
    case 3:
     {
      i7 = i4 + 2440 + (HEAPU8[13462] << 2) | 0;
      while (1) {
       i21 = HEAP32[i4 + 116 >> 2] | 0;
       if (i21 >>> 0 < 258) {
        _fill_window(i4);
        i20 = HEAP32[i4 + 116 >> 2] | 0;
        if ((i2 | 0) == 0 & i20 >>> 0 < 258) break L172;
        if (!i20) break;
        HEAP32[i4 + 96 >> 2] = 0;
        if (i20 >>> 0 > 2) {
         i51 = i20;
         i11 = 143;
        } else {
         i52 = HEAP32[i4 + 108 >> 2] | 0;
         i11 = 158;
        }
       } else {
        HEAP32[i4 + 96 >> 2] = 0;
        i51 = i21;
        i11 = 143;
       }
       if ((i11 | 0) == 143) {
        i11 = 0;
        i21 = HEAP32[i4 + 108 >> 2] | 0;
        if (i21) {
         i20 = HEAP32[i4 + 56 >> 2] | 0;
         i5 = HEAP8[i20 + (i21 + -1) >> 0] | 0;
         if ((i5 << 24 >> 24 == (HEAP8[i20 + i21 >> 0] | 0) ? i5 << 24 >> 24 == (HEAP8[i20 + (i21 + 1) >> 0] | 0) : 0) ? i5 << 24 >> 24 == (HEAP8[i20 + (i21 + 2) >> 0] | 0) : 0) {
          i8 = i20 + (i21 + 2) | 0;
          while (1) {
           i22 = i8 + 1 | 0;
           if (i5 << 24 >> 24 != (HEAP8[i22 >> 0] | 0)) {
            i53 = i22;
            break;
           }
           i22 = i8 + 2 | 0;
           if (i5 << 24 >> 24 != (HEAP8[i22 >> 0] | 0)) {
            i53 = i22;
            break;
           }
           i22 = i8 + 3 | 0;
           if (i5 << 24 >> 24 != (HEAP8[i22 >> 0] | 0)) {
            i53 = i22;
            break;
           }
           i22 = i8 + 4 | 0;
           if (i5 << 24 >> 24 != (HEAP8[i22 >> 0] | 0)) {
            i53 = i22;
            break;
           }
           i22 = i8 + 5 | 0;
           if (i5 << 24 >> 24 != (HEAP8[i22 >> 0] | 0)) {
            i53 = i22;
            break;
           }
           i22 = i8 + 6 | 0;
           if (i5 << 24 >> 24 != (HEAP8[i22 >> 0] | 0)) {
            i53 = i22;
            break;
           }
           i22 = i8 + 7 | 0;
           if (i5 << 24 >> 24 != (HEAP8[i22 >> 0] | 0)) {
            i53 = i22;
            break;
           }
           i22 = i8 + 8 | 0;
           if (i22 >>> 0 < (i20 + (i21 + 258) | 0) >>> 0 ? i5 << 24 >> 24 == (HEAP8[i22 >> 0] | 0) : 0) i8 = i22; else {
            i53 = i22;
            break;
           }
          }
          i8 = i53 - (i20 + (i21 + 258)) + 258 | 0;
          i5 = i8 >>> 0 > i51 >>> 0 ? i51 : i8;
          HEAP32[i4 + 96 >> 2] = i5;
          if (i5 >>> 0 > 2) {
           i8 = HEAP32[i4 + 5792 >> 2] | 0;
           HEAP16[(HEAP32[i4 + 5796 >> 2] | 0) + (i8 << 1) >> 1] = 1;
           HEAP32[i4 + 5792 >> 2] = i8 + 1;
           HEAP8[(HEAP32[i4 + 5784 >> 2] | 0) + i8 >> 0] = i5 + 253;
           i8 = i4 + 148 + ((HEAPU8[13974 + (i5 + 253 & 255) >> 0] | 256) + 1 << 2) | 0;
           HEAP16[i8 >> 1] = (HEAP16[i8 >> 1] | 0) + 1 << 16 >> 16;
           HEAP16[i7 >> 1] = (HEAP16[i7 >> 1] | 0) + 1 << 16 >> 16;
           i8 = (HEAP32[i4 + 5792 >> 2] | 0) == ((HEAP32[i4 + 5788 >> 2] | 0) + -1 | 0) & 1;
           i5 = HEAP32[i4 + 96 >> 2] | 0;
           HEAP32[i4 + 116 >> 2] = (HEAP32[i4 + 116 >> 2] | 0) - i5;
           i22 = (HEAP32[i4 + 108 >> 2] | 0) + i5 | 0;
           HEAP32[i4 + 108 >> 2] = i22;
           HEAP32[i4 + 96 >> 2] = 0;
           i54 = i22;
           i55 = i8;
          } else {
           i52 = i21;
           i11 = 158;
          }
         } else {
          i52 = i21;
          i11 = 158;
         }
        } else {
         i52 = 0;
         i11 = 158;
        }
       }
       if ((i11 | 0) == 158) {
        i11 = 0;
        i8 = HEAP8[(HEAP32[i4 + 56 >> 2] | 0) + i52 >> 0] | 0;
        i22 = HEAP32[i4 + 5792 >> 2] | 0;
        HEAP16[(HEAP32[i4 + 5796 >> 2] | 0) + (i22 << 1) >> 1] = 0;
        HEAP32[i4 + 5792 >> 2] = i22 + 1;
        HEAP8[(HEAP32[i4 + 5784 >> 2] | 0) + i22 >> 0] = i8;
        HEAP16[i4 + 148 + ((i8 & 255) << 2) >> 1] = (HEAP16[i4 + 148 + ((i8 & 255) << 2) >> 1] | 0) + 1 << 16 >> 16;
        i8 = (HEAP32[i4 + 5792 >> 2] | 0) == ((HEAP32[i4 + 5788 >> 2] | 0) + -1 | 0) & 1;
        HEAP32[i4 + 116 >> 2] = (HEAP32[i4 + 116 >> 2] | 0) + -1;
        i22 = (HEAP32[i4 + 108 >> 2] | 0) + 1 | 0;
        HEAP32[i4 + 108 >> 2] = i22;
        i54 = i22;
        i55 = i8;
       }
       if (!i55) continue;
       i8 = HEAP32[i4 + 92 >> 2] | 0;
       if ((i8 | 0) > -1) i56 = (HEAP32[i4 + 56 >> 2] | 0) + i8 | 0; else i56 = 0;
       __tr_flush_block(i4, i56, i54 - i8 | 0, 0);
       HEAP32[i4 + 92 >> 2] = HEAP32[i4 + 108 >> 2];
       i8 = HEAP32[i4 >> 2] | 0;
       i22 = HEAP32[i8 + 28 >> 2] | 0;
       i5 = HEAP32[i22 + 20 >> 2] | 0;
       i19 = HEAP32[i8 + 16 >> 2] | 0;
       i26 = i5 >>> 0 > i19 >>> 0 ? i19 : i5;
       if ((i26 | 0) != 0 ? (_memcpy(HEAP32[i8 + 12 >> 2] | 0, HEAP32[i22 + 16 >> 2] | 0, i26 | 0) | 0, HEAP32[i8 + 12 >> 2] = (HEAP32[i8 + 12 >> 2] | 0) + i26, i22 = HEAP32[i8 + 28 >> 2] | 0, HEAP32[i22 + 16 >> 2] = (HEAP32[i22 + 16 >> 2] | 0) + i26, HEAP32[i8 + 20 >> 2] = (HEAP32[i8 + 20 >> 2] | 0) + i26, HEAP32[i8 + 16 >> 2] = (HEAP32[i8 + 16 >> 2] | 0) - i26, i8 = HEAP32[i22 + 20 >> 2] | 0, HEAP32[i22 + 20 >> 2] = i8 - i26, (i8 | 0) == (i26 | 0)) : 0) HEAP32[i22 + 16 >> 2] = HEAP32[i22 + 8 >> 2];
       if (!(HEAP32[(HEAP32[i4 >> 2] | 0) + 16 >> 2] | 0)) break L172;
      }
      i7 = HEAP32[i4 + 92 >> 2] | 0;
      if ((i7 | 0) > -1) i57 = (HEAP32[i4 + 56 >> 2] | 0) + i7 | 0; else i57 = 0;
      __tr_flush_block(i4, i57, (HEAP32[i4 + 108 >> 2] | 0) - i7 | 0, (i2 | 0) == 4 & 1);
      HEAP32[i4 + 92 >> 2] = HEAP32[i4 + 108 >> 2];
      i7 = HEAP32[i4 >> 2] | 0;
      i22 = HEAP32[i7 + 28 >> 2] | 0;
      i26 = HEAP32[i22 + 20 >> 2] | 0;
      i8 = HEAP32[i7 + 16 >> 2] | 0;
      i5 = i26 >>> 0 > i8 >>> 0 ? i8 : i26;
      if ((i5 | 0) != 0 ? (_memcpy(HEAP32[i7 + 12 >> 2] | 0, HEAP32[i22 + 16 >> 2] | 0, i5 | 0) | 0, HEAP32[i7 + 12 >> 2] = (HEAP32[i7 + 12 >> 2] | 0) + i5, i22 = HEAP32[i7 + 28 >> 2] | 0, HEAP32[i22 + 16 >> 2] = (HEAP32[i22 + 16 >> 2] | 0) + i5, HEAP32[i7 + 20 >> 2] = (HEAP32[i7 + 20 >> 2] | 0) + i5, HEAP32[i7 + 16 >> 2] = (HEAP32[i7 + 16 >> 2] | 0) - i5, i7 = HEAP32[i22 + 20 >> 2] | 0, HEAP32[i22 + 20 >> 2] = i7 - i5, (i7 | 0) == (i5 | 0)) : 0) HEAP32[i22 + 16 >> 2] = HEAP32[i22 + 8 >> 2];
      if (!(HEAP32[(HEAP32[i4 >> 2] | 0) + 16 >> 2] | 0)) {
       i50 = (i2 | 0) == 4 ? 2 : 0;
       i11 = 175;
       break L172;
      } else {
       i50 = (i2 | 0) == 4 ? 3 : 1;
       i11 = 175;
       break L172;
      }
      break;
     }
    default:
     {
      i50 = FUNCTION_TABLE_iii[HEAP32[16 + ((HEAP32[i4 + 132 >> 2] | 0) * 12 | 0) + 8 >> 2] & 3](i4, i2) | 0;
      i11 = 175;
     }
    } while (0);
    if ((i11 | 0) == 175) {
     if ((i50 & -2 | 0) == 2) HEAP32[i4 + 4 >> 2] = 666;
     if (i50 & -3) {
      if ((i50 | 0) != 1) break;
      switch (i2 | 0) {
      case 1:
       {
        __tr_align(i4);
        break;
       }
      case 5:
       break;
      default:
       {
        __tr_stored_block(i4, 0, 0, 0);
        if ((i2 | 0) == 3 ? (i22 = HEAP32[i4 + 76 >> 2] | 0, i5 = HEAP32[i4 + 68 >> 2] | 0, HEAP16[i5 + (i22 + -1 << 1) >> 1] = 0, _memset(i5 | 0, 0, (i22 << 1) + -2 | 0) | 0, (HEAP32[i4 + 116 >> 2] | 0) == 0) : 0) {
         HEAP32[i4 + 108 >> 2] = 0;
         HEAP32[i4 + 92 >> 2] = 0;
        }
       }
      }
      i22 = HEAP32[i1 + 28 >> 2] | 0;
      i5 = HEAP32[i22 + 20 >> 2] | 0;
      i7 = HEAP32[i1 + 16 >> 2] | 0;
      i26 = i5 >>> 0 > i7 >>> 0 ? i7 : i5;
      if (i26) {
       _memcpy(HEAP32[i1 + 12 >> 2] | 0, HEAP32[i22 + 16 >> 2] | 0, i26 | 0) | 0;
       HEAP32[i1 + 12 >> 2] = (HEAP32[i1 + 12 >> 2] | 0) + i26;
       i22 = HEAP32[i1 + 28 >> 2] | 0;
       HEAP32[i22 + 16 >> 2] = (HEAP32[i22 + 16 >> 2] | 0) + i26;
       HEAP32[i1 + 20 >> 2] = (HEAP32[i1 + 20 >> 2] | 0) + i26;
       i5 = (HEAP32[i1 + 16 >> 2] | 0) - i26 | 0;
       HEAP32[i1 + 16 >> 2] = i5;
       i8 = HEAP32[i22 + 20 >> 2] | 0;
       HEAP32[i22 + 20 >> 2] = i8 - i26;
       if ((i8 | 0) == (i26 | 0)) {
        HEAP32[i22 + 16 >> 2] = HEAP32[i22 + 8 >> 2];
        i58 = i5;
       } else i58 = i5;
      } else i58 = i7;
      if (i58) break;
      HEAP32[i4 + 40 >> 2] = -1;
      i3 = 0;
      return i3 | 0;
     }
    }
    if (HEAP32[i1 + 16 >> 2] | 0) {
     i3 = 0;
     return i3 | 0;
    }
    HEAP32[i4 + 40 >> 2] = -1;
    i3 = 0;
    return i3 | 0;
   } while (0);
   if ((i2 | 0) != 4) {
    i3 = 0;
    return i3 | 0;
   }
   i6 = HEAP32[i4 + 24 >> 2] | 0;
   if ((i6 | 0) < 1) {
    i3 = 1;
    return i3 | 0;
   }
   i7 = HEAP32[i1 + 48 >> 2] | 0;
   if ((i6 | 0) == 2) {
    i6 = HEAP32[i4 + 20 >> 2] | 0;
    HEAP32[i4 + 20 >> 2] = i6 + 1;
    HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i6 >> 0] = i7;
    i6 = (HEAP32[i1 + 48 >> 2] | 0) >>> 8 & 255;
    i5 = HEAP32[i4 + 20 >> 2] | 0;
    HEAP32[i4 + 20 >> 2] = i5 + 1;
    HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i5 >> 0] = i6;
    i6 = (HEAP32[i1 + 48 >> 2] | 0) >>> 16 & 255;
    i5 = HEAP32[i4 + 20 >> 2] | 0;
    HEAP32[i4 + 20 >> 2] = i5 + 1;
    HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i5 >> 0] = i6;
    i6 = (HEAP32[i1 + 48 >> 2] | 0) >>> 24 & 255;
    i5 = HEAP32[i4 + 20 >> 2] | 0;
    HEAP32[i4 + 20 >> 2] = i5 + 1;
    HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i5 >> 0] = i6;
    i6 = HEAP32[i1 + 8 >> 2] & 255;
    i5 = HEAP32[i4 + 20 >> 2] | 0;
    HEAP32[i4 + 20 >> 2] = i5 + 1;
    HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i5 >> 0] = i6;
    i6 = (HEAP32[i1 + 8 >> 2] | 0) >>> 8 & 255;
    i5 = HEAP32[i4 + 20 >> 2] | 0;
    HEAP32[i4 + 20 >> 2] = i5 + 1;
    HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i5 >> 0] = i6;
    i6 = (HEAP32[i1 + 8 >> 2] | 0) >>> 16 & 255;
    i5 = HEAP32[i4 + 20 >> 2] | 0;
    HEAP32[i4 + 20 >> 2] = i5 + 1;
    HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i5 >> 0] = i6;
    i6 = (HEAP32[i1 + 8 >> 2] | 0) >>> 24 & 255;
    i5 = HEAP32[i4 + 20 >> 2] | 0;
    HEAP32[i4 + 20 >> 2] = i5 + 1;
    HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i5 >> 0] = i6;
   } else {
    i6 = HEAP32[i4 + 20 >> 2] | 0;
    HEAP32[i4 + 20 >> 2] = i6 + 1;
    HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i6 >> 0] = i7 >>> 24;
    i6 = HEAP32[i4 + 20 >> 2] | 0;
    HEAP32[i4 + 20 >> 2] = i6 + 1;
    HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i6 >> 0] = i7 >>> 16;
    i7 = HEAP32[i1 + 48 >> 2] | 0;
    i6 = HEAP32[i4 + 20 >> 2] | 0;
    HEAP32[i4 + 20 >> 2] = i6 + 1;
    HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i6 >> 0] = i7 >>> 8;
    i6 = HEAP32[i4 + 20 >> 2] | 0;
    HEAP32[i4 + 20 >> 2] = i6 + 1;
    HEAP8[(HEAP32[i4 + 8 >> 2] | 0) + i6 >> 0] = i7;
   }
   i7 = HEAP32[i1 + 28 >> 2] | 0;
   i6 = HEAP32[i7 + 20 >> 2] | 0;
   i5 = HEAP32[i1 + 16 >> 2] | 0;
   i22 = i6 >>> 0 > i5 >>> 0 ? i5 : i6;
   if ((i22 | 0) != 0 ? (_memcpy(HEAP32[i1 + 12 >> 2] | 0, HEAP32[i7 + 16 >> 2] | 0, i22 | 0) | 0, HEAP32[i1 + 12 >> 2] = (HEAP32[i1 + 12 >> 2] | 0) + i22, i7 = HEAP32[i1 + 28 >> 2] | 0, HEAP32[i7 + 16 >> 2] = (HEAP32[i7 + 16 >> 2] | 0) + i22, HEAP32[i1 + 20 >> 2] = (HEAP32[i1 + 20 >> 2] | 0) + i22, HEAP32[i1 + 16 >> 2] = (HEAP32[i1 + 16 >> 2] | 0) - i22, i6 = HEAP32[i7 + 20 >> 2] | 0, HEAP32[i7 + 20 >> 2] = i6 - i22, (i6 | 0) == (i22 | 0)) : 0) HEAP32[i7 + 16 >> 2] = HEAP32[i7 + 8 >> 2];
   i7 = HEAP32[i4 + 24 >> 2] | 0;
   if ((i7 | 0) > 0) HEAP32[i4 + 24 >> 2] = 0 - i7;
   i3 = (HEAP32[i4 + 20 >> 2] | 0) == 0 & 1;
   return i3 | 0;
  }
 } while (0);
 HEAP32[i1 + 24 >> 2] = HEAP32[190];
 i3 = -2;
 return i3 | 0;
}

function _build_tree(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0, i24 = 0, i25 = 0, i26 = 0, i27 = 0, i28 = 0, i29 = 0, i30 = 0, i31 = 0, i32 = 0, i33 = 0, i34 = 0, i35 = 0, i36 = 0;
 i3 = STACKTOP;
 STACKTOP = STACKTOP + 32 | 0;
 i4 = HEAP32[i2 >> 2] | 0;
 i5 = HEAP32[i2 + 8 >> 2] | 0;
 i6 = HEAP32[i5 >> 2] | 0;
 i7 = HEAP32[i5 + 12 >> 2] | 0;
 HEAP32[i1 + 5200 >> 2] = 0;
 HEAP32[i1 + 5204 >> 2] = 573;
 if ((i7 | 0) > 0) {
  i5 = -1;
  i8 = 0;
  while (1) {
   if (!(HEAP16[i4 + (i8 << 2) >> 1] | 0)) {
    HEAP16[i4 + (i8 << 2) + 2 >> 1] = 0;
    i9 = i5;
   } else {
    i10 = (HEAP32[i1 + 5200 >> 2] | 0) + 1 | 0;
    HEAP32[i1 + 5200 >> 2] = i10;
    HEAP32[i1 + 2908 + (i10 << 2) >> 2] = i8;
    HEAP8[i1 + 5208 + i8 >> 0] = 0;
    i9 = i8;
   }
   i8 = i8 + 1 | 0;
   if ((i8 | 0) == (i7 | 0)) {
    i11 = i9;
    break;
   } else i5 = i9;
  }
  i9 = HEAP32[i1 + 5200 >> 2] | 0;
  if ((i9 | 0) < 2) {
   i12 = i9;
   i13 = i11;
   i14 = 3;
  } else {
   i15 = i9;
   i16 = i11;
  }
 } else {
  i12 = 0;
  i13 = -1;
  i14 = 3;
 }
 if ((i14 | 0) == 3) if (!i6) {
  i14 = i12;
  i11 = i13;
  while (1) {
   i9 = (i11 | 0) < 2;
   i5 = i11 + 1 | 0;
   i8 = i9 ? i5 : i11;
   i10 = i9 ? i5 : 0;
   i5 = i14 + 1 | 0;
   HEAP32[i1 + 5200 >> 2] = i5;
   HEAP32[i1 + 2908 + (i5 << 2) >> 2] = i10;
   HEAP16[i4 + (i10 << 2) >> 1] = 1;
   HEAP8[i1 + 5208 + i10 >> 0] = 0;
   HEAP32[i1 + 5800 >> 2] = (HEAP32[i1 + 5800 >> 2] | 0) + -1;
   i10 = HEAP32[i1 + 5200 >> 2] | 0;
   if ((i10 | 0) < 2) {
    i14 = i10;
    i11 = i8;
   } else {
    i15 = i10;
    i16 = i8;
    break;
   }
  }
 } else {
  i11 = i12;
  i12 = i13;
  while (1) {
   i13 = (i12 | 0) < 2;
   i14 = i12 + 1 | 0;
   i8 = i13 ? i14 : i12;
   i10 = i13 ? i14 : 0;
   i14 = i11 + 1 | 0;
   HEAP32[i1 + 5200 >> 2] = i14;
   HEAP32[i1 + 2908 + (i14 << 2) >> 2] = i10;
   HEAP16[i4 + (i10 << 2) >> 1] = 1;
   HEAP8[i1 + 5208 + i10 >> 0] = 0;
   HEAP32[i1 + 5800 >> 2] = (HEAP32[i1 + 5800 >> 2] | 0) + -1;
   HEAP32[i1 + 5804 >> 2] = (HEAP32[i1 + 5804 >> 2] | 0) - (HEAPU16[i6 + (i10 << 2) + 2 >> 1] | 0);
   i10 = HEAP32[i1 + 5200 >> 2] | 0;
   if ((i10 | 0) < 2) {
    i11 = i10;
    i12 = i8;
   } else {
    i15 = i10;
    i16 = i8;
    break;
   }
  }
 }
 HEAP32[i2 + 4 >> 2] = i16;
 i12 = i15;
 i11 = (i15 | 0) / 2 | 0;
 while (1) {
  i15 = HEAP32[i1 + 2908 + (i11 << 2) >> 2] | 0;
  i6 = i11 << 1;
  L19 : do if ((i6 | 0) > (i12 | 0)) i17 = i11; else {
   i8 = i11;
   i10 = i12;
   i14 = i6;
   while (1) {
    do if ((i14 | 0) < (i10 | 0)) {
     i13 = i14 | 1;
     i5 = HEAP32[i1 + 2908 + (i13 << 2) >> 2] | 0;
     i9 = HEAP16[i4 + (i5 << 2) >> 1] | 0;
     i18 = HEAP32[i1 + 2908 + (i14 << 2) >> 2] | 0;
     i19 = HEAP16[i4 + (i18 << 2) >> 1] | 0;
     if ((i9 & 65535) >= (i19 & 65535)) {
      if (i9 << 16 >> 16 != i19 << 16 >> 16) {
       i20 = i14;
       break;
      }
      if ((HEAPU8[i1 + 5208 + i5 >> 0] | 0) > (HEAPU8[i1 + 5208 + i18 >> 0] | 0)) {
       i20 = i14;
       break;
      }
     }
     i20 = i13;
    } else i20 = i14; while (0);
    i13 = HEAP16[i4 + (i15 << 2) >> 1] | 0;
    i18 = HEAP32[i1 + 2908 + (i20 << 2) >> 2] | 0;
    i5 = HEAP16[i4 + (i18 << 2) >> 1] | 0;
    if ((i13 & 65535) < (i5 & 65535)) {
     i17 = i8;
     break L19;
    }
    if (i13 << 16 >> 16 == i5 << 16 >> 16 ? (HEAPU8[i1 + 5208 + i15 >> 0] | 0) <= (HEAPU8[i1 + 5208 + i18 >> 0] | 0) : 0) {
     i17 = i8;
     break L19;
    }
    HEAP32[i1 + 2908 + (i8 << 2) >> 2] = i18;
    i14 = i20 << 1;
    i10 = HEAP32[i1 + 5200 >> 2] | 0;
    if ((i14 | 0) > (i10 | 0)) {
     i17 = i20;
     break;
    } else i8 = i20;
   }
  } while (0);
  HEAP32[i1 + 2908 + (i17 << 2) >> 2] = i15;
  if ((i11 | 0) <= 1) break;
  i12 = HEAP32[i1 + 5200 >> 2] | 0;
  i11 = i11 + -1 | 0;
 }
 i11 = HEAP32[i1 + 5200 >> 2] | 0;
 i12 = i7;
 do {
  i7 = HEAP32[i1 + 2912 >> 2] | 0;
  i17 = i11 + -1 | 0;
  HEAP32[i1 + 5200 >> 2] = i17;
  i20 = HEAP32[i1 + 2908 + (i11 << 2) >> 2] | 0;
  HEAP32[i1 + 2912 >> 2] = i20;
  L39 : do if ((i11 | 0) < 3) i21 = 1; else {
   i6 = 1;
   i8 = i17;
   i10 = 2;
   while (1) {
    do if ((i10 | 0) < (i8 | 0)) {
     i14 = i10 | 1;
     i18 = HEAP32[i1 + 2908 + (i14 << 2) >> 2] | 0;
     i5 = HEAP16[i4 + (i18 << 2) >> 1] | 0;
     i13 = HEAP32[i1 + 2908 + (i10 << 2) >> 2] | 0;
     i19 = HEAP16[i4 + (i13 << 2) >> 1] | 0;
     if ((i5 & 65535) >= (i19 & 65535)) {
      if (i5 << 16 >> 16 != i19 << 16 >> 16) {
       i22 = i10;
       break;
      }
      if ((HEAPU8[i1 + 5208 + i18 >> 0] | 0) > (HEAPU8[i1 + 5208 + i13 >> 0] | 0)) {
       i22 = i10;
       break;
      }
     }
     i22 = i14;
    } else i22 = i10; while (0);
    i14 = HEAP16[i4 + (i20 << 2) >> 1] | 0;
    i13 = HEAP32[i1 + 2908 + (i22 << 2) >> 2] | 0;
    i18 = HEAP16[i4 + (i13 << 2) >> 1] | 0;
    if ((i14 & 65535) < (i18 & 65535)) {
     i21 = i6;
     break L39;
    }
    if (i14 << 16 >> 16 == i18 << 16 >> 16 ? (HEAPU8[i1 + 5208 + i20 >> 0] | 0) <= (HEAPU8[i1 + 5208 + i13 >> 0] | 0) : 0) {
     i21 = i6;
     break L39;
    }
    HEAP32[i1 + 2908 + (i6 << 2) >> 2] = i13;
    i10 = i22 << 1;
    i8 = HEAP32[i1 + 5200 >> 2] | 0;
    if ((i10 | 0) > (i8 | 0)) {
     i21 = i22;
     break;
    } else i6 = i22;
   }
  } while (0);
  HEAP32[i1 + 2908 + (i21 << 2) >> 2] = i20;
  i17 = HEAP32[i1 + 2912 >> 2] | 0;
  i15 = (HEAP32[i1 + 5204 >> 2] | 0) + -1 | 0;
  HEAP32[i1 + 5204 >> 2] = i15;
  HEAP32[i1 + 2908 + (i15 << 2) >> 2] = i7;
  i15 = (HEAP32[i1 + 5204 >> 2] | 0) + -1 | 0;
  HEAP32[i1 + 5204 >> 2] = i15;
  HEAP32[i1 + 2908 + (i15 << 2) >> 2] = i17;
  i15 = i4 + (i12 << 2) | 0;
  HEAP16[i15 >> 1] = (HEAPU16[i4 + (i17 << 2) >> 1] | 0) + (HEAPU16[i4 + (i7 << 2) >> 1] | 0);
  i6 = HEAP8[i1 + 5208 + i7 >> 0] | 0;
  i8 = HEAP8[i1 + 5208 + i17 >> 0] | 0;
  i10 = i1 + 5208 + i12 | 0;
  HEAP8[i10 >> 0] = (((i6 & 255) < (i8 & 255) ? i8 : i6) & 255) + 1;
  i6 = i12 & 65535;
  HEAP16[i4 + (i17 << 2) + 2 >> 1] = i6;
  HEAP16[i4 + (i7 << 2) + 2 >> 1] = i6;
  HEAP32[i1 + 2912 >> 2] = i12;
  i6 = HEAP32[i1 + 5200 >> 2] | 0;
  L55 : do if ((i6 | 0) < 2) i23 = 1; else {
   i17 = 1;
   i8 = i6;
   i13 = 2;
   while (1) {
    do if ((i13 | 0) < (i8 | 0)) {
     i18 = i13 | 1;
     i14 = HEAP32[i1 + 2908 + (i18 << 2) >> 2] | 0;
     i19 = HEAP16[i4 + (i14 << 2) >> 1] | 0;
     i5 = HEAP32[i1 + 2908 + (i13 << 2) >> 2] | 0;
     i9 = HEAP16[i4 + (i5 << 2) >> 1] | 0;
     if ((i19 & 65535) >= (i9 & 65535)) {
      if (i19 << 16 >> 16 != i9 << 16 >> 16) {
       i24 = i13;
       break;
      }
      if ((HEAPU8[i1 + 5208 + i14 >> 0] | 0) > (HEAPU8[i1 + 5208 + i5 >> 0] | 0)) {
       i24 = i13;
       break;
      }
     }
     i24 = i18;
    } else i24 = i13; while (0);
    i18 = HEAP16[i15 >> 1] | 0;
    i5 = HEAP32[i1 + 2908 + (i24 << 2) >> 2] | 0;
    i14 = HEAP16[i4 + (i5 << 2) >> 1] | 0;
    if ((i18 & 65535) < (i14 & 65535)) {
     i23 = i17;
     break L55;
    }
    if (i18 << 16 >> 16 == i14 << 16 >> 16 ? (HEAPU8[i10 >> 0] | 0) <= (HEAPU8[i1 + 5208 + i5 >> 0] | 0) : 0) {
     i23 = i17;
     break L55;
    }
    HEAP32[i1 + 2908 + (i17 << 2) >> 2] = i5;
    i13 = i24 << 1;
    i8 = HEAP32[i1 + 5200 >> 2] | 0;
    if ((i13 | 0) > (i8 | 0)) {
     i23 = i24;
     break;
    } else i17 = i24;
   }
  } while (0);
  HEAP32[i1 + 2908 + (i23 << 2) >> 2] = i12;
  i12 = i12 + 1 | 0;
  i11 = HEAP32[i1 + 5200 >> 2] | 0;
 } while ((i11 | 0) > 1);
 i11 = HEAP32[i1 + 2912 >> 2] | 0;
 i12 = (HEAP32[i1 + 5204 >> 2] | 0) + -1 | 0;
 HEAP32[i1 + 5204 >> 2] = i12;
 HEAP32[i1 + 2908 + (i12 << 2) >> 2] = i11;
 i11 = HEAP32[i2 >> 2] | 0;
 i12 = HEAP32[i2 + 4 >> 2] | 0;
 i23 = HEAP32[i2 + 8 >> 2] | 0;
 i2 = HEAP32[i23 >> 2] | 0;
 i24 = HEAP32[i23 + 4 >> 2] | 0;
 i21 = HEAP32[i23 + 8 >> 2] | 0;
 i22 = HEAP32[i23 + 16 >> 2] | 0;
 i23 = i1 + 2876 | 0;
 i10 = i23 + 32 | 0;
 do {
  HEAP16[i23 >> 1] = 0;
  i23 = i23 + 2 | 0;
 } while ((i23 | 0) < (i10 | 0));
 i23 = HEAP32[i1 + 5204 >> 2] | 0;
 HEAP16[i11 + (HEAP32[i1 + 2908 + (i23 << 2) >> 2] << 2) + 2 >> 1] = 0;
 L71 : do if ((i23 + 1 | 0) < 573) {
  if (!i2) {
   i10 = i23 + 1 | 0;
   i15 = 0;
   while (1) {
    i6 = HEAP32[i1 + 2908 + (i10 << 2) >> 2] | 0;
    i7 = HEAPU16[i11 + (HEAPU16[i11 + (i6 << 2) + 2 >> 1] << 2) + 2 >> 1] | 0;
    i20 = (i7 | 0) < (i22 | 0) ? i7 + 1 | 0 : i22;
    i17 = ((i7 | 0) < (i22 | 0) ^ 1) + i15 | 0;
    HEAP16[i11 + (i6 << 2) + 2 >> 1] = i20;
    if ((i6 | 0) <= (i12 | 0)) {
     HEAP16[i1 + 2876 + (i20 << 1) >> 1] = (HEAP16[i1 + 2876 + (i20 << 1) >> 1] | 0) + 1 << 16 >> 16;
     if ((i6 | 0) < (i21 | 0)) i25 = 0; else i25 = HEAP32[i24 + (i6 - i21 << 2) >> 2] | 0;
     i7 = Math_imul(HEAPU16[i11 + (i6 << 2) >> 1] | 0, i25 + i20 | 0) | 0;
     HEAP32[i1 + 5800 >> 2] = i7 + (HEAP32[i1 + 5800 >> 2] | 0);
    }
    i10 = i10 + 1 | 0;
    if ((i10 | 0) == 573) {
     i26 = i17;
     break;
    } else i15 = i17;
   }
  } else {
   i15 = i23 + 1 | 0;
   i10 = 0;
   while (1) {
    i17 = HEAP32[i1 + 2908 + (i15 << 2) >> 2] | 0;
    i7 = HEAPU16[i11 + (HEAPU16[i11 + (i17 << 2) + 2 >> 1] << 2) + 2 >> 1] | 0;
    i20 = (i7 | 0) < (i22 | 0) ? i7 + 1 | 0 : i22;
    i6 = ((i7 | 0) < (i22 | 0) ^ 1) + i10 | 0;
    HEAP16[i11 + (i17 << 2) + 2 >> 1] = i20;
    if ((i17 | 0) <= (i12 | 0)) {
     HEAP16[i1 + 2876 + (i20 << 1) >> 1] = (HEAP16[i1 + 2876 + (i20 << 1) >> 1] | 0) + 1 << 16 >> 16;
     if ((i17 | 0) < (i21 | 0)) i27 = 0; else i27 = HEAP32[i24 + (i17 - i21 << 2) >> 2] | 0;
     i7 = HEAPU16[i11 + (i17 << 2) >> 1] | 0;
     i8 = Math_imul(i7, i27 + i20 | 0) | 0;
     HEAP32[i1 + 5800 >> 2] = i8 + (HEAP32[i1 + 5800 >> 2] | 0);
     i8 = Math_imul((HEAPU16[i2 + (i17 << 2) + 2 >> 1] | 0) + i27 | 0, i7) | 0;
     HEAP32[i1 + 5804 >> 2] = i8 + (HEAP32[i1 + 5804 >> 2] | 0);
    }
    i15 = i15 + 1 | 0;
    if ((i15 | 0) == 573) {
     i26 = i6;
     break;
    } else i10 = i6;
   }
  }
  if (i26) {
   i10 = i26;
   while (1) {
    i15 = i22;
    while (1) {
     i28 = i15 + -1 | 0;
     i29 = HEAP16[i1 + 2876 + (i28 << 1) >> 1] | 0;
     if (!(i29 << 16 >> 16)) i15 = i28; else {
      i30 = i15;
      break;
     }
    }
    HEAP16[i1 + 2876 + (i28 << 1) >> 1] = i29 + -1 << 16 >> 16;
    HEAP16[i1 + 2876 + (i30 << 1) >> 1] = (HEAPU16[i1 + 2876 + (i30 << 1) >> 1] | 0) + 2;
    i31 = (HEAP16[i1 + 2876 + (i22 << 1) >> 1] | 0) + -1 << 16 >> 16;
    HEAP16[i1 + 2876 + (i22 << 1) >> 1] = i31;
    if ((i10 | 0) > 2) i10 = i10 + -2 | 0; else break;
   }
   if (i22) {
    i10 = i31;
    i15 = i22;
    i6 = 573;
    while (1) {
     i8 = i15 & 65535;
     if (!(i10 << 16 >> 16)) i32 = i6; else {
      i7 = i6;
      i17 = i10 & 65535;
      while (1) {
       i20 = i7;
       while (1) {
        i33 = i20 + -1 | 0;
        i34 = HEAP32[i1 + 2908 + (i33 << 2) >> 2] | 0;
        if ((i34 | 0) > (i12 | 0)) i20 = i33; else break;
       }
       i20 = HEAPU16[i11 + (i34 << 2) + 2 >> 1] | 0;
       if ((i15 | 0) != (i20 | 0)) {
        i13 = Math_imul(HEAPU16[i11 + (i34 << 2) >> 1] | 0, i15 - i20 | 0) | 0;
        HEAP32[i1 + 5800 >> 2] = i13 + (HEAP32[i1 + 5800 >> 2] | 0);
        HEAP16[i11 + (i34 << 2) + 2 >> 1] = i8;
       }
       i17 = i17 + -1 | 0;
       if (!i17) {
        i32 = i33;
        break;
       } else i7 = i33;
      }
     }
     i7 = i15 + -1 | 0;
     if (!i7) break L71;
     i10 = HEAP16[i1 + 2876 + (i7 << 1) >> 1] | 0;
     i15 = i7;
     i6 = i32;
    }
   }
  }
 } while (0);
 i32 = 1;
 i33 = 0;
 do {
  i33 = (HEAPU16[i1 + 2876 + (i32 + -1 << 1) >> 1] | 0) + (i33 & 65534) << 1;
  HEAP16[i3 + (i32 << 1) >> 1] = i33;
  i32 = i32 + 1 | 0;
 } while ((i32 | 0) != 16);
 if ((i16 | 0) < 0) {
  STACKTOP = i3;
  return;
 } else i35 = 0;
 while (1) {
  i32 = HEAP16[i4 + (i35 << 2) + 2 >> 1] | 0;
  if (i32 << 16 >> 16) {
   i33 = HEAP16[i3 + ((i32 & 65535) << 1) >> 1] | 0;
   HEAP16[i3 + ((i32 & 65535) << 1) >> 1] = i33 + 1 << 16 >> 16;
   i1 = i32 & 65535;
   i32 = i33 & 65535;
   i33 = 0;
   while (1) {
    i36 = i33 | i32 & 1;
    if ((i1 | 0) > 1) {
     i1 = i1 + -1 | 0;
     i32 = i32 >>> 1;
     i33 = i36 << 1;
    } else break;
   }
   HEAP16[i4 + (i35 << 2) >> 1] = i36;
  }
  if ((i35 | 0) == (i16 | 0)) break; else i35 = i35 + 1 | 0;
 }
 STACKTOP = i3;
 return;
}

function _free(i1) {
 i1 = i1 | 0;
 var i2 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0, i24 = 0, i25 = 0, i26 = 0, i27 = 0, i28 = 0, i29 = 0, i30 = 0, i31 = 0, i32 = 0, i33 = 0, i34 = 0, i35 = 0, i36 = 0;
 L1 : do if (i1) {
  i2 = HEAP32[2261] | 0;
  L3 : do if ((i1 + -8 | 0) >>> 0 >= i2 >>> 0 ? (i3 = HEAP32[i1 + -4 >> 2] | 0, (i3 & 3 | 0) != 1) : 0) {
   i4 = i1 + ((i3 & -8) + -8) | 0;
   do if (!(i3 & 1)) {
    i5 = HEAP32[i1 + -8 >> 2] | 0;
    if (!(i3 & 3)) break L1;
    i6 = i1 + (-8 - i5) | 0;
    i7 = i5 + (i3 & -8) | 0;
    if (i6 >>> 0 < i2 >>> 0) break L3;
    if ((i6 | 0) == (HEAP32[2262] | 0)) {
     i8 = HEAP32[i1 + ((i3 & -8) + -4) >> 2] | 0;
     if ((i8 & 3 | 0) != 3) {
      i9 = i6;
      i10 = i7;
      break;
     }
     HEAP32[2259] = i7;
     HEAP32[i1 + ((i3 & -8) + -4) >> 2] = i8 & -2;
     HEAP32[i1 + (-8 - i5 + 4) >> 2] = i7 | 1;
     HEAP32[i4 >> 2] = i7;
     break L1;
    }
    if (i5 >>> 0 < 256) {
     i8 = HEAP32[i1 + (-8 - i5 + 8) >> 2] | 0;
     i11 = HEAP32[i1 + (-8 - i5 + 12) >> 2] | 0;
     do if ((i8 | 0) != (9068 + (i5 >>> 3 << 1 << 2) | 0)) {
      if (i8 >>> 0 >= i2 >>> 0 ? (HEAP32[i8 + 12 >> 2] | 0) == (i6 | 0) : 0) break;
      _abort();
     } while (0);
     if ((i11 | 0) == (i8 | 0)) {
      HEAP32[2257] = HEAP32[2257] & ~(1 << (i5 >>> 3));
      i9 = i6;
      i10 = i7;
      break;
     }
     do if ((i11 | 0) == (9068 + (i5 >>> 3 << 1 << 2) | 0)) i12 = i11 + 8 | 0; else {
      if (i11 >>> 0 >= i2 >>> 0 ? (HEAP32[i11 + 8 >> 2] | 0) == (i6 | 0) : 0) {
       i12 = i11 + 8 | 0;
       break;
      }
      _abort();
     } while (0);
     HEAP32[i8 + 12 >> 2] = i11;
     HEAP32[i12 >> 2] = i8;
     i9 = i6;
     i10 = i7;
     break;
    }
    i13 = HEAP32[i1 + (-8 - i5 + 24) >> 2] | 0;
    i14 = HEAP32[i1 + (-8 - i5 + 12) >> 2] | 0;
    do if ((i14 | 0) == (i6 | 0)) {
     i15 = HEAP32[i1 + (-8 - i5 + 20) >> 2] | 0;
     if (!i15) {
      i16 = HEAP32[i1 + (-8 - i5 + 16) >> 2] | 0;
      if (!i16) {
       i17 = 0;
       break;
      } else {
       i18 = i16;
       i19 = i1 + (-8 - i5 + 16) | 0;
      }
     } else {
      i18 = i15;
      i19 = i1 + (-8 - i5 + 20) | 0;
     }
     while (1) {
      i15 = i18 + 20 | 0;
      i16 = HEAP32[i15 >> 2] | 0;
      if (i16) {
       i18 = i16;
       i19 = i15;
       continue;
      }
      i15 = i18 + 16 | 0;
      i16 = HEAP32[i15 >> 2] | 0;
      if (!i16) {
       i20 = i18;
       i21 = i19;
       break;
      } else {
       i18 = i16;
       i19 = i15;
      }
     }
     if (i21 >>> 0 < i2 >>> 0) _abort(); else {
      HEAP32[i21 >> 2] = 0;
      i17 = i20;
      break;
     }
    } else {
     i15 = HEAP32[i1 + (-8 - i5 + 8) >> 2] | 0;
     if ((i15 >>> 0 >= i2 >>> 0 ? (HEAP32[i15 + 12 >> 2] | 0) == (i6 | 0) : 0) ? (HEAP32[i14 + 8 >> 2] | 0) == (i6 | 0) : 0) {
      HEAP32[i15 + 12 >> 2] = i14;
      HEAP32[i14 + 8 >> 2] = i15;
      i17 = i14;
      break;
     }
     _abort();
    } while (0);
    if (i13) {
     i14 = HEAP32[i1 + (-8 - i5 + 28) >> 2] | 0;
     if ((i6 | 0) == (HEAP32[9332 + (i14 << 2) >> 2] | 0)) {
      HEAP32[9332 + (i14 << 2) >> 2] = i17;
      if (!i17) {
       HEAP32[2258] = HEAP32[2258] & ~(1 << i14);
       i9 = i6;
       i10 = i7;
       break;
      }
     } else {
      if (i13 >>> 0 < (HEAP32[2261] | 0) >>> 0) _abort();
      if ((HEAP32[i13 + 16 >> 2] | 0) == (i6 | 0)) HEAP32[i13 + 16 >> 2] = i17; else HEAP32[i13 + 20 >> 2] = i17;
      if (!i17) {
       i9 = i6;
       i10 = i7;
       break;
      }
     }
     i14 = HEAP32[2261] | 0;
     if (i17 >>> 0 < i14 >>> 0) _abort();
     HEAP32[i17 + 24 >> 2] = i13;
     i8 = HEAP32[i1 + (-8 - i5 + 16) >> 2] | 0;
     do if (i8) if (i8 >>> 0 < i14 >>> 0) _abort(); else {
      HEAP32[i17 + 16 >> 2] = i8;
      HEAP32[i8 + 24 >> 2] = i17;
      break;
     } while (0);
     i8 = HEAP32[i1 + (-8 - i5 + 20) >> 2] | 0;
     if (i8) if (i8 >>> 0 < (HEAP32[2261] | 0) >>> 0) _abort(); else {
      HEAP32[i17 + 20 >> 2] = i8;
      HEAP32[i8 + 24 >> 2] = i17;
      i9 = i6;
      i10 = i7;
      break;
     } else {
      i9 = i6;
      i10 = i7;
     }
    } else {
     i9 = i6;
     i10 = i7;
    }
   } else {
    i9 = i1 + -8 | 0;
    i10 = i3 & -8;
   } while (0);
   if (i9 >>> 0 < i4 >>> 0 ? (i8 = HEAP32[i1 + ((i3 & -8) + -4) >> 2] | 0, (i8 & 1 | 0) != 0) : 0) {
    if (!(i8 & 2)) {
     if ((i4 | 0) == (HEAP32[2263] | 0)) {
      i14 = (HEAP32[2260] | 0) + i10 | 0;
      HEAP32[2260] = i14;
      HEAP32[2263] = i9;
      HEAP32[i9 + 4 >> 2] = i14 | 1;
      if ((i9 | 0) != (HEAP32[2262] | 0)) break L1;
      HEAP32[2262] = 0;
      HEAP32[2259] = 0;
      break L1;
     }
     if ((i4 | 0) == (HEAP32[2262] | 0)) {
      i14 = (HEAP32[2259] | 0) + i10 | 0;
      HEAP32[2259] = i14;
      HEAP32[2262] = i9;
      HEAP32[i9 + 4 >> 2] = i14 | 1;
      HEAP32[i9 + i14 >> 2] = i14;
      break L1;
     }
     i14 = (i8 & -8) + i10 | 0;
     do if (i8 >>> 0 >= 256) {
      i13 = HEAP32[i1 + ((i3 & -8) + 16) >> 2] | 0;
      i11 = HEAP32[i1 + (i3 & -8 | 4) >> 2] | 0;
      do if ((i11 | 0) == (i4 | 0)) {
       i15 = HEAP32[i1 + ((i3 & -8) + 12) >> 2] | 0;
       if (!i15) {
        i16 = HEAP32[i1 + ((i3 & -8) + 8) >> 2] | 0;
        if (!i16) {
         i22 = 0;
         break;
        } else {
         i23 = i16;
         i24 = i1 + ((i3 & -8) + 8) | 0;
        }
       } else {
        i23 = i15;
        i24 = i1 + ((i3 & -8) + 12) | 0;
       }
       while (1) {
        i15 = i23 + 20 | 0;
        i16 = HEAP32[i15 >> 2] | 0;
        if (i16) {
         i23 = i16;
         i24 = i15;
         continue;
        }
        i15 = i23 + 16 | 0;
        i16 = HEAP32[i15 >> 2] | 0;
        if (!i16) {
         i25 = i23;
         i26 = i24;
         break;
        } else {
         i23 = i16;
         i24 = i15;
        }
       }
       if (i26 >>> 0 < (HEAP32[2261] | 0) >>> 0) _abort(); else {
        HEAP32[i26 >> 2] = 0;
        i22 = i25;
        break;
       }
      } else {
       i15 = HEAP32[i1 + (i3 & -8) >> 2] | 0;
       if ((i15 >>> 0 >= (HEAP32[2261] | 0) >>> 0 ? (HEAP32[i15 + 12 >> 2] | 0) == (i4 | 0) : 0) ? (HEAP32[i11 + 8 >> 2] | 0) == (i4 | 0) : 0) {
        HEAP32[i15 + 12 >> 2] = i11;
        HEAP32[i11 + 8 >> 2] = i15;
        i22 = i11;
        break;
       }
       _abort();
      } while (0);
      if (i13) {
       i11 = HEAP32[i1 + ((i3 & -8) + 20) >> 2] | 0;
       if ((i4 | 0) == (HEAP32[9332 + (i11 << 2) >> 2] | 0)) {
        HEAP32[9332 + (i11 << 2) >> 2] = i22;
        if (!i22) {
         HEAP32[2258] = HEAP32[2258] & ~(1 << i11);
         break;
        }
       } else {
        if (i13 >>> 0 < (HEAP32[2261] | 0) >>> 0) _abort();
        if ((HEAP32[i13 + 16 >> 2] | 0) == (i4 | 0)) HEAP32[i13 + 16 >> 2] = i22; else HEAP32[i13 + 20 >> 2] = i22;
        if (!i22) break;
       }
       i11 = HEAP32[2261] | 0;
       if (i22 >>> 0 < i11 >>> 0) _abort();
       HEAP32[i22 + 24 >> 2] = i13;
       i7 = HEAP32[i1 + ((i3 & -8) + 8) >> 2] | 0;
       do if (i7) if (i7 >>> 0 < i11 >>> 0) _abort(); else {
        HEAP32[i22 + 16 >> 2] = i7;
        HEAP32[i7 + 24 >> 2] = i22;
        break;
       } while (0);
       i7 = HEAP32[i1 + ((i3 & -8) + 12) >> 2] | 0;
       if (i7) if (i7 >>> 0 < (HEAP32[2261] | 0) >>> 0) _abort(); else {
        HEAP32[i22 + 20 >> 2] = i7;
        HEAP32[i7 + 24 >> 2] = i22;
        break;
       }
      }
     } else {
      i7 = HEAP32[i1 + (i3 & -8) >> 2] | 0;
      i11 = HEAP32[i1 + (i3 & -8 | 4) >> 2] | 0;
      do if ((i7 | 0) != (9068 + (i8 >>> 3 << 1 << 2) | 0)) {
       if (i7 >>> 0 >= (HEAP32[2261] | 0) >>> 0 ? (HEAP32[i7 + 12 >> 2] | 0) == (i4 | 0) : 0) break;
       _abort();
      } while (0);
      if ((i11 | 0) == (i7 | 0)) {
       HEAP32[2257] = HEAP32[2257] & ~(1 << (i8 >>> 3));
       break;
      }
      do if ((i11 | 0) == (9068 + (i8 >>> 3 << 1 << 2) | 0)) i27 = i11 + 8 | 0; else {
       if (i11 >>> 0 >= (HEAP32[2261] | 0) >>> 0 ? (HEAP32[i11 + 8 >> 2] | 0) == (i4 | 0) : 0) {
        i27 = i11 + 8 | 0;
        break;
       }
       _abort();
      } while (0);
      HEAP32[i7 + 12 >> 2] = i11;
      HEAP32[i27 >> 2] = i7;
     } while (0);
     HEAP32[i9 + 4 >> 2] = i14 | 1;
     HEAP32[i9 + i14 >> 2] = i14;
     if ((i9 | 0) == (HEAP32[2262] | 0)) {
      HEAP32[2259] = i14;
      break L1;
     } else i28 = i14;
    } else {
     HEAP32[i1 + ((i3 & -8) + -4) >> 2] = i8 & -2;
     HEAP32[i9 + 4 >> 2] = i10 | 1;
     HEAP32[i9 + i10 >> 2] = i10;
     i28 = i10;
    }
    i4 = i28 >>> 3;
    if (i28 >>> 0 < 256) {
     i13 = HEAP32[2257] | 0;
     if (i13 & 1 << i4) {
      i6 = HEAP32[9068 + ((i4 << 1) + 2 << 2) >> 2] | 0;
      if (i6 >>> 0 < (HEAP32[2261] | 0) >>> 0) _abort(); else {
       i29 = 9068 + ((i4 << 1) + 2 << 2) | 0;
       i30 = i6;
      }
     } else {
      HEAP32[2257] = i13 | 1 << i4;
      i29 = 9068 + ((i4 << 1) + 2 << 2) | 0;
      i30 = 9068 + (i4 << 1 << 2) | 0;
     }
     HEAP32[i29 >> 2] = i9;
     HEAP32[i30 + 12 >> 2] = i9;
     HEAP32[i9 + 8 >> 2] = i30;
     HEAP32[i9 + 12 >> 2] = 9068 + (i4 << 1 << 2);
     break L1;
    }
    i4 = i28 >>> 8;
    if (i4) if (i28 >>> 0 > 16777215) i31 = 31; else {
     i13 = i4 << ((i4 + 1048320 | 0) >>> 16 & 8) << (((i4 << ((i4 + 1048320 | 0) >>> 16 & 8)) + 520192 | 0) >>> 16 & 4);
     i6 = 14 - (((i4 << ((i4 + 1048320 | 0) >>> 16 & 8)) + 520192 | 0) >>> 16 & 4 | (i4 + 1048320 | 0) >>> 16 & 8 | (i13 + 245760 | 0) >>> 16 & 2) + (i13 << ((i13 + 245760 | 0) >>> 16 & 2) >>> 15) | 0;
     i31 = i28 >>> (i6 + 7 | 0) & 1 | i6 << 1;
    } else i31 = 0;
    i6 = 9332 + (i31 << 2) | 0;
    HEAP32[i9 + 28 >> 2] = i31;
    HEAP32[i9 + 20 >> 2] = 0;
    HEAP32[i9 + 16 >> 2] = 0;
    i13 = HEAP32[2258] | 0;
    i4 = 1 << i31;
    L168 : do if (i13 & i4) {
     i5 = HEAP32[i6 >> 2] | 0;
     L171 : do if ((HEAP32[i5 + 4 >> 2] & -8 | 0) != (i28 | 0)) {
      i15 = i28 << ((i31 | 0) == 31 ? 0 : 25 - (i31 >>> 1) | 0);
      i16 = i5;
      while (1) {
       i32 = i16 + 16 + (i15 >>> 31 << 2) | 0;
       i33 = HEAP32[i32 >> 2] | 0;
       if (!i33) {
        i34 = i16;
        break;
       }
       if ((HEAP32[i33 + 4 >> 2] & -8 | 0) == (i28 | 0)) {
        i35 = i33;
        break L171;
       } else {
        i15 = i15 << 1;
        i16 = i33;
       }
      }
      if (i32 >>> 0 < (HEAP32[2261] | 0) >>> 0) _abort(); else {
       HEAP32[i32 >> 2] = i9;
       HEAP32[i9 + 24 >> 2] = i34;
       HEAP32[i9 + 12 >> 2] = i9;
       HEAP32[i9 + 8 >> 2] = i9;
       break L168;
      }
     } else i35 = i5; while (0);
     i5 = i35 + 8 | 0;
     i7 = HEAP32[i5 >> 2] | 0;
     i11 = HEAP32[2261] | 0;
     if (i7 >>> 0 >= i11 >>> 0 & i35 >>> 0 >= i11 >>> 0) {
      HEAP32[i7 + 12 >> 2] = i9;
      HEAP32[i5 >> 2] = i9;
      HEAP32[i9 + 8 >> 2] = i7;
      HEAP32[i9 + 12 >> 2] = i35;
      HEAP32[i9 + 24 >> 2] = 0;
      break;
     } else _abort();
    } else {
     HEAP32[2258] = i13 | i4;
     HEAP32[i6 >> 2] = i9;
     HEAP32[i9 + 24 >> 2] = i6;
     HEAP32[i9 + 12 >> 2] = i9;
     HEAP32[i9 + 8 >> 2] = i9;
    } while (0);
    i6 = (HEAP32[2265] | 0) + -1 | 0;
    HEAP32[2265] = i6;
    if (!i6) i36 = 9484; else break L1;
    while (1) {
     i6 = HEAP32[i36 >> 2] | 0;
     if (!i6) break; else i36 = i6 + 8 | 0;
    }
    HEAP32[2265] = -1;
    break L1;
   }
  } while (0);
  _abort();
 } while (0);
 return;
}

function _inflate_table(i1, i2, i3, i4, i5, i6) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 i5 = i5 | 0;
 i6 = i6 | 0;
 var i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0, i24 = 0, i25 = 0, i26 = 0, i27 = 0, i28 = 0, i29 = 0, i30 = 0, i31 = 0, i32 = 0, i33 = 0, i34 = 0, i35 = 0, i36 = 0, i37 = 0, i38 = 0, i39 = 0, i40 = 0, i41 = 0, i42 = 0, i43 = 0, i44 = 0, i45 = 0, i46 = 0, i47 = 0, i48 = 0, i49 = 0, i50 = 0, i51 = 0;
 i7 = STACKTOP;
 STACKTOP = STACKTOP + 64 | 0;
 i8 = i7 + 32 | 0;
 i9 = i8 + 32 | 0;
 do {
  HEAP16[i8 >> 1] = 0;
  i8 = i8 + 2 | 0;
 } while ((i8 | 0) < (i9 | 0));
 if (i3) {
  i8 = 0;
  do {
   i9 = i7 + 32 + (HEAPU16[i2 + (i8 << 1) >> 1] << 1) | 0;
   HEAP16[i9 >> 1] = (HEAP16[i9 >> 1] | 0) + 1 << 16 >> 16;
   i8 = i8 + 1 | 0;
  } while ((i8 | 0) != (i3 | 0));
  i8 = HEAP16[i7 + 32 + 30 >> 1] | 0;
  i9 = HEAP32[i5 >> 2] | 0;
  if (!(i8 << 16 >> 16)) {
   i10 = i9;
   i11 = 5;
  } else {
   i12 = i9;
   i13 = i8;
   i14 = 15;
   i11 = 6;
  }
 } else {
  i10 = HEAP32[i5 >> 2] | 0;
  i11 = 5;
 }
 do if ((i11 | 0) == 5) if (!(HEAP16[i7 + 32 + 28 >> 1] | 0)) if (!(HEAP16[i7 + 32 + 26 >> 1] | 0)) if (!(HEAP16[i7 + 32 + 24 >> 1] | 0)) if (!(HEAP16[i7 + 32 + 22 >> 1] | 0)) if (!(HEAP16[i7 + 32 + 20 >> 1] | 0)) if (!(HEAP16[i7 + 32 + 18 >> 1] | 0)) if (!(HEAP16[i7 + 32 + 16 >> 1] | 0)) if (!(HEAP16[i7 + 32 + 14 >> 1] | 0)) if (!(HEAP16[i7 + 32 + 12 >> 1] | 0)) if (!(HEAP16[i7 + 32 + 10 >> 1] | 0)) if (!(HEAP16[i7 + 32 + 8 >> 1] | 0)) if (!(HEAP16[i7 + 32 + 6 >> 1] | 0)) if (!(HEAP16[i7 + 32 + 4 >> 1] | 0)) {
  if (HEAP16[i7 + 32 + 2 >> 1] | 0) {
   i15 = 0;
   i16 = i10 >>> 0 > 1 ? 1 : i10;
   i17 = 1;
   i18 = 1;
   break;
  }
  i8 = HEAP32[i4 >> 2] | 0;
  HEAP32[i4 >> 2] = i8 + 4;
  HEAP8[i8 >> 0] = 64;
  HEAP8[i8 + 1 >> 0] = 1;
  HEAP16[i8 + 2 >> 1] = 0;
  i8 = HEAP32[i4 >> 2] | 0;
  HEAP32[i4 >> 2] = i8 + 4;
  HEAP8[i8 >> 0] = 64;
  HEAP8[i8 + 1 >> 0] = 1;
  HEAP16[i8 + 2 >> 1] = 0;
  HEAP32[i5 >> 2] = 1;
  i19 = 0;
  STACKTOP = i7;
  return i19 | 0;
 } else {
  i12 = i10;
  i13 = 0;
  i14 = 2;
  i11 = 6;
 } else {
  i12 = i10;
  i13 = 0;
  i14 = 3;
  i11 = 6;
 } else {
  i12 = i10;
  i13 = 0;
  i14 = 4;
  i11 = 6;
 } else {
  i12 = i10;
  i13 = 0;
  i14 = 5;
  i11 = 6;
 } else {
  i12 = i10;
  i13 = 0;
  i14 = 6;
  i11 = 6;
 } else {
  i12 = i10;
  i13 = 0;
  i14 = 7;
  i11 = 6;
 } else {
  i12 = i10;
  i13 = 0;
  i14 = 8;
  i11 = 6;
 } else {
  i12 = i10;
  i13 = 0;
  i14 = 9;
  i11 = 6;
 } else {
  i12 = i10;
  i13 = 0;
  i14 = 10;
  i11 = 6;
 } else {
  i12 = i10;
  i13 = 0;
  i14 = 11;
  i11 = 6;
 } else {
  i12 = i10;
  i13 = 0;
  i14 = 12;
  i11 = 6;
 } else {
  i12 = i10;
  i13 = 0;
  i14 = 13;
  i11 = 6;
 } else {
  i12 = i10;
  i13 = 0;
  i14 = 14;
  i11 = 6;
 } while (0);
 L25 : do if ((i11 | 0) == 6) {
  i10 = i12 >>> 0 > i14 >>> 0 ? i14 : i12;
  i8 = 1;
  while (1) {
   if (HEAP16[i7 + 32 + (i8 << 1) >> 1] | 0) {
    i15 = i13;
    i16 = i10;
    i17 = i14;
    i18 = i8;
    break L25;
   }
   i9 = i8 + 1 | 0;
   if (i9 >>> 0 < i14 >>> 0) i8 = i9; else {
    i15 = i13;
    i16 = i10;
    i17 = i14;
    i18 = i9;
    break;
   }
  }
 } while (0);
 i14 = i16 >>> 0 < i18 >>> 0 ? i18 : i16;
 i16 = HEAP16[i7 + 32 + 2 >> 1] | 0;
 if ((2 - (i16 & 65535) | 0) < 0) {
  i19 = -1;
  STACKTOP = i7;
  return i19 | 0;
 }
 i13 = (2 - (i16 & 65535) << 1) - (HEAPU16[i7 + 32 + 4 >> 1] | 0) | 0;
 if ((i13 | 0) < 0) {
  i19 = -1;
  STACKTOP = i7;
  return i19 | 0;
 }
 i12 = (i13 << 1) - (HEAPU16[i7 + 32 + 6 >> 1] | 0) | 0;
 if ((i12 | 0) < 0) {
  i19 = -1;
  STACKTOP = i7;
  return i19 | 0;
 }
 i13 = (i12 << 1) - (HEAPU16[i7 + 32 + 8 >> 1] | 0) | 0;
 if ((i13 | 0) < 0) {
  i19 = -1;
  STACKTOP = i7;
  return i19 | 0;
 }
 i12 = (i13 << 1) - (HEAPU16[i7 + 32 + 10 >> 1] | 0) | 0;
 if ((i12 | 0) < 0) {
  i19 = -1;
  STACKTOP = i7;
  return i19 | 0;
 }
 i13 = (i12 << 1) - (HEAPU16[i7 + 32 + 12 >> 1] | 0) | 0;
 if ((i13 | 0) < 0) {
  i19 = -1;
  STACKTOP = i7;
  return i19 | 0;
 }
 i12 = (i13 << 1) - (HEAPU16[i7 + 32 + 14 >> 1] | 0) | 0;
 if ((i12 | 0) < 0) {
  i19 = -1;
  STACKTOP = i7;
  return i19 | 0;
 }
 i13 = (i12 << 1) - (HEAPU16[i7 + 32 + 16 >> 1] | 0) | 0;
 if ((i13 | 0) < 0) {
  i19 = -1;
  STACKTOP = i7;
  return i19 | 0;
 }
 i12 = (i13 << 1) - (HEAPU16[i7 + 32 + 18 >> 1] | 0) | 0;
 if ((i12 | 0) < 0) {
  i19 = -1;
  STACKTOP = i7;
  return i19 | 0;
 }
 i13 = (i12 << 1) - (HEAPU16[i7 + 32 + 20 >> 1] | 0) | 0;
 if ((i13 | 0) < 0) {
  i19 = -1;
  STACKTOP = i7;
  return i19 | 0;
 }
 i12 = (i13 << 1) - (HEAPU16[i7 + 32 + 22 >> 1] | 0) | 0;
 if ((i12 | 0) < 0) {
  i19 = -1;
  STACKTOP = i7;
  return i19 | 0;
 }
 i13 = (i12 << 1) - (HEAPU16[i7 + 32 + 24 >> 1] | 0) | 0;
 if ((i13 | 0) < 0) {
  i19 = -1;
  STACKTOP = i7;
  return i19 | 0;
 }
 i12 = (i13 << 1) - (HEAPU16[i7 + 32 + 26 >> 1] | 0) | 0;
 if ((i12 | 0) < 0) {
  i19 = -1;
  STACKTOP = i7;
  return i19 | 0;
 }
 i13 = (i12 << 1) - (HEAPU16[i7 + 32 + 28 >> 1] | 0) | 0;
 if ((i13 | 0) < 0) {
  i19 = -1;
  STACKTOP = i7;
  return i19 | 0;
 }
 i12 = (i13 << 1) - (i15 & 65535) | 0;
 if ((i12 | 0) < 0) {
  i19 = -1;
  STACKTOP = i7;
  return i19 | 0;
 }
 if ((i12 | 0) > 0 ? (i1 | 0) == 0 | (i17 | 0) != 1 : 0) {
  i19 = -1;
  STACKTOP = i7;
  return i19 | 0;
 }
 HEAP16[i7 + 2 >> 1] = 0;
 HEAP16[i7 + 4 >> 1] = i16;
 i12 = (HEAPU16[i7 + 32 + 4 >> 1] | 0) + (i16 & 65535) | 0;
 HEAP16[i7 + 6 >> 1] = i12;
 i16 = (HEAPU16[i7 + 32 + 6 >> 1] | 0) + (i12 & 65535) | 0;
 HEAP16[i7 + 8 >> 1] = i16;
 i12 = (HEAPU16[i7 + 32 + 8 >> 1] | 0) + (i16 & 65535) | 0;
 HEAP16[i7 + 10 >> 1] = i12;
 i16 = (HEAPU16[i7 + 32 + 10 >> 1] | 0) + (i12 & 65535) | 0;
 HEAP16[i7 + 12 >> 1] = i16;
 i12 = (HEAPU16[i7 + 32 + 12 >> 1] | 0) + (i16 & 65535) | 0;
 HEAP16[i7 + 14 >> 1] = i12;
 i16 = (HEAPU16[i7 + 32 + 14 >> 1] | 0) + (i12 & 65535) | 0;
 HEAP16[i7 + 16 >> 1] = i16;
 i12 = (HEAPU16[i7 + 32 + 16 >> 1] | 0) + (i16 & 65535) | 0;
 HEAP16[i7 + 18 >> 1] = i12;
 i16 = (HEAPU16[i7 + 32 + 18 >> 1] | 0) + (i12 & 65535) | 0;
 HEAP16[i7 + 20 >> 1] = i16;
 i12 = (HEAPU16[i7 + 32 + 20 >> 1] | 0) + (i16 & 65535) | 0;
 HEAP16[i7 + 22 >> 1] = i12;
 i16 = (HEAPU16[i7 + 32 + 22 >> 1] | 0) + (i12 & 65535) | 0;
 HEAP16[i7 + 24 >> 1] = i16;
 i12 = (HEAPU16[i7 + 32 + 24 >> 1] | 0) + (i16 & 65535) | 0;
 HEAP16[i7 + 26 >> 1] = i12;
 i16 = (HEAPU16[i7 + 32 + 26 >> 1] | 0) + (i12 & 65535) | 0;
 HEAP16[i7 + 28 >> 1] = i16;
 HEAP16[i7 + 30 >> 1] = (HEAPU16[i7 + 32 + 28 >> 1] | 0) + (i16 & 65535);
 if (i3) {
  i16 = 0;
  do {
   i12 = HEAP16[i2 + (i16 << 1) >> 1] | 0;
   if (i12 << 16 >> 16) {
    i15 = HEAP16[i7 + ((i12 & 65535) << 1) >> 1] | 0;
    HEAP16[i7 + ((i12 & 65535) << 1) >> 1] = i15 + 1 << 16 >> 16;
    HEAP16[i6 + ((i15 & 65535) << 1) >> 1] = i16;
   }
   i16 = i16 + 1 | 0;
  } while ((i16 | 0) != (i3 | 0));
 }
 switch (i1 | 0) {
 case 0:
  {
   i20 = 0;
   i21 = 0;
   i22 = i6;
   i23 = 19;
   i24 = i6;
   break;
  }
 case 1:
  {
   if (i14 >>> 0 > 9) {
    i19 = 1;
    STACKTOP = i7;
    return i19 | 0;
   } else {
    i20 = 0;
    i21 = 1;
    i22 = 12676;
    i23 = 256;
    i24 = 12802;
   }
   break;
  }
 default:
  if ((i1 | 0) == 2 & i14 >>> 0 > 9) {
   i19 = 1;
   STACKTOP = i7;
   return i19 | 0;
  } else {
   i20 = (i1 | 0) == 2;
   i21 = 0;
   i22 = 13126;
   i23 = -1;
   i24 = 13252;
  }
 }
 i1 = i14;
 i3 = 0;
 i16 = 0;
 i15 = i18;
 i18 = -1;
 i12 = HEAP32[i4 >> 2] | 0;
 i13 = 0;
 i10 = 1 << i14;
 L93 : while (1) {
  i8 = 1 << i1;
  i9 = i16;
  i25 = i15;
  i26 = i13;
  while (1) {
   i27 = i25 - i3 | 0;
   i28 = HEAP16[i6 + (i26 << 1) >> 1] | 0;
   do if ((i28 & 65535 | 0) < (i23 | 0)) {
    i29 = 0;
    i30 = i28;
   } else {
    if ((i28 & 65535 | 0) <= (i23 | 0)) {
     i29 = 96;
     i30 = 0;
     break;
    }
    i29 = HEAP16[i24 + ((i28 & 65535) << 1) >> 1] & 255;
    i30 = HEAP16[i22 + ((i28 & 65535) << 1) >> 1] | 0;
   } while (0);
   i28 = i9 >>> i3;
   i31 = i8;
   do {
    i32 = i31;
    i31 = i31 - (1 << i27) | 0;
    i33 = i31 + i28 | 0;
    HEAP8[i12 + (i33 << 2) >> 0] = i29;
    HEAP8[i12 + (i33 << 2) + 1 >> 0] = i27;
    HEAP16[i12 + (i33 << 2) + 2 >> 1] = i30;
   } while ((i32 | 0) != (1 << i27 | 0));
   i28 = 1 << i25 + -1;
   while (1) if (!(i28 & i9)) {
    i34 = i28;
    break;
   } else i28 = i28 >>> 1;
   if (!i34) i35 = 0; else i35 = (i34 + -1 & i9) + i34 | 0;
   i36 = i26 + 1 | 0;
   i28 = i7 + 32 + (i25 << 1) | 0;
   i31 = (HEAP16[i28 >> 1] | 0) + -1 << 16 >> 16;
   HEAP16[i28 >> 1] = i31;
   if (!(i31 << 16 >> 16)) {
    if ((i25 | 0) == (i17 | 0)) {
     i37 = i3;
     i38 = i35;
     i39 = i18;
     i40 = i12;
     i41 = i10;
     break L93;
    }
    i42 = HEAPU16[i2 + (HEAPU16[i6 + (i36 << 1) >> 1] << 1) >> 1] | 0;
   } else i42 = i25;
   if (i42 >>> 0 <= i14 >>> 0) {
    i9 = i35;
    i25 = i42;
    i26 = i36;
    continue;
   }
   i43 = i35 & (1 << i14) + -1;
   if ((i43 | 0) == (i18 | 0)) {
    i9 = i35;
    i25 = i42;
    i26 = i36;
   } else {
    i44 = i35;
    i45 = i42;
    break;
   }
  }
  i26 = (i3 | 0) == 0 ? i14 : i3;
  i25 = i12 + (i8 << 2) | 0;
  L116 : do if (i45 >>> 0 < i17 >>> 0) {
   i9 = i45;
   i31 = i45 - i26 | 0;
   i28 = 1 << i45 - i26;
   while (1) {
    i32 = i28 - (HEAPU16[i7 + 32 + (i9 << 1) >> 1] | 0) | 0;
    if ((i32 | 0) < 1) {
     i46 = i31;
     break L116;
    }
    i33 = i31 + 1 | 0;
    i9 = i33 + i26 | 0;
    if (i9 >>> 0 >= i17 >>> 0) {
     i46 = i33;
     break;
    } else {
     i31 = i33;
     i28 = i32 << 1;
    }
   }
  } else i46 = i45 - i26 | 0; while (0);
  i8 = (1 << i46) + i10 | 0;
  if (i21 & i8 >>> 0 > 851 | i20 & i8 >>> 0 > 591) {
   i19 = 1;
   i11 = 49;
   break;
  }
  i28 = HEAP32[i4 >> 2] | 0;
  HEAP8[i28 + (i43 << 2) >> 0] = i46;
  HEAP8[i28 + (i43 << 2) + 1 >> 0] = i14;
  HEAP16[i28 + (i43 << 2) + 2 >> 1] = (i25 - i28 | 0) >>> 2;
  i1 = i46;
  i3 = i26;
  i16 = i44;
  i15 = i45;
  i18 = i43;
  i12 = i25;
  i13 = i36;
  i10 = i8;
 }
 if ((i11 | 0) == 49) {
  STACKTOP = i7;
  return i19 | 0;
 }
 L126 : do if (i38) {
  i11 = i37;
  i10 = i27 & 255;
  i36 = i38;
  i13 = i17;
  i12 = i40;
  while (1) {
   if ((i11 | 0) == 0 ? 1 : (i36 & (1 << i14) + -1 | 0) == (i39 | 0)) {
    i47 = i11;
    i48 = i10;
    i49 = i13;
    i50 = i12;
   } else {
    i47 = 0;
    i48 = i14 & 255;
    i49 = i14;
    i50 = HEAP32[i4 >> 2] | 0;
   }
   i43 = i36 >>> i47;
   HEAP8[i50 + (i43 << 2) >> 0] = 64;
   HEAP8[i50 + (i43 << 2) + 1 >> 0] = i48;
   HEAP16[i50 + (i43 << 2) + 2 >> 1] = 0;
   i43 = 1 << i49 + -1;
   while (1) if (!(i43 & i36)) {
    i51 = i43;
    break;
   } else i43 = i43 >>> 1;
   if (!i51) break L126;
   i36 = (i51 + -1 & i36) + i51 | 0;
   if (!i36) break; else {
    i11 = i47;
    i10 = i48;
    i13 = i49;
    i12 = i50;
   }
  }
 } while (0);
 HEAP32[i4 >> 2] = (HEAP32[i4 >> 2] | 0) + (i41 << 2);
 HEAP32[i5 >> 2] = i14;
 i19 = 0;
 STACKTOP = i7;
 return i19 | 0;
}

function _inflate_fast(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0, i24 = 0, i25 = 0, i26 = 0, i27 = 0, i28 = 0, i29 = 0, i30 = 0, i31 = 0, i32 = 0, i33 = 0, i34 = 0, i35 = 0, i36 = 0, i37 = 0, i38 = 0, i39 = 0, i40 = 0, i41 = 0, i42 = 0, i43 = 0, i44 = 0, i45 = 0, i46 = 0, i47 = 0, i48 = 0, i49 = 0, i50 = 0, i51 = 0, i52 = 0, i53 = 0, i54 = 0, i55 = 0, i56 = 0, i57 = 0, i58 = 0, i59 = 0, i60 = 0, i61 = 0, i62 = 0, i63 = 0, i64 = 0, i65 = 0, i66 = 0, i67 = 0, i68 = 0, i69 = 0, i70 = 0, i71 = 0, i72 = 0, i73 = 0, i74 = 0, i75 = 0, i76 = 0, i77 = 0, i78 = 0, i79 = 0, i80 = 0, i81 = 0, i82 = 0, i83 = 0, i84 = 0, i85 = 0, i86 = 0, i87 = 0, i88 = 0, i89 = 0, i90 = 0, i91 = 0, i92 = 0, i93 = 0;
 i3 = HEAP32[i1 + 28 >> 2] | 0;
 i4 = HEAP32[i1 >> 2] | 0;
 i5 = i4 + ((HEAP32[i1 + 4 >> 2] | 0) + -6) | 0;
 i6 = HEAP32[i1 + 12 >> 2] | 0;
 i7 = HEAP32[i1 + 16 >> 2] | 0;
 i8 = HEAP32[i3 + 44 >> 2] | 0;
 i9 = HEAP32[i3 + 48 >> 2] | 0;
 i10 = HEAP32[i3 + 52 >> 2] | 0;
 i11 = HEAP32[i3 + 76 >> 2] | 0;
 i12 = HEAP32[i3 + 80 >> 2] | 0;
 i13 = (1 << HEAP32[i3 + 84 >> 2]) + -1 | 0;
 i14 = (1 << HEAP32[i3 + 88 >> 2]) + -1 | 0;
 i15 = (HEAP32[i3 + 40 >> 2] | 0) + -1 | 0;
 i16 = HEAP32[i3 + 60 >> 2] | 0;
 i17 = HEAP32[i3 + 56 >> 2] | 0;
 i18 = i4 + -1 | 0;
 i4 = i6 + -1 | 0;
 L1 : while (1) {
  if (i16 >>> 0 < 15) {
   i19 = i18 + 2 | 0;
   i20 = i16 + 16 | 0;
   i21 = ((HEAPU8[i18 + 1 >> 0] | 0) << i16) + i17 + ((HEAPU8[i19 >> 0] | 0) << i16 + 8) | 0;
   i22 = i19;
  } else {
   i20 = i16;
   i21 = i17;
   i22 = i18;
  }
  i19 = i21 & i13;
  i23 = HEAP8[i11 + (i19 << 2) >> 0] | 0;
  i24 = HEAP16[i11 + (i19 << 2) + 2 >> 1] | 0;
  i25 = HEAPU8[i11 + (i19 << 2) + 1 >> 0] | 0;
  i19 = i21 >>> i25;
  i26 = i20 - i25 | 0;
  do if (i23 << 24 >> 24) {
   i25 = i19;
   i27 = i26;
   i28 = i23 & 255;
   i29 = i24;
   while (1) {
    if (i28 & 16) {
     i30 = i28;
     i31 = i27;
     i32 = i25;
     i33 = i29;
     break;
    }
    if (i28 & 64) {
     i34 = i28;
     i35 = i27;
     i36 = i25;
     i37 = i22;
     i38 = i4;
     i39 = 57;
     break L1;
    }
    i40 = (i25 & (1 << i28) + -1) + (i29 & 65535) | 0;
    i41 = HEAP8[i11 + (i40 << 2) >> 0] | 0;
    i42 = HEAP16[i11 + (i40 << 2) + 2 >> 1] | 0;
    i43 = HEAPU8[i11 + (i40 << 2) + 1 >> 0] | 0;
    i44 = i25 >>> i43;
    i45 = i27 - i43 | 0;
    if (!(i41 << 24 >> 24)) {
     i39 = 6;
     break;
    } else {
     i25 = i44;
     i27 = i45;
     i28 = i41 & 255;
     i29 = i42;
    }
   }
   if ((i39 | 0) == 6) {
    i39 = 0;
    i46 = i44;
    i47 = i45;
    i48 = i42 & 255;
    i39 = 7;
    break;
   }
   if (!(i30 & 15)) {
    i49 = i31;
    i50 = i32;
    i51 = i22;
    i52 = i33 & 65535;
   } else {
    if (i31 >>> 0 < (i30 & 15) >>> 0) {
     i29 = i22 + 1 | 0;
     i53 = i31 + 8 | 0;
     i54 = ((HEAPU8[i29 >> 0] | 0) << i31) + i32 | 0;
     i55 = i29;
    } else {
     i53 = i31;
     i54 = i32;
     i55 = i22;
    }
    i49 = i53 - (i30 & 15) | 0;
    i50 = i54 >>> (i30 & 15);
    i51 = i55;
    i52 = (i54 & (1 << (i30 & 15)) + -1) + (i33 & 65535) | 0;
   }
   if (i49 >>> 0 < 15) {
    i29 = i51 + 2 | 0;
    i56 = i49 + 16 | 0;
    i57 = ((HEAPU8[i51 + 1 >> 0] | 0) << i49) + i50 + ((HEAPU8[i29 >> 0] | 0) << i49 + 8) | 0;
    i58 = i29;
   } else {
    i56 = i49;
    i57 = i50;
    i58 = i51;
   }
   i29 = i57 & i14;
   i28 = HEAP16[i12 + (i29 << 2) + 2 >> 1] | 0;
   i27 = HEAPU8[i12 + (i29 << 2) + 1 >> 0] | 0;
   i25 = i57 >>> i27;
   i41 = i56 - i27 | 0;
   i27 = HEAPU8[i12 + (i29 << 2) >> 0] | 0;
   if (!(i27 & 16)) {
    i29 = i27;
    i43 = i25;
    i40 = i41;
    i59 = i28;
    while (1) {
     if (i29 & 64) {
      i60 = i40;
      i61 = i43;
      i62 = i58;
      i63 = i4;
      i39 = 54;
      break L1;
     }
     i64 = (i43 & (1 << i29) + -1) + (i59 & 65535) | 0;
     i65 = HEAP16[i12 + (i64 << 2) + 2 >> 1] | 0;
     i66 = HEAPU8[i12 + (i64 << 2) + 1 >> 0] | 0;
     i67 = i43 >>> i66;
     i68 = i40 - i66 | 0;
     i66 = HEAPU8[i12 + (i64 << 2) >> 0] | 0;
     if (!(i66 & 16)) {
      i29 = i66;
      i43 = i67;
      i40 = i68;
      i59 = i65;
     } else {
      i69 = i67;
      i70 = i68;
      i71 = i66;
      i72 = i65;
      break;
     }
    }
   } else {
    i69 = i25;
    i70 = i41;
    i71 = i27;
    i72 = i28;
   }
   i59 = i72 & 65535;
   i40 = i71 & 15;
   if (i70 >>> 0 < i40 >>> 0) {
    i43 = i58 + 1 | 0;
    i29 = ((HEAPU8[i43 >> 0] | 0) << i70) + i69 | 0;
    i65 = i70 + 8 | 0;
    if (i65 >>> 0 < i40 >>> 0) {
     i66 = i58 + 2 | 0;
     i73 = i70 + 16 | 0;
     i74 = ((HEAPU8[i66 >> 0] | 0) << i65) + i29 | 0;
     i75 = i66;
    } else {
     i73 = i65;
     i74 = i29;
     i75 = i43;
    }
   } else {
    i73 = i70;
    i74 = i69;
    i75 = i58;
   }
   i43 = (i74 & (1 << i40) + -1) + i59 | 0;
   i76 = i74 >>> i40;
   i77 = i73 - i40 | 0;
   i40 = i4;
   if (i43 >>> 0 <= (i40 - (i6 + (i7 + ~i2)) | 0) >>> 0) {
    i59 = 2 - i52 | 0;
    i29 = i52 + (i59 >>> 0 > 4294967293 ? i59 : -3) | 0;
    i59 = i4 + (0 - i43) | 0;
    i65 = i52;
    i66 = i4;
    do {
     HEAP8[i66 + 1 >> 0] = HEAP8[i59 + 1 >> 0] | 0;
     HEAP8[i66 + 2 >> 0] = HEAP8[i59 + 2 >> 0] | 0;
     i59 = i59 + 3 | 0;
     i66 = i66 + 3 | 0;
     HEAP8[i66 >> 0] = HEAP8[i59 >> 0] | 0;
     i65 = i65 + -3 | 0;
    } while (i65 >>> 0 > 2);
    i65 = i52 + -3 | 0;
    if ((i65 | 0) == (i29 - ((i29 >>> 0) % 3 | 0) | 0)) {
     i78 = i77;
     i79 = i76;
     i80 = i75;
     i81 = i4 + (i29 - ((i29 >>> 0) % 3 | 0) + 3) | 0;
     break;
    }
    i59 = i4 + (i29 - ((i29 >>> 0) % 3 | 0) + 4) | 0;
    HEAP8[i59 >> 0] = HEAP8[i4 + (i29 - ((i29 >>> 0) % 3 | 0) - i43 + 4) >> 0] | 0;
    if ((i65 - (i29 - ((i29 >>> 0) % 3 | 0)) | 0) >>> 0 <= 1) {
     i78 = i77;
     i79 = i76;
     i80 = i75;
     i81 = i59;
     break;
    }
    i59 = i4 + (i29 - ((i29 >>> 0) % 3 | 0) + 5) | 0;
    HEAP8[i59 >> 0] = HEAP8[i4 + (i29 - ((i29 >>> 0) % 3 | 0) - i43 + 5) >> 0] | 0;
    i78 = i77;
    i79 = i76;
    i80 = i75;
    i81 = i59;
    break;
   }
   i59 = i43 - (i40 - (i6 + (i7 + ~i2))) | 0;
   if (i59 >>> 0 > i8 >>> 0 ? (HEAP32[i3 + 7104 >> 2] | 0) != 0 : 0) {
    i82 = i75;
    i83 = i4;
    i39 = 22;
    break L1;
   }
   do if (!i9) if (i52 >>> 0 > i59 >>> 0) {
    i65 = i52 - i59 | 0;
    i66 = i10 + (i15 - i59) | 0;
    i28 = i59;
    i27 = i4;
    do {
     i66 = i66 + 1 | 0;
     i27 = i27 + 1 | 0;
     HEAP8[i27 >> 0] = HEAP8[i66 >> 0] | 0;
     i28 = i28 + -1 | 0;
    } while ((i28 | 0) != 0);
    i84 = i4 + (i6 + (i7 + ~i2) + -1 + (i43 - i40) + (1 - i43)) | 0;
    i85 = i65;
    i86 = i4 + (i6 + (i7 + ~i2) + (i43 - i40)) | 0;
   } else {
    i84 = i10 + (i15 - i59) | 0;
    i85 = i52;
    i86 = i4;
   } else {
    if (i59 >>> 0 <= i9 >>> 0) {
     if (i52 >>> 0 <= i59 >>> 0) {
      i84 = i10 + (i9 + -1 - i59) | 0;
      i85 = i52;
      i86 = i4;
      break;
     }
     i28 = i52 - i59 | 0;
     i66 = i10 + (i9 + -1 - i59) | 0;
     i27 = i59;
     i41 = i4;
     do {
      i66 = i66 + 1 | 0;
      i41 = i41 + 1 | 0;
      HEAP8[i41 >> 0] = HEAP8[i66 >> 0] | 0;
      i27 = i27 + -1 | 0;
     } while ((i27 | 0) != 0);
     i84 = i4 + (i6 + (i7 + ~i2) + -1 + (i43 - i40) + (1 - i43)) | 0;
     i85 = i28;
     i86 = i4 + (i6 + (i7 + ~i2) + (i43 - i40)) | 0;
     break;
    }
    if (i52 >>> 0 > (i59 - i9 | 0) >>> 0) {
     i27 = i52 - (i59 - i9) | 0;
     i66 = i10 + (i15 + i9 - i59) | 0;
     i41 = i59 - i9 | 0;
     i65 = i4;
     do {
      i66 = i66 + 1 | 0;
      i65 = i65 + 1 | 0;
      HEAP8[i65 >> 0] = HEAP8[i66 >> 0] | 0;
      i41 = i41 + -1 | 0;
     } while ((i41 | 0) != 0);
     i41 = i4 + (i6 + (i7 + ~i2) - i9 + (i43 - i40)) | 0;
     if (i27 >>> 0 > i9 >>> 0) {
      i66 = i10 + -1 | 0;
      i65 = i9;
      i28 = i41;
      do {
       i66 = i66 + 1 | 0;
       i28 = i28 + 1 | 0;
       HEAP8[i28 >> 0] = HEAP8[i66 >> 0] | 0;
       i65 = i65 + -1 | 0;
      } while ((i65 | 0) != 0);
      i84 = i4 + (i6 + (i7 + ~i2) + -2 + (i43 - i40) + (2 - i43)) | 0;
      i85 = i27 - i9 | 0;
      i86 = i4 + (i6 + (i7 + ~i2) + (i43 - i40)) | 0;
     } else {
      i84 = i10 + -1 | 0;
      i85 = i27;
      i86 = i41;
     }
    } else {
     i84 = i10 + (i15 + i9 - i59) | 0;
     i85 = i52;
     i86 = i4;
    }
   } while (0);
   if (i85 >>> 0 > 2) {
    i59 = i85 + -3 | 0;
    i40 = i84;
    i43 = i85;
    i29 = i86;
    do {
     HEAP8[i29 + 1 >> 0] = HEAP8[i40 + 1 >> 0] | 0;
     HEAP8[i29 + 2 >> 0] = HEAP8[i40 + 2 >> 0] | 0;
     i40 = i40 + 3 | 0;
     i29 = i29 + 3 | 0;
     HEAP8[i29 >> 0] = HEAP8[i40 >> 0] | 0;
     i43 = i43 + -3 | 0;
    } while (i43 >>> 0 > 2);
    i87 = i84 + (i59 - ((i59 >>> 0) % 3 | 0) + 3) | 0;
    i88 = (i59 >>> 0) % 3 | 0;
    i89 = i86 + (i59 - ((i59 >>> 0) % 3 | 0) + 3) | 0;
   } else {
    i87 = i84;
    i88 = i85;
    i89 = i86;
   }
   if (i88) {
    i43 = i89 + 1 | 0;
    HEAP8[i43 >> 0] = HEAP8[i87 + 1 >> 0] | 0;
    if (i88 >>> 0 > 1) {
     i40 = i89 + 2 | 0;
     HEAP8[i40 >> 0] = HEAP8[i87 + 2 >> 0] | 0;
     i78 = i77;
     i79 = i76;
     i80 = i75;
     i81 = i40;
    } else {
     i78 = i77;
     i79 = i76;
     i80 = i75;
     i81 = i43;
    }
   } else {
    i78 = i77;
    i79 = i76;
    i80 = i75;
    i81 = i89;
   }
  } else {
   i46 = i19;
   i47 = i26;
   i48 = i24 & 255;
   i39 = 7;
  } while (0);
  if ((i39 | 0) == 7) {
   i39 = 0;
   i24 = i4 + 1 | 0;
   HEAP8[i24 >> 0] = i48;
   i78 = i47;
   i79 = i46;
   i80 = i22;
   i81 = i24;
  }
  if (i81 >>> 0 < (i6 + (i7 + -258) | 0) >>> 0 & i80 >>> 0 < i5 >>> 0) {
   i16 = i78;
   i17 = i79;
   i18 = i80;
   i4 = i81;
  } else {
   i90 = i78;
   i91 = i79;
   i92 = i80;
   i93 = i81;
   break;
  }
 }
 do if ((i39 | 0) == 22) {
  HEAP32[i1 + 24 >> 2] = 14753;
  HEAP32[i3 >> 2] = 29;
  i90 = i77;
  i91 = i76;
  i92 = i82;
  i93 = i83;
 } else if ((i39 | 0) == 54) {
  HEAP32[i1 + 24 >> 2] = 14783;
  HEAP32[i3 >> 2] = 29;
  i90 = i60;
  i91 = i61;
  i92 = i62;
  i93 = i63;
 } else if ((i39 | 0) == 57) if (!(i34 & 32)) {
  HEAP32[i1 + 24 >> 2] = 14805;
  HEAP32[i3 >> 2] = 29;
  i90 = i35;
  i91 = i36;
  i92 = i37;
  i93 = i38;
  break;
 } else {
  HEAP32[i3 >> 2] = 11;
  i90 = i35;
  i91 = i36;
  i92 = i37;
  i93 = i38;
  break;
 } while (0);
 i38 = i90 >>> 3;
 i37 = i90 - (i38 << 3) | 0;
 HEAP32[i1 >> 2] = i92 + (1 - i38);
 HEAP32[i1 + 12 >> 2] = i93 + 1;
 HEAP32[i1 + 4 >> 2] = i5 + 5 - (i92 + (0 - i38));
 HEAP32[i1 + 16 >> 2] = i6 + (i7 + -258) + 257 - i93;
 HEAP32[i3 + 56 >> 2] = (1 << i37) + -1 & i91;
 HEAP32[i3 + 60 >> 2] = i37;
 return;
}

function __tr_flush_block(i1, i2, i3, i4) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 var i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0, i24 = 0, i25 = 0, i26 = 0, i27 = 0, i28 = 0, i29 = 0, i30 = 0, i31 = 0, i32 = 0, i33 = 0, i34 = 0, i35 = 0, i36 = 0, i37 = 0, i38 = 0;
 if ((HEAP32[i1 + 132 >> 2] | 0) > 0) {
  i5 = (HEAP32[i1 >> 2] | 0) + 44 | 0;
  if ((HEAP32[i5 >> 2] | 0) == 2) {
   i6 = -201342849;
   i7 = 0;
   while (1) {
    if ((i6 & 1 | 0) != 0 ? (HEAP16[i1 + 148 + (i7 << 2) >> 1] | 0) != 0 : 0) {
     i8 = 0;
     break;
    }
    i7 = i7 + 1 | 0;
    if ((i7 | 0) >= 32) {
     i9 = 6;
     break;
    } else i6 = i6 >>> 1;
   }
   L9 : do if ((i9 | 0) == 6) if (((HEAP16[i1 + 184 >> 1] | 0) == 0 ? (HEAP16[i1 + 188 >> 1] | 0) == 0 : 0) ? (HEAP16[i1 + 200 >> 1] | 0) == 0 : 0) {
    i6 = 32;
    while (1) {
     if (HEAP16[i1 + 148 + (i6 << 2) >> 1] | 0) {
      i8 = 1;
      break L9;
     }
     i6 = i6 + 1 | 0;
     if ((i6 | 0) >= 256) {
      i8 = 0;
      break;
     }
    }
   } else i8 = 1; while (0);
   HEAP32[i5 >> 2] = i8;
  }
  _build_tree(i1, i1 + 2840 | 0);
  _build_tree(i1, i1 + 2852 | 0);
  i8 = HEAP32[i1 + 2844 >> 2] | 0;
  i5 = HEAP16[i1 + 150 >> 1] | 0;
  HEAP16[i1 + 148 + (i8 + 1 << 2) + 2 >> 1] = -1;
  i9 = i5 << 16 >> 16 == 0 ? 138 : 7;
  i6 = i5 << 16 >> 16 == 0 ? 3 : 4;
  i7 = 0;
  i10 = i5 & 65535;
  i5 = -1;
  L18 : while (1) {
   i11 = 0;
   i12 = i7;
   while (1) {
    if ((i12 | 0) > (i8 | 0)) break L18;
    i13 = i12 + 1 | 0;
    i14 = HEAP16[i1 + 148 + (i13 << 2) + 2 >> 1] | 0;
    i15 = i11 + 1 | 0;
    i16 = (i10 | 0) == (i14 & 65535 | 0);
    if ((i15 | 0) < (i9 | 0) & i16) {
     i11 = i15;
     i12 = i13;
    } else break;
   }
   do if ((i15 | 0) >= (i6 | 0)) if (!i10) if ((i15 | 0) < 11) {
    HEAP16[i1 + 2752 >> 1] = (HEAP16[i1 + 2752 >> 1] | 0) + 1 << 16 >> 16;
    break;
   } else {
    HEAP16[i1 + 2756 >> 1] = (HEAP16[i1 + 2756 >> 1] | 0) + 1 << 16 >> 16;
    break;
   } else {
    if ((i10 | 0) != (i5 | 0)) {
     i12 = i1 + 2684 + (i10 << 2) | 0;
     HEAP16[i12 >> 1] = (HEAP16[i12 >> 1] | 0) + 1 << 16 >> 16;
    }
    HEAP16[i1 + 2748 >> 1] = (HEAP16[i1 + 2748 >> 1] | 0) + 1 << 16 >> 16;
    break;
   } else {
    i12 = i1 + 2684 + (i10 << 2) | 0;
    HEAP16[i12 >> 1] = (HEAPU16[i12 >> 1] | 0) + i15;
   } while (0);
   i12 = i10;
   i9 = i14 << 16 >> 16 == 0 ? 138 : i16 ? 6 : 7;
   i6 = i14 << 16 >> 16 == 0 | i16 ? 3 : 4;
   i7 = i13;
   i10 = i14 & 65535;
   i5 = i12;
  }
  i5 = HEAP32[i1 + 2856 >> 2] | 0;
  i14 = HEAP16[i1 + 2442 >> 1] | 0;
  HEAP16[i1 + 2440 + (i5 + 1 << 2) + 2 >> 1] = -1;
  i10 = i14 << 16 >> 16 == 0 ? 138 : 7;
  i13 = i14 << 16 >> 16 == 0 ? 3 : 4;
  i7 = 0;
  i16 = i14 & 65535;
  i14 = -1;
  L38 : while (1) {
   i6 = 0;
   i9 = i7;
   while (1) {
    if ((i9 | 0) > (i5 | 0)) break L38;
    i17 = i9 + 1 | 0;
    i18 = HEAP16[i1 + 2440 + (i17 << 2) + 2 >> 1] | 0;
    i19 = i6 + 1 | 0;
    i20 = (i16 | 0) == (i18 & 65535 | 0);
    if ((i19 | 0) < (i10 | 0) & i20) {
     i6 = i19;
     i9 = i17;
    } else break;
   }
   do if ((i19 | 0) >= (i13 | 0)) if (!i16) if ((i19 | 0) < 11) {
    HEAP16[i1 + 2752 >> 1] = (HEAP16[i1 + 2752 >> 1] | 0) + 1 << 16 >> 16;
    break;
   } else {
    HEAP16[i1 + 2756 >> 1] = (HEAP16[i1 + 2756 >> 1] | 0) + 1 << 16 >> 16;
    break;
   } else {
    if ((i16 | 0) != (i14 | 0)) {
     i9 = i1 + 2684 + (i16 << 2) | 0;
     HEAP16[i9 >> 1] = (HEAP16[i9 >> 1] | 0) + 1 << 16 >> 16;
    }
    HEAP16[i1 + 2748 >> 1] = (HEAP16[i1 + 2748 >> 1] | 0) + 1 << 16 >> 16;
    break;
   } else {
    i9 = i1 + 2684 + (i16 << 2) | 0;
    HEAP16[i9 >> 1] = (HEAPU16[i9 >> 1] | 0) + i19;
   } while (0);
   i9 = i16;
   i10 = i18 << 16 >> 16 == 0 ? 138 : i20 ? 6 : 7;
   i13 = i18 << 16 >> 16 == 0 | i20 ? 3 : 4;
   i7 = i17;
   i16 = i18 & 65535;
   i14 = i9;
  }
  _build_tree(i1, i1 + 2864 | 0);
  if (!(HEAP16[i1 + 2746 >> 1] | 0)) if (!(HEAP16[i1 + 2690 >> 1] | 0)) if (!(HEAP16[i1 + 2742 >> 1] | 0)) if (!(HEAP16[i1 + 2694 >> 1] | 0)) if (!(HEAP16[i1 + 2738 >> 1] | 0)) if (!(HEAP16[i1 + 2698 >> 1] | 0)) if (!(HEAP16[i1 + 2734 >> 1] | 0)) if (!(HEAP16[i1 + 2702 >> 1] | 0)) if (!(HEAP16[i1 + 2730 >> 1] | 0)) if (!(HEAP16[i1 + 2706 >> 1] | 0)) if (!(HEAP16[i1 + 2726 >> 1] | 0)) if (!(HEAP16[i1 + 2710 >> 1] | 0)) if (!(HEAP16[i1 + 2722 >> 1] | 0)) if (!(HEAP16[i1 + 2714 >> 1] | 0)) if (!(HEAP16[i1 + 2718 >> 1] | 0)) i21 = (HEAP16[i1 + 2686 >> 1] | 0) == 0 ? 2 : 3; else i21 = 4; else i21 = 5; else i21 = 6; else i21 = 7; else i21 = 8; else i21 = 9; else i21 = 10; else i21 = 11; else i21 = 12; else i21 = 13; else i21 = 14; else i21 = 15; else i21 = 16; else i21 = 17; else i21 = 18;
  i14 = (i21 * 3 | 0) + 17 + (HEAP32[i1 + 5800 >> 2] | 0) | 0;
  HEAP32[i1 + 5800 >> 2] = i14;
  i18 = ((HEAP32[i1 + 5804 >> 2] | 0) + 10 | 0) >>> 3;
  i22 = i21;
  i23 = i18 >>> 0 > (i14 + 10 | 0) >>> 3 >>> 0 ? (i14 + 10 | 0) >>> 3 : i18;
  i24 = i18;
 } else {
  i22 = 0;
  i23 = i3 + 5 | 0;
  i24 = i3 + 5 | 0;
 }
 do if ((i2 | 0) != 0 & (i3 + 4 | 0) >>> 0 <= i23 >>> 0) __tr_stored_block(i1, i2, i3, i4); else {
  i18 = HEAP32[i1 + 5820 >> 2] | 0;
  if ((i24 | 0) == (i23 | 0) ? 1 : (HEAP32[i1 + 136 >> 2] | 0) == 4) {
   i14 = HEAPU16[i1 + 5816 >> 1] | (i4 + 2 & 65535) << i18;
   HEAP16[i1 + 5816 >> 1] = i14;
   if ((i18 | 0) > 13) {
    i21 = HEAP32[i1 + 20 >> 2] | 0;
    HEAP32[i1 + 20 >> 2] = i21 + 1;
    HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i21 >> 0] = i14;
    i14 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
    i21 = HEAP32[i1 + 20 >> 2] | 0;
    HEAP32[i1 + 20 >> 2] = i21 + 1;
    HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i21 >> 0] = i14;
    i14 = HEAP32[i1 + 5820 >> 2] | 0;
    HEAP16[i1 + 5816 >> 1] = (i4 + 2 & 65535) >>> (16 - i14 | 0);
    i25 = i14 + -13 | 0;
   } else i25 = i18 + 3 | 0;
   HEAP32[i1 + 5820 >> 2] = i25;
   _compress_block(i1, 9640, 10792);
   break;
  }
  i14 = HEAPU16[i1 + 5816 >> 1] | (i4 + 4 & 65535) << i18;
  HEAP16[i1 + 5816 >> 1] = i14;
  if ((i18 | 0) > 13) {
   i21 = HEAP32[i1 + 20 >> 2] | 0;
   HEAP32[i1 + 20 >> 2] = i21 + 1;
   HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i21 >> 0] = i14;
   i21 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
   i16 = HEAP32[i1 + 20 >> 2] | 0;
   HEAP32[i1 + 20 >> 2] = i16 + 1;
   HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i16 >> 0] = i21;
   i21 = HEAP32[i1 + 5820 >> 2] | 0;
   HEAP16[i1 + 5816 >> 1] = (i4 + 4 & 65535) >>> (16 - i21 | 0);
   i26 = (i4 + 4 & 65535) >>> (16 - i21 | 0);
   i27 = i21 + -13 | 0;
  } else {
   i26 = i14;
   i27 = i18 + 3 | 0;
  }
  HEAP32[i1 + 5820 >> 2] = i27;
  i18 = HEAP32[i1 + 2844 >> 2] | 0;
  i14 = HEAP32[i1 + 2856 >> 2] | 0;
  i21 = i26 & 65535 | (i18 + 65280 & 65535) << i27;
  HEAP16[i1 + 5816 >> 1] = i21;
  if ((i27 | 0) > 11) {
   i16 = HEAP32[i1 + 20 >> 2] | 0;
   HEAP32[i1 + 20 >> 2] = i16 + 1;
   HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i16 >> 0] = i21;
   i16 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
   i17 = HEAP32[i1 + 20 >> 2] | 0;
   HEAP32[i1 + 20 >> 2] = i17 + 1;
   HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i17 >> 0] = i16;
   i16 = HEAP32[i1 + 5820 >> 2] | 0;
   HEAP16[i1 + 5816 >> 1] = (i18 + 65280 & 65535) >>> (16 - i16 | 0);
   i28 = i16 + -11 | 0;
   i29 = (i18 + 65280 & 65535) >>> (16 - i16 | 0);
  } else {
   i28 = i27 + 5 | 0;
   i29 = i21;
  }
  HEAP32[i1 + 5820 >> 2] = i28;
  i21 = (i14 & 65535) << i28 | i29 & 65535;
  HEAP16[i1 + 5816 >> 1] = i21;
  if ((i28 | 0) > 11) {
   i16 = HEAP32[i1 + 20 >> 2] | 0;
   HEAP32[i1 + 20 >> 2] = i16 + 1;
   HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i16 >> 0] = i21;
   i16 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
   i17 = HEAP32[i1 + 20 >> 2] | 0;
   HEAP32[i1 + 20 >> 2] = i17 + 1;
   HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i17 >> 0] = i16;
   i16 = HEAP32[i1 + 5820 >> 2] | 0;
   HEAP16[i1 + 5816 >> 1] = (i14 & 65535) >>> (16 - i16 | 0);
   i30 = i16 + -11 | 0;
   i31 = (i14 & 65535) >>> (16 - i16 | 0);
  } else {
   i30 = i28 + 5 | 0;
   i31 = i21;
  }
  HEAP32[i1 + 5820 >> 2] = i30;
  i21 = i22 + 65533 & 65535;
  i16 = i21 << i30 | i31 & 65535;
  HEAP16[i1 + 5816 >> 1] = i16;
  if ((i30 | 0) > 12) {
   i17 = HEAP32[i1 + 20 >> 2] | 0;
   HEAP32[i1 + 20 >> 2] = i17 + 1;
   HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i17 >> 0] = i16;
   i17 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
   i7 = HEAP32[i1 + 20 >> 2] | 0;
   HEAP32[i1 + 20 >> 2] = i7 + 1;
   HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i7 >> 0] = i17;
   i17 = HEAP32[i1 + 5820 >> 2] | 0;
   HEAP16[i1 + 5816 >> 1] = i21 >>> (16 - i17 | 0);
   i32 = i1 + 8 | 0;
   i33 = i1 + 20 | 0;
   i34 = i21 >>> (16 - i17 | 0);
   i35 = i17 + -12 | 0;
  } else {
   i32 = i1 + 8 | 0;
   i33 = i1 + 20 | 0;
   i34 = i16;
   i35 = i30 + 4 | 0;
  }
  HEAP32[i1 + 5820 >> 2] = i35;
  i16 = i35;
  i17 = i34;
  i21 = 0;
  while (1) {
   i7 = HEAPU16[i1 + 2684 + (HEAPU8[14230 + i21 >> 0] << 2) + 2 >> 1] | 0;
   i20 = i7 << i16 | i17 & 65535;
   HEAP16[i1 + 5816 >> 1] = i20;
   if ((i16 | 0) > 13) {
    i13 = HEAP32[i33 >> 2] | 0;
    HEAP32[i33 >> 2] = i13 + 1;
    HEAP8[(HEAP32[i32 >> 2] | 0) + i13 >> 0] = i20;
    i13 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
    i10 = HEAP32[i33 >> 2] | 0;
    HEAP32[i33 >> 2] = i10 + 1;
    HEAP8[(HEAP32[i32 >> 2] | 0) + i10 >> 0] = i13;
    i13 = HEAP32[i1 + 5820 >> 2] | 0;
    HEAP16[i1 + 5816 >> 1] = i7 >>> (16 - i13 | 0);
    i36 = i7 >>> (16 - i13 | 0);
    i37 = i13 + -13 | 0;
   } else {
    i36 = i20;
    i37 = i16 + 3 | 0;
   }
   HEAP32[i1 + 5820 >> 2] = i37;
   if ((i21 | 0) == (i22 | 0)) break; else {
    i16 = i37;
    i17 = i36;
    i21 = i21 + 1 | 0;
   }
  }
  _send_tree(i1, i1 + 148 | 0, i18);
  _send_tree(i1, i1 + 2440 | 0, i14);
  _compress_block(i1, i1 + 148 | 0, i1 + 2440 | 0);
 } while (0);
 _init_block(i1);
 if (!i4) return;
 i4 = HEAP32[i1 + 5820 >> 2] | 0;
 if ((i4 | 0) <= 8) if ((i4 | 0) > 0) {
  i4 = HEAP16[i1 + 5816 >> 1] & 255;
  i36 = HEAP32[i1 + 20 >> 2] | 0;
  HEAP32[i1 + 20 >> 2] = i36 + 1;
  HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i36 >> 0] = i4;
  i38 = i1 + 5816 | 0;
 } else i38 = i1 + 5816 | 0; else {
  i4 = HEAP16[i1 + 5816 >> 1] & 255;
  i36 = HEAP32[i1 + 20 >> 2] | 0;
  HEAP32[i1 + 20 >> 2] = i36 + 1;
  HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i36 >> 0] = i4;
  i4 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
  i36 = HEAP32[i1 + 20 >> 2] | 0;
  HEAP32[i1 + 20 >> 2] = i36 + 1;
  HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i36 >> 0] = i4;
  i38 = i1 + 5816 | 0;
 }
 HEAP16[i38 >> 1] = 0;
 HEAP32[i1 + 5820 >> 2] = 0;
 return;
}

function _deflate_slow(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0;
 L1 : while (1) {
  i3 = HEAP32[i1 + 116 >> 2] | 0;
  while (1) {
   if (i3 >>> 0 < 262) {
    _fill_window(i1);
    i4 = HEAP32[i1 + 116 >> 2] | 0;
    if ((i2 | 0) == 0 & i4 >>> 0 < 262) {
     i5 = 0;
     i6 = 48;
     break L1;
    }
    if (!i4) {
     i6 = 38;
     break L1;
    }
    if (i4 >>> 0 <= 2) {
     HEAP32[i1 + 120 >> 2] = HEAP32[i1 + 96 >> 2];
     HEAP32[i1 + 100 >> 2] = HEAP32[i1 + 112 >> 2];
     HEAP32[i1 + 96 >> 2] = 2;
     i7 = 2;
     i6 = 16;
    } else i6 = 8;
   } else i6 = 8;
   do if ((i6 | 0) == 8) {
    i6 = 0;
    i4 = HEAP32[i1 + 108 >> 2] | 0;
    i8 = ((HEAPU8[(HEAP32[i1 + 56 >> 2] | 0) + (i4 + 2) >> 0] | 0) ^ HEAP32[i1 + 72 >> 2] << HEAP32[i1 + 88 >> 2]) & HEAP32[i1 + 84 >> 2];
    HEAP32[i1 + 72 >> 2] = i8;
    i9 = (HEAP32[i1 + 68 >> 2] | 0) + (i8 << 1) | 0;
    i8 = HEAP16[i9 >> 1] | 0;
    HEAP16[(HEAP32[i1 + 64 >> 2] | 0) + ((HEAP32[i1 + 52 >> 2] & i4) << 1) >> 1] = i8;
    HEAP16[i9 >> 1] = i4;
    i4 = HEAP32[i1 + 96 >> 2] | 0;
    HEAP32[i1 + 120 >> 2] = i4;
    HEAP32[i1 + 100 >> 2] = HEAP32[i1 + 112 >> 2];
    HEAP32[i1 + 96 >> 2] = 2;
    if (i8 << 16 >> 16) if (i4 >>> 0 < (HEAP32[i1 + 128 >> 2] | 0) >>> 0) if (((HEAP32[i1 + 108 >> 2] | 0) - (i8 & 65535) | 0) >>> 0 <= ((HEAP32[i1 + 44 >> 2] | 0) + -262 | 0) >>> 0) {
     i9 = _longest_match(i1, i8 & 65535) | 0;
     HEAP32[i1 + 96 >> 2] = i9;
     if (i9 >>> 0 < 6) {
      if ((HEAP32[i1 + 136 >> 2] | 0) != 1) {
       if ((i9 | 0) != 3) {
        i7 = i9;
        i6 = 16;
        break;
       }
       if (((HEAP32[i1 + 108 >> 2] | 0) - (HEAP32[i1 + 112 >> 2] | 0) | 0) >>> 0 <= 4096) {
        i7 = 3;
        i6 = 16;
        break;
       }
      }
      HEAP32[i1 + 96 >> 2] = 2;
      i7 = 2;
      i6 = 16;
     } else {
      i7 = i9;
      i6 = 16;
     }
    } else {
     i7 = 2;
     i6 = 16;
    } else {
     i10 = i4;
     i11 = 2;
    } else {
     i7 = 2;
     i6 = 16;
    }
   } while (0);
   if ((i6 | 0) == 16) {
    i6 = 0;
    i10 = HEAP32[i1 + 120 >> 2] | 0;
    i11 = i7;
   }
   if (!(i10 >>> 0 < 3 | i11 >>> 0 > i10 >>> 0)) {
    i12 = i10;
    break;
   }
   if (!(HEAP32[i1 + 104 >> 2] | 0)) {
    HEAP32[i1 + 104 >> 2] = 1;
    HEAP32[i1 + 108 >> 2] = (HEAP32[i1 + 108 >> 2] | 0) + 1;
    i4 = (HEAP32[i1 + 116 >> 2] | 0) + -1 | 0;
    HEAP32[i1 + 116 >> 2] = i4;
    i3 = i4;
    continue;
   }
   i4 = HEAP8[(HEAP32[i1 + 56 >> 2] | 0) + ((HEAP32[i1 + 108 >> 2] | 0) + -1) >> 0] | 0;
   i9 = HEAP32[i1 + 5792 >> 2] | 0;
   HEAP16[(HEAP32[i1 + 5796 >> 2] | 0) + (i9 << 1) >> 1] = 0;
   HEAP32[i1 + 5792 >> 2] = i9 + 1;
   HEAP8[(HEAP32[i1 + 5784 >> 2] | 0) + i9 >> 0] = i4;
   HEAP16[i1 + 148 + ((i4 & 255) << 2) >> 1] = (HEAP16[i1 + 148 + ((i4 & 255) << 2) >> 1] | 0) + 1 << 16 >> 16;
   if ((HEAP32[i1 + 5792 >> 2] | 0) == ((HEAP32[i1 + 5788 >> 2] | 0) + -1 | 0)) {
    i4 = HEAP32[i1 + 92 >> 2] | 0;
    if ((i4 | 0) > -1) i13 = (HEAP32[i1 + 56 >> 2] | 0) + i4 | 0; else i13 = 0;
    __tr_flush_block(i1, i13, (HEAP32[i1 + 108 >> 2] | 0) - i4 | 0, 0);
    HEAP32[i1 + 92 >> 2] = HEAP32[i1 + 108 >> 2];
    i4 = HEAP32[i1 >> 2] | 0;
    i9 = HEAP32[i4 + 28 >> 2] | 0;
    i8 = HEAP32[i9 + 20 >> 2] | 0;
    i14 = HEAP32[i4 + 16 >> 2] | 0;
    i15 = i8 >>> 0 > i14 >>> 0 ? i14 : i8;
    if ((i15 | 0) != 0 ? (_memcpy(HEAP32[i4 + 12 >> 2] | 0, HEAP32[i9 + 16 >> 2] | 0, i15 | 0) | 0, HEAP32[i4 + 12 >> 2] = (HEAP32[i4 + 12 >> 2] | 0) + i15, i9 = HEAP32[i4 + 28 >> 2] | 0, HEAP32[i9 + 16 >> 2] = (HEAP32[i9 + 16 >> 2] | 0) + i15, HEAP32[i4 + 20 >> 2] = (HEAP32[i4 + 20 >> 2] | 0) + i15, HEAP32[i4 + 16 >> 2] = (HEAP32[i4 + 16 >> 2] | 0) - i15, i4 = HEAP32[i9 + 20 >> 2] | 0, HEAP32[i9 + 20 >> 2] = i4 - i15, (i4 | 0) == (i15 | 0)) : 0) HEAP32[i9 + 16 >> 2] = HEAP32[i9 + 8 >> 2];
   }
   HEAP32[i1 + 108 >> 2] = (HEAP32[i1 + 108 >> 2] | 0) + 1;
   i3 = (HEAP32[i1 + 116 >> 2] | 0) + -1 | 0;
   HEAP32[i1 + 116 >> 2] = i3;
   if (!(HEAP32[(HEAP32[i1 >> 2] | 0) + 16 >> 2] | 0)) {
    i5 = 0;
    i6 = 48;
    break L1;
   }
  }
  i3 = HEAP32[i1 + 108 >> 2] | 0;
  i9 = i3 + -3 + (HEAP32[i1 + 116 >> 2] | 0) | 0;
  i15 = i3 + 65535 - (HEAP32[i1 + 100 >> 2] | 0) | 0;
  i3 = HEAP32[i1 + 5792 >> 2] | 0;
  HEAP16[(HEAP32[i1 + 5796 >> 2] | 0) + (i3 << 1) >> 1] = i15;
  HEAP32[i1 + 5792 >> 2] = i3 + 1;
  HEAP8[(HEAP32[i1 + 5784 >> 2] | 0) + i3 >> 0] = i12 + 253;
  i3 = i1 + 148 + ((HEAPU8[13974 + (i12 + 253 & 255) >> 0] | 0 | 256) + 1 << 2) | 0;
  HEAP16[i3 >> 1] = (HEAP16[i3 >> 1] | 0) + 1 << 16 >> 16;
  i3 = i1 + 2440 + ((HEAPU8[13462 + ((i15 + 65535 & 65535) >>> 0 < 256 ? i15 + 65535 & 65535 : ((i15 + 65535 & 65535) >>> 7) + 256 | 0) >> 0] | 0) << 2) | 0;
  HEAP16[i3 >> 1] = (HEAP16[i3 >> 1] | 0) + 1 << 16 >> 16;
  i3 = HEAP32[i1 + 5792 >> 2] | 0;
  i15 = (HEAP32[i1 + 5788 >> 2] | 0) + -1 | 0;
  i4 = HEAP32[i1 + 120 >> 2] | 0;
  HEAP32[i1 + 116 >> 2] = 1 - i4 + (HEAP32[i1 + 116 >> 2] | 0);
  HEAP32[i1 + 120 >> 2] = i4 + -2;
  i8 = HEAP32[i1 + 108 >> 2] | 0;
  i14 = i4 + -2 | 0;
  while (1) {
   i4 = i8 + 1 | 0;
   HEAP32[i1 + 108 >> 2] = i4;
   if (i4 >>> 0 <= i9 >>> 0) {
    i16 = ((HEAPU8[(HEAP32[i1 + 56 >> 2] | 0) + (i8 + 3) >> 0] | 0) ^ HEAP32[i1 + 72 >> 2] << HEAP32[i1 + 88 >> 2]) & HEAP32[i1 + 84 >> 2];
    HEAP32[i1 + 72 >> 2] = i16;
    i17 = (HEAP32[i1 + 68 >> 2] | 0) + (i16 << 1) | 0;
    HEAP16[(HEAP32[i1 + 64 >> 2] | 0) + ((HEAP32[i1 + 52 >> 2] & i4) << 1) >> 1] = HEAP16[i17 >> 1] | 0;
    HEAP16[i17 >> 1] = i4;
   }
   i14 = i14 + -1 | 0;
   HEAP32[i1 + 120 >> 2] = i14;
   if (!i14) {
    i18 = i8;
    break;
   } else i8 = i4;
  }
  HEAP32[i1 + 104 >> 2] = 0;
  HEAP32[i1 + 96 >> 2] = 2;
  HEAP32[i1 + 108 >> 2] = i18 + 2;
  if ((i3 | 0) != (i15 | 0)) continue;
  i8 = HEAP32[i1 + 92 >> 2] | 0;
  if ((i8 | 0) > -1) i19 = (HEAP32[i1 + 56 >> 2] | 0) + i8 | 0; else i19 = 0;
  __tr_flush_block(i1, i19, i18 + 2 - i8 | 0, 0);
  HEAP32[i1 + 92 >> 2] = HEAP32[i1 + 108 >> 2];
  i8 = HEAP32[i1 >> 2] | 0;
  i14 = HEAP32[i8 + 28 >> 2] | 0;
  i9 = HEAP32[i14 + 20 >> 2] | 0;
  i4 = HEAP32[i8 + 16 >> 2] | 0;
  i17 = i9 >>> 0 > i4 >>> 0 ? i4 : i9;
  if ((i17 | 0) != 0 ? (_memcpy(HEAP32[i8 + 12 >> 2] | 0, HEAP32[i14 + 16 >> 2] | 0, i17 | 0) | 0, HEAP32[i8 + 12 >> 2] = (HEAP32[i8 + 12 >> 2] | 0) + i17, i14 = HEAP32[i8 + 28 >> 2] | 0, HEAP32[i14 + 16 >> 2] = (HEAP32[i14 + 16 >> 2] | 0) + i17, HEAP32[i8 + 20 >> 2] = (HEAP32[i8 + 20 >> 2] | 0) + i17, HEAP32[i8 + 16 >> 2] = (HEAP32[i8 + 16 >> 2] | 0) - i17, i8 = HEAP32[i14 + 20 >> 2] | 0, HEAP32[i14 + 20 >> 2] = i8 - i17, (i8 | 0) == (i17 | 0)) : 0) HEAP32[i14 + 16 >> 2] = HEAP32[i14 + 8 >> 2];
  if (!(HEAP32[(HEAP32[i1 >> 2] | 0) + 16 >> 2] | 0)) {
   i5 = 0;
   i6 = 48;
   break;
  }
 }
 if ((i6 | 0) == 38) {
  if (HEAP32[i1 + 104 >> 2] | 0) {
   i18 = HEAP8[(HEAP32[i1 + 56 >> 2] | 0) + ((HEAP32[i1 + 108 >> 2] | 0) + -1) >> 0] | 0;
   i19 = HEAP32[i1 + 5792 >> 2] | 0;
   HEAP16[(HEAP32[i1 + 5796 >> 2] | 0) + (i19 << 1) >> 1] = 0;
   HEAP32[i1 + 5792 >> 2] = i19 + 1;
   HEAP8[(HEAP32[i1 + 5784 >> 2] | 0) + i19 >> 0] = i18;
   HEAP16[i1 + 148 + ((i18 & 255) << 2) >> 1] = (HEAP16[i1 + 148 + ((i18 & 255) << 2) >> 1] | 0) + 1 << 16 >> 16;
   HEAP32[i1 + 104 >> 2] = 0;
  }
  i18 = HEAP32[i1 + 92 >> 2] | 0;
  if ((i18 | 0) > -1) i20 = (HEAP32[i1 + 56 >> 2] | 0) + i18 | 0; else i20 = 0;
  __tr_flush_block(i1, i20, (HEAP32[i1 + 108 >> 2] | 0) - i18 | 0, (i2 | 0) == 4 & 1);
  HEAP32[i1 + 92 >> 2] = HEAP32[i1 + 108 >> 2];
  i18 = HEAP32[i1 >> 2] | 0;
  i20 = HEAP32[i18 + 28 >> 2] | 0;
  i19 = HEAP32[i20 + 20 >> 2] | 0;
  i12 = HEAP32[i18 + 16 >> 2] | 0;
  i13 = i19 >>> 0 > i12 >>> 0 ? i12 : i19;
  if ((i13 | 0) != 0 ? (_memcpy(HEAP32[i18 + 12 >> 2] | 0, HEAP32[i20 + 16 >> 2] | 0, i13 | 0) | 0, HEAP32[i18 + 12 >> 2] = (HEAP32[i18 + 12 >> 2] | 0) + i13, i20 = HEAP32[i18 + 28 >> 2] | 0, HEAP32[i20 + 16 >> 2] = (HEAP32[i20 + 16 >> 2] | 0) + i13, HEAP32[i18 + 20 >> 2] = (HEAP32[i18 + 20 >> 2] | 0) + i13, HEAP32[i18 + 16 >> 2] = (HEAP32[i18 + 16 >> 2] | 0) - i13, i18 = HEAP32[i20 + 20 >> 2] | 0, HEAP32[i20 + 20 >> 2] = i18 - i13, (i18 | 0) == (i13 | 0)) : 0) HEAP32[i20 + 16 >> 2] = HEAP32[i20 + 8 >> 2];
  if (!(HEAP32[(HEAP32[i1 >> 2] | 0) + 16 >> 2] | 0)) {
   i5 = (i2 | 0) == 4 ? 2 : 0;
   return i5 | 0;
  } else {
   i5 = (i2 | 0) == 4 ? 3 : 1;
   return i5 | 0;
  }
 } else if ((i6 | 0) == 48) return i5 | 0;
 return 0;
}

function _send_tree(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 var i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0, i24 = 0, i25 = 0, i26 = 0, i27 = 0, i28 = 0, i29 = 0, i30 = 0, i31 = 0, i32 = 0, i33 = 0, i34 = 0, i35 = 0, i36 = 0;
 i4 = HEAP16[i2 + 2 >> 1] | 0;
 i5 = i4 << 16 >> 16 == 0 ? 138 : 7;
 i6 = i4 << 16 >> 16 == 0 ? 3 : 4;
 i7 = 0;
 i8 = i4 & 65535;
 i4 = -1;
 L1 : while (1) {
  i9 = 0;
  i10 = i7;
  while (1) {
   if ((i10 | 0) > (i3 | 0)) break L1;
   i11 = i10 + 1 | 0;
   i12 = HEAP16[i2 + (i11 << 2) + 2 >> 1] | 0;
   i13 = i9 + 1 | 0;
   i14 = (i8 | 0) == (i12 & 65535 | 0);
   if ((i13 | 0) < (i5 | 0) & i14) {
    i9 = i13;
    i10 = i11;
   } else {
    i15 = i9;
    break;
   }
  }
  do if ((i13 | 0) >= (i6 | 0)) {
   if (i8) {
    if ((i8 | 0) == (i4 | 0)) {
     i16 = HEAP16[i1 + 5816 >> 1] | 0;
     i17 = HEAP32[i1 + 5820 >> 2] | 0;
     i18 = i13;
    } else {
     i9 = HEAPU16[i1 + 2684 + (i8 << 2) + 2 >> 1] | 0;
     i10 = HEAP32[i1 + 5820 >> 2] | 0;
     i19 = HEAPU16[i1 + 2684 + (i8 << 2) >> 1] | 0;
     i20 = HEAPU16[i1 + 5816 >> 1] | 0 | i19 << i10;
     HEAP16[i1 + 5816 >> 1] = i20;
     if ((i10 | 0) > (16 - i9 | 0)) {
      i21 = HEAP32[i1 + 20 >> 2] | 0;
      HEAP32[i1 + 20 >> 2] = i21 + 1;
      HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i21 >> 0] = i20;
      i21 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
      i22 = HEAP32[i1 + 20 >> 2] | 0;
      HEAP32[i1 + 20 >> 2] = i22 + 1;
      HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i22 >> 0] = i21;
      i21 = HEAP32[i1 + 5820 >> 2] | 0;
      HEAP16[i1 + 5816 >> 1] = i19 >>> (16 - i21 | 0);
      i23 = i19 >>> (16 - i21 | 0) & 65535;
      i24 = i9 + -16 + i21 | 0;
     } else {
      i23 = i20 & 65535;
      i24 = i10 + i9 | 0;
     }
     HEAP32[i1 + 5820 >> 2] = i24;
     i16 = i23;
     i17 = i24;
     i18 = i15;
    }
    i9 = HEAPU16[i1 + 2750 >> 1] | 0;
    i10 = HEAPU16[i1 + 2748 >> 1] | 0;
    i20 = i16 & 65535 | i10 << i17;
    HEAP16[i1 + 5816 >> 1] = i20;
    if ((i17 | 0) > (16 - i9 | 0)) {
     i21 = HEAP32[i1 + 20 >> 2] | 0;
     HEAP32[i1 + 20 >> 2] = i21 + 1;
     HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i21 >> 0] = i20;
     i21 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
     i19 = HEAP32[i1 + 20 >> 2] | 0;
     HEAP32[i1 + 20 >> 2] = i19 + 1;
     HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i19 >> 0] = i21;
     i21 = HEAP32[i1 + 5820 >> 2] | 0;
     HEAP16[i1 + 5816 >> 1] = i10 >>> (16 - i21 | 0);
     i25 = i9 + -16 + i21 | 0;
     i26 = i10 >>> (16 - i21 | 0);
    } else {
     i25 = i17 + i9 | 0;
     i26 = i20;
    }
    HEAP32[i1 + 5820 >> 2] = i25;
    i20 = i18 + 65533 & 65535;
    i9 = i26 & 65535 | i20 << i25;
    HEAP16[i1 + 5816 >> 1] = i9;
    if ((i25 | 0) > 14) {
     i21 = HEAP32[i1 + 20 >> 2] | 0;
     HEAP32[i1 + 20 >> 2] = i21 + 1;
     HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i21 >> 0] = i9;
     i9 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
     i21 = HEAP32[i1 + 20 >> 2] | 0;
     HEAP32[i1 + 20 >> 2] = i21 + 1;
     HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i21 >> 0] = i9;
     i9 = HEAP32[i1 + 5820 >> 2] | 0;
     HEAP16[i1 + 5816 >> 1] = i20 >>> (16 - i9 | 0);
     HEAP32[i1 + 5820 >> 2] = i9 + -14;
     break;
    } else {
     HEAP32[i1 + 5820 >> 2] = i25 + 2;
     break;
    }
   }
   if ((i13 | 0) < 11) {
    i9 = HEAPU16[i1 + 2754 >> 1] | 0;
    i20 = HEAP32[i1 + 5820 >> 2] | 0;
    i21 = HEAPU16[i1 + 2752 >> 1] | 0;
    i10 = HEAPU16[i1 + 5816 >> 1] | 0 | i21 << i20;
    HEAP16[i1 + 5816 >> 1] = i10;
    if ((i20 | 0) > (16 - i9 | 0)) {
     i19 = HEAP32[i1 + 20 >> 2] | 0;
     HEAP32[i1 + 20 >> 2] = i19 + 1;
     HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i19 >> 0] = i10;
     i19 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
     i22 = HEAP32[i1 + 20 >> 2] | 0;
     HEAP32[i1 + 20 >> 2] = i22 + 1;
     HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i22 >> 0] = i19;
     i19 = HEAP32[i1 + 5820 >> 2] | 0;
     HEAP16[i1 + 5816 >> 1] = i21 >>> (16 - i19 | 0);
     i27 = i9 + -16 + i19 | 0;
     i28 = i21 >>> (16 - i19 | 0);
    } else {
     i27 = i20 + i9 | 0;
     i28 = i10;
    }
    HEAP32[i1 + 5820 >> 2] = i27;
    i10 = i28 & 65535 | (i15 + 65534 & 65535) << i27;
    HEAP16[i1 + 5816 >> 1] = i10;
    if ((i27 | 0) > 13) {
     i9 = HEAP32[i1 + 20 >> 2] | 0;
     HEAP32[i1 + 20 >> 2] = i9 + 1;
     HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i9 >> 0] = i10;
     i10 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
     i9 = HEAP32[i1 + 20 >> 2] | 0;
     HEAP32[i1 + 20 >> 2] = i9 + 1;
     HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i9 >> 0] = i10;
     i10 = HEAP32[i1 + 5820 >> 2] | 0;
     HEAP16[i1 + 5816 >> 1] = (i15 + 65534 & 65535) >>> (16 - i10 | 0);
     HEAP32[i1 + 5820 >> 2] = i10 + -13;
     break;
    } else {
     HEAP32[i1 + 5820 >> 2] = i27 + 3;
     break;
    }
   } else {
    i10 = HEAPU16[i1 + 2758 >> 1] | 0;
    i9 = HEAP32[i1 + 5820 >> 2] | 0;
    i20 = HEAPU16[i1 + 2756 >> 1] | 0;
    i19 = HEAPU16[i1 + 5816 >> 1] | 0 | i20 << i9;
    HEAP16[i1 + 5816 >> 1] = i19;
    if ((i9 | 0) > (16 - i10 | 0)) {
     i21 = HEAP32[i1 + 20 >> 2] | 0;
     HEAP32[i1 + 20 >> 2] = i21 + 1;
     HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i21 >> 0] = i19;
     i21 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
     i22 = HEAP32[i1 + 20 >> 2] | 0;
     HEAP32[i1 + 20 >> 2] = i22 + 1;
     HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i22 >> 0] = i21;
     i21 = HEAP32[i1 + 5820 >> 2] | 0;
     HEAP16[i1 + 5816 >> 1] = i20 >>> (16 - i21 | 0);
     i29 = i10 + -16 + i21 | 0;
     i30 = i20 >>> (16 - i21 | 0);
    } else {
     i29 = i9 + i10 | 0;
     i30 = i19;
    }
    HEAP32[i1 + 5820 >> 2] = i29;
    i19 = i30 & 65535 | (i15 + 65526 & 65535) << i29;
    HEAP16[i1 + 5816 >> 1] = i19;
    if ((i29 | 0) > 9) {
     i10 = HEAP32[i1 + 20 >> 2] | 0;
     HEAP32[i1 + 20 >> 2] = i10 + 1;
     HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i10 >> 0] = i19;
     i19 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
     i10 = HEAP32[i1 + 20 >> 2] | 0;
     HEAP32[i1 + 20 >> 2] = i10 + 1;
     HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i10 >> 0] = i19;
     i19 = HEAP32[i1 + 5820 >> 2] | 0;
     HEAP16[i1 + 5816 >> 1] = (i15 + 65526 & 65535) >>> (16 - i19 | 0);
     HEAP32[i1 + 5820 >> 2] = i19 + -9;
     break;
    } else {
     HEAP32[i1 + 5820 >> 2] = i29 + 7;
     break;
    }
   }
  } else {
   i19 = i1 + 2684 + (i8 << 2) + 2 | 0;
   i10 = i1 + 2684 + (i8 << 2) | 0;
   i9 = HEAP32[i1 + 5820 >> 2] | 0;
   i21 = HEAP16[i1 + 5816 >> 1] | 0;
   i20 = i13;
   while (1) {
    i22 = HEAPU16[i19 >> 1] | 0;
    i31 = HEAPU16[i10 >> 1] | 0;
    i32 = i21 & 65535 | i31 << i9;
    HEAP16[i1 + 5816 >> 1] = i32;
    if ((i9 | 0) > (16 - i22 | 0)) {
     i33 = HEAP32[i1 + 20 >> 2] | 0;
     HEAP32[i1 + 20 >> 2] = i33 + 1;
     HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i33 >> 0] = i32;
     i33 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
     i34 = HEAP32[i1 + 20 >> 2] | 0;
     HEAP32[i1 + 20 >> 2] = i34 + 1;
     HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i34 >> 0] = i33;
     i33 = HEAP32[i1 + 5820 >> 2] | 0;
     HEAP16[i1 + 5816 >> 1] = i31 >>> (16 - i33 | 0);
     i35 = i31 >>> (16 - i33 | 0) & 65535;
     i36 = i22 + -16 + i33 | 0;
    } else {
     i35 = i32 & 65535;
     i36 = i9 + i22 | 0;
    }
    HEAP32[i1 + 5820 >> 2] = i36;
    i20 = i20 + -1 | 0;
    if (!i20) break; else {
     i9 = i36;
     i21 = i35;
    }
   }
  } while (0);
  i21 = i8;
  i5 = i12 << 16 >> 16 == 0 ? 138 : i14 ? 6 : 7;
  i6 = i12 << 16 >> 16 == 0 | i14 ? 3 : 4;
  i7 = i11;
  i8 = i12 & 65535;
  i4 = i21;
 }
 return;
}

function _compress_block(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 var i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0, i24 = 0, i25 = 0, i26 = 0, i27 = 0, i28 = 0, i29 = 0;
 if (!(HEAP32[i1 + 5792 >> 2] | 0)) {
  i4 = i1 + 5816 | 0;
  i5 = i1 + 5820 | 0;
  i6 = HEAP32[i1 + 5820 >> 2] | 0;
  i7 = HEAP16[i1 + 5816 >> 1] | 0;
 } else {
  i8 = 0;
  do {
   i9 = HEAP16[(HEAP32[i1 + 5796 >> 2] | 0) + (i8 << 1) >> 1] | 0;
   i10 = HEAPU8[(HEAP32[i1 + 5784 >> 2] | 0) + i8 >> 0] | 0;
   i8 = i8 + 1 | 0;
   do if (!(i9 << 16 >> 16)) {
    i11 = HEAPU16[i2 + (i10 << 2) + 2 >> 1] | 0;
    i12 = HEAP32[i1 + 5820 >> 2] | 0;
    i13 = HEAPU16[i2 + (i10 << 2) >> 1] | 0;
    i14 = HEAPU16[i1 + 5816 >> 1] | 0 | i13 << i12;
    HEAP16[i1 + 5816 >> 1] = i14;
    if ((i12 | 0) > (16 - i11 | 0)) {
     i15 = HEAP32[i1 + 20 >> 2] | 0;
     HEAP32[i1 + 20 >> 2] = i15 + 1;
     HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i15 >> 0] = i14;
     i15 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
     i16 = HEAP32[i1 + 20 >> 2] | 0;
     HEAP32[i1 + 20 >> 2] = i16 + 1;
     HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i16 >> 0] = i15;
     i15 = HEAP32[i1 + 5820 >> 2] | 0;
     HEAP16[i1 + 5816 >> 1] = i13 >>> (16 - i15 | 0);
     HEAP32[i1 + 5820 >> 2] = i11 + -16 + i15;
     i17 = i13 >>> (16 - i15 | 0) & 65535;
     i18 = i11 + -16 + i15 | 0;
     break;
    } else {
     HEAP32[i1 + 5820 >> 2] = i12 + i11;
     i17 = i14 & 65535;
     i18 = i12 + i11 | 0;
     break;
    }
   } else {
    i11 = HEAPU8[13974 + i10 >> 0] | 0;
    i12 = HEAPU16[i2 + ((i11 | 256) + 1 << 2) + 2 >> 1] | 0;
    i14 = HEAP32[i1 + 5820 >> 2] | 0;
    i15 = HEAPU16[i2 + ((i11 | 256) + 1 << 2) >> 1] | 0;
    i13 = HEAPU16[i1 + 5816 >> 1] | 0 | i15 << i14;
    HEAP16[i1 + 5816 >> 1] = i13;
    if ((i14 | 0) > (16 - i12 | 0)) {
     i16 = HEAP32[i1 + 20 >> 2] | 0;
     HEAP32[i1 + 20 >> 2] = i16 + 1;
     HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i16 >> 0] = i13;
     i16 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
     i19 = HEAP32[i1 + 20 >> 2] | 0;
     HEAP32[i1 + 20 >> 2] = i19 + 1;
     HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i19 >> 0] = i16;
     i16 = HEAP32[i1 + 5820 >> 2] | 0;
     HEAP16[i1 + 5816 >> 1] = i15 >>> (16 - i16 | 0);
     i20 = i15 >>> (16 - i16 | 0) & 65535;
     i21 = i12 + -16 + i16 | 0;
    } else {
     i20 = i13 & 65535;
     i21 = i14 + i12 | 0;
    }
    HEAP32[i1 + 5820 >> 2] = i21;
    i12 = HEAP32[196 + (i11 << 2) >> 2] | 0;
    do if ((i11 + -8 | 0) >>> 0 < 20) {
     i14 = i10 - (HEAP32[312 + (i11 << 2) >> 2] | 0) & 65535;
     i13 = i14 << i21 | i20 & 65535;
     HEAP16[i1 + 5816 >> 1] = i13;
     if ((i21 | 0) > (16 - i12 | 0)) {
      i16 = HEAP32[i1 + 20 >> 2] | 0;
      HEAP32[i1 + 20 >> 2] = i16 + 1;
      HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i16 >> 0] = i13;
      i16 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
      i15 = HEAP32[i1 + 20 >> 2] | 0;
      HEAP32[i1 + 20 >> 2] = i15 + 1;
      HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i15 >> 0] = i16;
      i16 = HEAP32[i1 + 5820 >> 2] | 0;
      HEAP16[i1 + 5816 >> 1] = i14 >>> (16 - i16 | 0);
      HEAP32[i1 + 5820 >> 2] = i12 + -16 + i16;
      i22 = i12 + -16 + i16 | 0;
      i23 = i14 >>> (16 - i16 | 0) & 65535;
      break;
     } else {
      i16 = i21 + i12 | 0;
      HEAP32[i1 + 5820 >> 2] = i16;
      i22 = i16;
      i23 = i13 & 65535;
      break;
     }
    } else {
     i22 = i21;
     i23 = i20;
    } while (0);
    i12 = HEAPU8[13462 + (((i9 & 65535) + -1 | 0) >>> 0 < 256 ? (i9 & 65535) + -1 | 0 : (((i9 & 65535) + -1 | 0) >>> 7) + 256 | 0) >> 0] | 0;
    i11 = HEAPU16[i3 + (i12 << 2) + 2 >> 1] | 0;
    i13 = HEAPU16[i3 + (i12 << 2) >> 1] | 0;
    i16 = i23 & 65535 | i13 << i22;
    HEAP16[i1 + 5816 >> 1] = i16;
    if ((i22 | 0) > (16 - i11 | 0)) {
     i14 = HEAP32[i1 + 20 >> 2] | 0;
     HEAP32[i1 + 20 >> 2] = i14 + 1;
     HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i14 >> 0] = i16;
     i14 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
     i15 = HEAP32[i1 + 20 >> 2] | 0;
     HEAP32[i1 + 20 >> 2] = i15 + 1;
     HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i15 >> 0] = i14;
     i14 = HEAP32[i1 + 5820 >> 2] | 0;
     HEAP16[i1 + 5816 >> 1] = i13 >>> (16 - i14 | 0);
     i24 = i11 + -16 + i14 | 0;
     i25 = i13 >>> (16 - i14 | 0) & 65535;
    } else {
     i24 = i22 + i11 | 0;
     i25 = i16 & 65535;
    }
    HEAP32[i1 + 5820 >> 2] = i24;
    i16 = HEAP32[428 + (i12 << 2) >> 2] | 0;
    if ((i12 + -4 | 0) >>> 0 < 26) {
     i11 = (i9 & 65535) + -1 - (HEAP32[548 + (i12 << 2) >> 2] | 0) & 65535;
     i12 = i11 << i24 | i25 & 65535;
     HEAP16[i1 + 5816 >> 1] = i12;
     if ((i24 | 0) > (16 - i16 | 0)) {
      i14 = HEAP32[i1 + 20 >> 2] | 0;
      HEAP32[i1 + 20 >> 2] = i14 + 1;
      HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i14 >> 0] = i12;
      i14 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
      i13 = HEAP32[i1 + 20 >> 2] | 0;
      HEAP32[i1 + 20 >> 2] = i13 + 1;
      HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i13 >> 0] = i14;
      i14 = HEAP32[i1 + 5820 >> 2] | 0;
      HEAP16[i1 + 5816 >> 1] = i11 >>> (16 - i14 | 0);
      HEAP32[i1 + 5820 >> 2] = i16 + -16 + i14;
      i17 = i11 >>> (16 - i14 | 0) & 65535;
      i18 = i16 + -16 + i14 | 0;
      break;
     } else {
      i14 = i24 + i16 | 0;
      HEAP32[i1 + 5820 >> 2] = i14;
      i17 = i12 & 65535;
      i18 = i14;
      break;
     }
    } else {
     i17 = i25;
     i18 = i24;
    }
   } while (0);
  } while (i8 >>> 0 < (HEAP32[i1 + 5792 >> 2] | 0) >>> 0);
  i4 = i1 + 5816 | 0;
  i5 = i1 + 5820 | 0;
  i6 = i18;
  i7 = i17;
 }
 i17 = HEAPU16[i2 + 1026 >> 1] | 0;
 i18 = HEAPU16[i2 + 1024 >> 1] | 0;
 i8 = i7 & 65535 | i18 << i6;
 HEAP16[i4 >> 1] = i8;
 if ((i6 | 0) > (16 - i17 | 0)) {
  i7 = HEAP32[i1 + 20 >> 2] | 0;
  HEAP32[i1 + 20 >> 2] = i7 + 1;
  HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i7 >> 0] = i8;
  i8 = (HEAPU16[i4 >> 1] | 0) >>> 8 & 255;
  i7 = HEAP32[i1 + 20 >> 2] | 0;
  HEAP32[i1 + 20 >> 2] = i7 + 1;
  HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i7 >> 0] = i8;
  i8 = HEAP32[i5 >> 2] | 0;
  HEAP16[i4 >> 1] = i18 >>> (16 - i8 | 0);
  i26 = i17 + -16 + i8 | 0;
  HEAP32[i5 >> 2] = i26;
  i27 = HEAP16[i2 + 1026 >> 1] | 0;
  i28 = i27 & 65535;
  i29 = i1 + 5812 | 0;
  HEAP32[i29 >> 2] = i28;
  return;
 } else {
  i26 = i6 + i17 | 0;
  HEAP32[i5 >> 2] = i26;
  i27 = HEAP16[i2 + 1026 >> 1] | 0;
  i28 = i27 & 65535;
  i29 = i1 + 5812 | 0;
  HEAP32[i29 >> 2] = i28;
  return;
 }
}

function _deflate_fast(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0;
 while (1) {
  if ((HEAP32[i1 + 116 >> 2] | 0) >>> 0 < 262) {
   _fill_window(i1);
   i3 = HEAP32[i1 + 116 >> 2] | 0;
   if ((i2 | 0) == 0 & i3 >>> 0 < 262) {
    i4 = 0;
    i5 = 32;
    break;
   }
   if (!i3) {
    i5 = 24;
    break;
   }
   if (i3 >>> 0 <= 2) i5 = 9; else i5 = 6;
  } else i5 = 6;
  if ((i5 | 0) == 6) {
   i5 = 0;
   i3 = HEAP32[i1 + 108 >> 2] | 0;
   i6 = ((HEAPU8[(HEAP32[i1 + 56 >> 2] | 0) + (i3 + 2) >> 0] | 0) ^ HEAP32[i1 + 72 >> 2] << HEAP32[i1 + 88 >> 2]) & HEAP32[i1 + 84 >> 2];
   HEAP32[i1 + 72 >> 2] = i6;
   i7 = (HEAP32[i1 + 68 >> 2] | 0) + (i6 << 1) | 0;
   i6 = HEAP16[i7 >> 1] | 0;
   HEAP16[(HEAP32[i1 + 64 >> 2] | 0) + ((HEAP32[i1 + 52 >> 2] & i3) << 1) >> 1] = i6;
   HEAP16[i7 >> 1] = i3;
   if (i6 << 16 >> 16 != 0 ? (i3 - (i6 & 65535) | 0) >>> 0 <= ((HEAP32[i1 + 44 >> 2] | 0) + -262 | 0) >>> 0 : 0) {
    i3 = _longest_match(i1, i6 & 65535) | 0;
    HEAP32[i1 + 96 >> 2] = i3;
    i8 = i3;
   } else i5 = 9;
  }
  if ((i5 | 0) == 9) {
   i5 = 0;
   i8 = HEAP32[i1 + 96 >> 2] | 0;
  }
  do if (i8 >>> 0 > 2) {
   i3 = i8 + 253 | 0;
   i6 = (HEAP32[i1 + 108 >> 2] | 0) - (HEAP32[i1 + 112 >> 2] | 0) | 0;
   i7 = HEAP32[i1 + 5792 >> 2] | 0;
   HEAP16[(HEAP32[i1 + 5796 >> 2] | 0) + (i7 << 1) >> 1] = i6;
   HEAP32[i1 + 5792 >> 2] = i7 + 1;
   HEAP8[(HEAP32[i1 + 5784 >> 2] | 0) + i7 >> 0] = i3;
   i7 = i1 + 148 + ((HEAPU8[13974 + (i3 & 255) >> 0] | 0 | 256) + 1 << 2) | 0;
   HEAP16[i7 >> 1] = (HEAP16[i7 >> 1] | 0) + 1 << 16 >> 16;
   i7 = i1 + 2440 + ((HEAPU8[13462 + ((i6 + 65535 & 65535) >>> 0 < 256 ? i6 + 65535 & 65535 : ((i6 + 65535 & 65535) >>> 7) + 256 | 0) >> 0] | 0) << 2) | 0;
   HEAP16[i7 >> 1] = (HEAP16[i7 >> 1] | 0) + 1 << 16 >> 16;
   i7 = (HEAP32[i1 + 5792 >> 2] | 0) == ((HEAP32[i1 + 5788 >> 2] | 0) + -1 | 0) & 1;
   i6 = HEAP32[i1 + 96 >> 2] | 0;
   i3 = (HEAP32[i1 + 116 >> 2] | 0) - i6 | 0;
   HEAP32[i1 + 116 >> 2] = i3;
   if (!(i3 >>> 0 > 2 ? i6 >>> 0 <= (HEAP32[i1 + 128 >> 2] | 0) >>> 0 : 0)) {
    i3 = (HEAP32[i1 + 108 >> 2] | 0) + i6 | 0;
    HEAP32[i1 + 108 >> 2] = i3;
    HEAP32[i1 + 96 >> 2] = 0;
    i9 = HEAP32[i1 + 56 >> 2] | 0;
    i10 = HEAPU8[i9 + i3 >> 0] | 0;
    HEAP32[i1 + 72 >> 2] = i10;
    HEAP32[i1 + 72 >> 2] = ((HEAPU8[i9 + (i3 + 1) >> 0] | 0) ^ i10 << HEAP32[i1 + 88 >> 2]) & HEAP32[i1 + 84 >> 2];
    i11 = i3;
    i12 = i7;
    break;
   }
   HEAP32[i1 + 96 >> 2] = i6 + -1;
   i3 = HEAP32[i1 + 88 >> 2] | 0;
   i10 = HEAP32[i1 + 56 >> 2] | 0;
   i9 = HEAP32[i1 + 84 >> 2] | 0;
   i13 = HEAP32[i1 + 68 >> 2] | 0;
   i14 = HEAP32[i1 + 52 >> 2] | 0;
   i15 = HEAP32[i1 + 64 >> 2] | 0;
   i16 = i6 + -1 | 0;
   i6 = HEAP32[i1 + 108 >> 2] | 0;
   i17 = HEAP32[i1 + 72 >> 2] | 0;
   while (1) {
    i18 = i6 + 1 | 0;
    HEAP32[i1 + 108 >> 2] = i18;
    i17 = ((HEAPU8[i10 + (i6 + 3) >> 0] | 0) ^ i17 << i3) & i9;
    HEAP32[i1 + 72 >> 2] = i17;
    i19 = i13 + (i17 << 1) | 0;
    HEAP16[i15 + ((i14 & i18) << 1) >> 1] = HEAP16[i19 >> 1] | 0;
    HEAP16[i19 >> 1] = i18;
    i16 = i16 + -1 | 0;
    HEAP32[i1 + 96 >> 2] = i16;
    if (!i16) {
     i20 = i6;
     break;
    } else i6 = i18;
   }
   HEAP32[i1 + 108 >> 2] = i20 + 2;
   i11 = i20 + 2 | 0;
   i12 = i7;
  } else {
   i6 = HEAP8[(HEAP32[i1 + 56 >> 2] | 0) + (HEAP32[i1 + 108 >> 2] | 0) >> 0] | 0;
   i16 = HEAP32[i1 + 5792 >> 2] | 0;
   HEAP16[(HEAP32[i1 + 5796 >> 2] | 0) + (i16 << 1) >> 1] = 0;
   HEAP32[i1 + 5792 >> 2] = i16 + 1;
   HEAP8[(HEAP32[i1 + 5784 >> 2] | 0) + i16 >> 0] = i6;
   HEAP16[i1 + 148 + ((i6 & 255) << 2) >> 1] = (HEAP16[i1 + 148 + ((i6 & 255) << 2) >> 1] | 0) + 1 << 16 >> 16;
   i6 = (HEAP32[i1 + 5792 >> 2] | 0) == ((HEAP32[i1 + 5788 >> 2] | 0) + -1 | 0) & 1;
   HEAP32[i1 + 116 >> 2] = (HEAP32[i1 + 116 >> 2] | 0) + -1;
   i16 = (HEAP32[i1 + 108 >> 2] | 0) + 1 | 0;
   HEAP32[i1 + 108 >> 2] = i16;
   i11 = i16;
   i12 = i6;
  } while (0);
  if (!i12) continue;
  i6 = HEAP32[i1 + 92 >> 2] | 0;
  if ((i6 | 0) > -1) i21 = (HEAP32[i1 + 56 >> 2] | 0) + i6 | 0; else i21 = 0;
  __tr_flush_block(i1, i21, i11 - i6 | 0, 0);
  HEAP32[i1 + 92 >> 2] = HEAP32[i1 + 108 >> 2];
  i6 = HEAP32[i1 >> 2] | 0;
  i16 = HEAP32[i6 + 28 >> 2] | 0;
  i14 = HEAP32[i16 + 20 >> 2] | 0;
  i15 = HEAP32[i6 + 16 >> 2] | 0;
  i17 = i14 >>> 0 > i15 >>> 0 ? i15 : i14;
  if ((i17 | 0) != 0 ? (_memcpy(HEAP32[i6 + 12 >> 2] | 0, HEAP32[i16 + 16 >> 2] | 0, i17 | 0) | 0, HEAP32[i6 + 12 >> 2] = (HEAP32[i6 + 12 >> 2] | 0) + i17, i16 = HEAP32[i6 + 28 >> 2] | 0, HEAP32[i16 + 16 >> 2] = (HEAP32[i16 + 16 >> 2] | 0) + i17, HEAP32[i6 + 20 >> 2] = (HEAP32[i6 + 20 >> 2] | 0) + i17, HEAP32[i6 + 16 >> 2] = (HEAP32[i6 + 16 >> 2] | 0) - i17, i6 = HEAP32[i16 + 20 >> 2] | 0, HEAP32[i16 + 20 >> 2] = i6 - i17, (i6 | 0) == (i17 | 0)) : 0) HEAP32[i16 + 16 >> 2] = HEAP32[i16 + 8 >> 2];
  if (!(HEAP32[(HEAP32[i1 >> 2] | 0) + 16 >> 2] | 0)) {
   i4 = 0;
   i5 = 32;
   break;
  }
 }
 if ((i5 | 0) == 24) {
  i11 = HEAP32[i1 + 92 >> 2] | 0;
  if ((i11 | 0) > -1) i22 = (HEAP32[i1 + 56 >> 2] | 0) + i11 | 0; else i22 = 0;
  __tr_flush_block(i1, i22, (HEAP32[i1 + 108 >> 2] | 0) - i11 | 0, (i2 | 0) == 4 & 1);
  HEAP32[i1 + 92 >> 2] = HEAP32[i1 + 108 >> 2];
  i11 = HEAP32[i1 >> 2] | 0;
  i22 = HEAP32[i11 + 28 >> 2] | 0;
  i21 = HEAP32[i22 + 20 >> 2] | 0;
  i12 = HEAP32[i11 + 16 >> 2] | 0;
  i20 = i21 >>> 0 > i12 >>> 0 ? i12 : i21;
  if ((i20 | 0) != 0 ? (_memcpy(HEAP32[i11 + 12 >> 2] | 0, HEAP32[i22 + 16 >> 2] | 0, i20 | 0) | 0, HEAP32[i11 + 12 >> 2] = (HEAP32[i11 + 12 >> 2] | 0) + i20, i22 = HEAP32[i11 + 28 >> 2] | 0, HEAP32[i22 + 16 >> 2] = (HEAP32[i22 + 16 >> 2] | 0) + i20, HEAP32[i11 + 20 >> 2] = (HEAP32[i11 + 20 >> 2] | 0) + i20, HEAP32[i11 + 16 >> 2] = (HEAP32[i11 + 16 >> 2] | 0) - i20, i11 = HEAP32[i22 + 20 >> 2] | 0, HEAP32[i22 + 20 >> 2] = i11 - i20, (i11 | 0) == (i20 | 0)) : 0) HEAP32[i22 + 16 >> 2] = HEAP32[i22 + 8 >> 2];
  if (!(HEAP32[(HEAP32[i1 >> 2] | 0) + 16 >> 2] | 0)) {
   i4 = (i2 | 0) == 4 ? 2 : 0;
   return i4 | 0;
  } else {
   i4 = (i2 | 0) == 4 ? 3 : 1;
   return i4 | 0;
  }
 } else if ((i5 | 0) == 32) return i4 | 0;
 return 0;
}

function _adler32(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 var i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0, i24 = 0, i25 = 0, i26 = 0, i27 = 0, i28 = 0, i29 = 0, i30 = 0, i31 = 0, i32 = 0, i33 = 0, i34 = 0, i35 = 0, i36 = 0, i37 = 0, i38 = 0, i39 = 0, i40 = 0, i41 = 0, i42 = 0, i43 = 0, i44 = 0, i45 = 0, i46 = 0, i47 = 0;
 if ((i3 | 0) == 1) {
  i4 = (HEAPU8[i2 >> 0] | 0) + (i1 & 65535) | 0;
  i5 = i4 >>> 0 > 65520 ? i4 + -65521 | 0 : i4;
  i6 = ((i5 + (i1 >>> 16) | 0) >>> 0 > 65520 ? i5 + (i1 >>> 16) + 15 | 0 : i5 + (i1 >>> 16) | 0) << 16 | i5;
  return i6 | 0;
 }
 if (!i2) {
  i6 = 1;
  return i6 | 0;
 }
 if (i3 >>> 0 < 16) {
  if (!i3) {
   i7 = i1 & 65535;
   i8 = i1 >>> 16;
  } else {
   i5 = i2;
   i4 = i3;
   i9 = i1 & 65535;
   i10 = i1 >>> 16;
   while (1) {
    i4 = i4 + -1 | 0;
    i11 = (HEAPU8[i5 >> 0] | 0) + i9 | 0;
    i12 = i11 + i10 | 0;
    if (!i4) {
     i7 = i11;
     i8 = i12;
     break;
    } else {
     i5 = i5 + 1 | 0;
     i9 = i11;
     i10 = i12;
    }
   }
  }
  i6 = ((i8 >>> 0) % 65521 | 0) << 16 | (i7 >>> 0 > 65520 ? i7 + -65521 | 0 : i7);
  return i6 | 0;
 }
 if (i3 >>> 0 > 5551) {
  i7 = ((i3 + -5552 | 0) >>> 0) % 5552 | 0;
  i8 = i2;
  i10 = i3;
  i9 = i1 & 65535;
  i5 = i1 >>> 16;
  while (1) {
   i10 = i10 + -5552 | 0;
   i4 = i8;
   i12 = i9;
   i11 = 347;
   i13 = i5;
   while (1) {
    i14 = (HEAPU8[i4 >> 0] | 0) + i12 | 0;
    i15 = i14 + (HEAPU8[i4 + 1 >> 0] | 0) | 0;
    i16 = i15 + (HEAPU8[i4 + 2 >> 0] | 0) | 0;
    i17 = i16 + (HEAPU8[i4 + 3 >> 0] | 0) | 0;
    i18 = i17 + (HEAPU8[i4 + 4 >> 0] | 0) | 0;
    i19 = i18 + (HEAPU8[i4 + 5 >> 0] | 0) | 0;
    i20 = i19 + (HEAPU8[i4 + 6 >> 0] | 0) | 0;
    i21 = i20 + (HEAPU8[i4 + 7 >> 0] | 0) | 0;
    i22 = i21 + (HEAPU8[i4 + 8 >> 0] | 0) | 0;
    i23 = i22 + (HEAPU8[i4 + 9 >> 0] | 0) | 0;
    i24 = i23 + (HEAPU8[i4 + 10 >> 0] | 0) | 0;
    i25 = i24 + (HEAPU8[i4 + 11 >> 0] | 0) | 0;
    i26 = i25 + (HEAPU8[i4 + 12 >> 0] | 0) | 0;
    i27 = i26 + (HEAPU8[i4 + 13 >> 0] | 0) | 0;
    i28 = i27 + (HEAPU8[i4 + 14 >> 0] | 0) | 0;
    i29 = i28 + (HEAPU8[i4 + 15 >> 0] | 0) | 0;
    i30 = i14 + i13 + i15 + i16 + i17 + i18 + i19 + i20 + i21 + i22 + i23 + i24 + i25 + i26 + i27 + i28 + i29 | 0;
    i11 = i11 + -1 | 0;
    if (!i11) break; else {
     i4 = i4 + 16 | 0;
     i12 = i29;
     i13 = i30;
    }
   }
   if (i10 >>> 0 <= 5551) {
    i31 = (i29 >>> 0) % 65521 | 0;
    i32 = (i30 >>> 0) % 65521 | 0;
    break;
   } else {
    i8 = i8 + 5552 | 0;
    i9 = (i29 >>> 0) % 65521 | 0;
    i5 = (i30 >>> 0) % 65521 | 0;
   }
  }
  i30 = i2 + (i3 + -5552 - i7 + 5552) | 0;
  if (i7) if (i7 >>> 0 > 15) {
   i33 = i7;
   i34 = i30;
   i35 = i31;
   i36 = i32;
   i37 = 15;
  } else {
   i38 = i7;
   i39 = i30;
   i40 = i31;
   i41 = i32;
   i37 = 18;
  } else {
   i42 = i31;
   i43 = i32;
  }
 } else {
  i33 = i3;
  i34 = i2;
  i35 = i1 & 65535;
  i36 = i1 >>> 16;
  i37 = 15;
 }
 if ((i37 | 0) == 15) {
  i1 = i33 + -16 | 0;
  i2 = i34 + ((i1 & -16) + 16) | 0;
  i3 = i33;
  i33 = i34;
  i34 = i35;
  i35 = i36;
  while (1) {
   i3 = i3 + -16 | 0;
   i36 = (HEAPU8[i33 >> 0] | 0) + i34 | 0;
   i32 = i36 + (HEAPU8[i33 + 1 >> 0] | 0) | 0;
   i31 = i32 + (HEAPU8[i33 + 2 >> 0] | 0) | 0;
   i30 = i31 + (HEAPU8[i33 + 3 >> 0] | 0) | 0;
   i7 = i30 + (HEAPU8[i33 + 4 >> 0] | 0) | 0;
   i5 = i7 + (HEAPU8[i33 + 5 >> 0] | 0) | 0;
   i29 = i5 + (HEAPU8[i33 + 6 >> 0] | 0) | 0;
   i9 = i29 + (HEAPU8[i33 + 7 >> 0] | 0) | 0;
   i8 = i9 + (HEAPU8[i33 + 8 >> 0] | 0) | 0;
   i10 = i8 + (HEAPU8[i33 + 9 >> 0] | 0) | 0;
   i13 = i10 + (HEAPU8[i33 + 10 >> 0] | 0) | 0;
   i12 = i13 + (HEAPU8[i33 + 11 >> 0] | 0) | 0;
   i4 = i12 + (HEAPU8[i33 + 12 >> 0] | 0) | 0;
   i11 = i4 + (HEAPU8[i33 + 13 >> 0] | 0) | 0;
   i28 = i11 + (HEAPU8[i33 + 14 >> 0] | 0) | 0;
   i44 = i28 + (HEAPU8[i33 + 15 >> 0] | 0) | 0;
   i45 = i36 + i35 + i32 + i31 + i30 + i7 + i5 + i29 + i9 + i8 + i10 + i13 + i12 + i4 + i11 + i28 + i44 | 0;
   if (i3 >>> 0 <= 15) break; else {
    i33 = i33 + 16 | 0;
    i34 = i44;
    i35 = i45;
   }
  }
  if ((i1 | 0) == (i1 & -16 | 0)) {
   i46 = i44;
   i47 = i45;
   i37 = 19;
  } else {
   i38 = i1 - (i1 & -16) | 0;
   i39 = i2;
   i40 = i44;
   i41 = i45;
   i37 = 18;
  }
 }
 if ((i37 | 0) == 18) while (1) {
  i37 = 0;
  i38 = i38 + -1 | 0;
  i45 = (HEAPU8[i39 >> 0] | 0) + i40 | 0;
  i44 = i45 + i41 | 0;
  if (!i38) {
   i46 = i45;
   i47 = i44;
   i37 = 19;
   break;
  } else {
   i39 = i39 + 1 | 0;
   i40 = i45;
   i41 = i44;
   i37 = 18;
  }
 }
 if ((i37 | 0) == 19) {
  i42 = (i46 >>> 0) % 65521 | 0;
  i43 = (i47 >>> 0) % 65521 | 0;
 }
 i6 = i43 << 16 | i42;
 return i6 | 0;
}

function ___udivmoddi4(i1, i2, i3, i4, i5) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 i5 = i5 | 0;
 var i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0, i23 = 0;
 if (!i2) if (!i4) {
  if (i5) {
   HEAP32[i5 >> 2] = (i1 >>> 0) % (i3 >>> 0);
   HEAP32[i5 + 4 >> 2] = 0;
  }
  i6 = 0;
  i7 = (i1 >>> 0) / (i3 >>> 0) >>> 0;
  return (tempRet0 = i6, i7) | 0;
 } else {
  if (!i5) {
   i6 = 0;
   i7 = 0;
   return (tempRet0 = i6, i7) | 0;
  }
  HEAP32[i5 >> 2] = i1 | 0;
  HEAP32[i5 + 4 >> 2] = i2 & 0;
  i6 = 0;
  i7 = 0;
  return (tempRet0 = i6, i7) | 0;
 }
 do if (i3) {
  if (i4) {
   i8 = (Math_clz32(i4 | 0) | 0) - (Math_clz32(i2 | 0) | 0) | 0;
   if (i8 >>> 0 <= 31) {
    i9 = i8 + 1 | 0;
    i10 = i1 >>> ((i8 + 1 | 0) >>> 0) & i8 - 31 >> 31 | i2 << 31 - i8;
    i11 = i2 >>> ((i8 + 1 | 0) >>> 0) & i8 - 31 >> 31;
    i12 = 0;
    i13 = i1 << 31 - i8;
    break;
   }
   if (!i5) {
    i6 = 0;
    i7 = 0;
    return (tempRet0 = i6, i7) | 0;
   }
   HEAP32[i5 >> 2] = i1 | 0;
   HEAP32[i5 + 4 >> 2] = i2 | i2 & 0;
   i6 = 0;
   i7 = 0;
   return (tempRet0 = i6, i7) | 0;
  }
  if (i3 - 1 & i3) {
   i8 = (Math_clz32(i3 | 0) | 0) + 33 - (Math_clz32(i2 | 0) | 0) | 0;
   i9 = i8;
   i10 = 32 - i8 - 1 >> 31 & i2 >>> ((i8 - 32 | 0) >>> 0) | (i2 << 32 - i8 | i1 >>> (i8 >>> 0)) & i8 - 32 >> 31;
   i11 = i8 - 32 >> 31 & i2 >>> (i8 >>> 0);
   i12 = i1 << 64 - i8 & 32 - i8 >> 31;
   i13 = (i2 << 64 - i8 | i1 >>> ((i8 - 32 | 0) >>> 0)) & 32 - i8 >> 31 | i1 << 32 - i8 & i8 - 33 >> 31;
   break;
  }
  if (i5) {
   HEAP32[i5 >> 2] = i3 - 1 & i1;
   HEAP32[i5 + 4 >> 2] = 0;
  }
  if ((i3 | 0) == 1) {
   i6 = i2 | i2 & 0;
   i7 = i1 | 0 | 0;
   return (tempRet0 = i6, i7) | 0;
  } else {
   i8 = _llvm_cttz_i32(i3 | 0) | 0;
   i6 = i2 >>> (i8 >>> 0) | 0;
   i7 = i2 << 32 - i8 | i1 >>> (i8 >>> 0) | 0;
   return (tempRet0 = i6, i7) | 0;
  }
 } else {
  if (!i4) {
   if (i5) {
    HEAP32[i5 >> 2] = (i2 >>> 0) % (i3 >>> 0);
    HEAP32[i5 + 4 >> 2] = 0;
   }
   i6 = 0;
   i7 = (i2 >>> 0) / (i3 >>> 0) >>> 0;
   return (tempRet0 = i6, i7) | 0;
  }
  if (!i1) {
   if (i5) {
    HEAP32[i5 >> 2] = 0;
    HEAP32[i5 + 4 >> 2] = (i2 >>> 0) % (i4 >>> 0);
   }
   i6 = 0;
   i7 = (i2 >>> 0) / (i4 >>> 0) >>> 0;
   return (tempRet0 = i6, i7) | 0;
  }
  if (!(i4 - 1 & i4)) {
   if (i5) {
    HEAP32[i5 >> 2] = i1 | 0;
    HEAP32[i5 + 4 >> 2] = i4 - 1 & i2 | i2 & 0;
   }
   i6 = 0;
   i7 = i2 >>> ((_llvm_cttz_i32(i4 | 0) | 0) >>> 0);
   return (tempRet0 = i6, i7) | 0;
  }
  i8 = (Math_clz32(i4 | 0) | 0) - (Math_clz32(i2 | 0) | 0) | 0;
  if (i8 >>> 0 <= 30) {
   i9 = i8 + 1 | 0;
   i10 = i2 << 31 - i8 | i1 >>> ((i8 + 1 | 0) >>> 0);
   i11 = i2 >>> ((i8 + 1 | 0) >>> 0);
   i12 = 0;
   i13 = i1 << 31 - i8;
   break;
  }
  if (!i5) {
   i6 = 0;
   i7 = 0;
   return (tempRet0 = i6, i7) | 0;
  }
  HEAP32[i5 >> 2] = i1 | 0;
  HEAP32[i5 + 4 >> 2] = i2 | i2 & 0;
  i6 = 0;
  i7 = 0;
  return (tempRet0 = i6, i7) | 0;
 } while (0);
 if (!i9) {
  i14 = i13;
  i15 = i12;
  i16 = i11;
  i17 = i10;
  i18 = 0;
  i19 = 0;
 } else {
  i2 = _i64Add(i3 | 0 | 0, i4 | i4 & 0 | 0, -1, -1) | 0;
  i1 = tempRet0;
  i8 = i13;
  i13 = i12;
  i12 = i11;
  i11 = i10;
  i10 = i9;
  i9 = 0;
  do {
   i20 = i8;
   i8 = i13 >>> 31 | i8 << 1;
   i13 = i9 | i13 << 1;
   i21 = i11 << 1 | i20 >>> 31 | 0;
   i20 = i11 >>> 31 | i12 << 1 | 0;
   _i64Subtract(i2, i1, i21, i20) | 0;
   i22 = tempRet0;
   i23 = i22 >> 31 | ((i22 | 0) < 0 ? -1 : 0) << 1;
   i9 = i23 & 1;
   i11 = _i64Subtract(i21, i20, i23 & (i3 | 0), (((i22 | 0) < 0 ? -1 : 0) >> 31 | ((i22 | 0) < 0 ? -1 : 0) << 1) & (i4 | i4 & 0)) | 0;
   i12 = tempRet0;
   i10 = i10 - 1 | 0;
  } while ((i10 | 0) != 0);
  i14 = i8;
  i15 = i13;
  i16 = i12;
  i17 = i11;
  i18 = 0;
  i19 = i9;
 }
 i9 = i15;
 if (i5) {
  HEAP32[i5 >> 2] = i17;
  HEAP32[i5 + 4 >> 2] = i16;
 }
 i6 = (i9 | 0) >>> 31 | i14 << 1 | (0 << 1 | i9 >>> 31) & 0 | i18;
 i7 = (i9 << 1 | 0 >>> 31) & -2 | i19;
 return (tempRet0 = i6, i7) | 0;
}

function _deflate_stored(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0;
 i3 = (HEAP32[i1 + 12 >> 2] | 0) + -5 | 0;
 i4 = i3 >>> 0 < 65535 ? i3 : 65535;
 while (1) {
  i3 = HEAP32[i1 + 116 >> 2] | 0;
  if (i3 >>> 0 < 2) {
   _fill_window(i1);
   i5 = HEAP32[i1 + 116 >> 2] | 0;
   if (!(i5 | i2)) {
    i6 = 0;
    i7 = 28;
    break;
   }
   if (!i5) {
    i7 = 20;
    break;
   } else i8 = i5;
  } else i8 = i3;
  i3 = (HEAP32[i1 + 108 >> 2] | 0) + i8 | 0;
  HEAP32[i1 + 108 >> 2] = i3;
  HEAP32[i1 + 116 >> 2] = 0;
  i5 = HEAP32[i1 + 92 >> 2] | 0;
  if ((i3 | 0) != 0 & i3 >>> 0 < (i5 + i4 | 0) >>> 0) {
   i9 = i3;
   i10 = i5;
  } else {
   HEAP32[i1 + 116 >> 2] = i3 - (i5 + i4);
   HEAP32[i1 + 108 >> 2] = i5 + i4;
   if ((i5 | 0) > -1) i11 = (HEAP32[i1 + 56 >> 2] | 0) + i5 | 0; else i11 = 0;
   __tr_flush_block(i1, i11, i4, 0);
   HEAP32[i1 + 92 >> 2] = HEAP32[i1 + 108 >> 2];
   i5 = HEAP32[i1 >> 2] | 0;
   i3 = HEAP32[i5 + 28 >> 2] | 0;
   i12 = HEAP32[i3 + 20 >> 2] | 0;
   i13 = HEAP32[i5 + 16 >> 2] | 0;
   i14 = i12 >>> 0 > i13 >>> 0 ? i13 : i12;
   if ((i14 | 0) != 0 ? (_memcpy(HEAP32[i5 + 12 >> 2] | 0, HEAP32[i3 + 16 >> 2] | 0, i14 | 0) | 0, HEAP32[i5 + 12 >> 2] = (HEAP32[i5 + 12 >> 2] | 0) + i14, i3 = HEAP32[i5 + 28 >> 2] | 0, HEAP32[i3 + 16 >> 2] = (HEAP32[i3 + 16 >> 2] | 0) + i14, HEAP32[i5 + 20 >> 2] = (HEAP32[i5 + 20 >> 2] | 0) + i14, HEAP32[i5 + 16 >> 2] = (HEAP32[i5 + 16 >> 2] | 0) - i14, i5 = HEAP32[i3 + 20 >> 2] | 0, HEAP32[i3 + 20 >> 2] = i5 - i14, (i5 | 0) == (i14 | 0)) : 0) HEAP32[i3 + 16 >> 2] = HEAP32[i3 + 8 >> 2];
   if (!(HEAP32[(HEAP32[i1 >> 2] | 0) + 16 >> 2] | 0)) {
    i6 = 0;
    i7 = 28;
    break;
   }
   i9 = HEAP32[i1 + 108 >> 2] | 0;
   i10 = HEAP32[i1 + 92 >> 2] | 0;
  }
  i3 = i9 - i10 | 0;
  if (i3 >>> 0 < ((HEAP32[i1 + 44 >> 2] | 0) + -262 | 0) >>> 0) continue;
  if ((i10 | 0) > -1) i15 = (HEAP32[i1 + 56 >> 2] | 0) + i10 | 0; else i15 = 0;
  __tr_flush_block(i1, i15, i3, 0);
  HEAP32[i1 + 92 >> 2] = HEAP32[i1 + 108 >> 2];
  i3 = HEAP32[i1 >> 2] | 0;
  i14 = HEAP32[i3 + 28 >> 2] | 0;
  i5 = HEAP32[i14 + 20 >> 2] | 0;
  i12 = HEAP32[i3 + 16 >> 2] | 0;
  i13 = i5 >>> 0 > i12 >>> 0 ? i12 : i5;
  if ((i13 | 0) != 0 ? (_memcpy(HEAP32[i3 + 12 >> 2] | 0, HEAP32[i14 + 16 >> 2] | 0, i13 | 0) | 0, HEAP32[i3 + 12 >> 2] = (HEAP32[i3 + 12 >> 2] | 0) + i13, i14 = HEAP32[i3 + 28 >> 2] | 0, HEAP32[i14 + 16 >> 2] = (HEAP32[i14 + 16 >> 2] | 0) + i13, HEAP32[i3 + 20 >> 2] = (HEAP32[i3 + 20 >> 2] | 0) + i13, HEAP32[i3 + 16 >> 2] = (HEAP32[i3 + 16 >> 2] | 0) - i13, i3 = HEAP32[i14 + 20 >> 2] | 0, HEAP32[i14 + 20 >> 2] = i3 - i13, (i3 | 0) == (i13 | 0)) : 0) HEAP32[i14 + 16 >> 2] = HEAP32[i14 + 8 >> 2];
  if (!(HEAP32[(HEAP32[i1 >> 2] | 0) + 16 >> 2] | 0)) {
   i6 = 0;
   i7 = 28;
   break;
  }
 }
 if ((i7 | 0) == 20) {
  i15 = HEAP32[i1 + 92 >> 2] | 0;
  if ((i15 | 0) > -1) i16 = (HEAP32[i1 + 56 >> 2] | 0) + i15 | 0; else i16 = 0;
  __tr_flush_block(i1, i16, (HEAP32[i1 + 108 >> 2] | 0) - i15 | 0, (i2 | 0) == 4 & 1);
  HEAP32[i1 + 92 >> 2] = HEAP32[i1 + 108 >> 2];
  i15 = HEAP32[i1 >> 2] | 0;
  i16 = HEAP32[i15 + 28 >> 2] | 0;
  i10 = HEAP32[i16 + 20 >> 2] | 0;
  i9 = HEAP32[i15 + 16 >> 2] | 0;
  i4 = i10 >>> 0 > i9 >>> 0 ? i9 : i10;
  if ((i4 | 0) != 0 ? (_memcpy(HEAP32[i15 + 12 >> 2] | 0, HEAP32[i16 + 16 >> 2] | 0, i4 | 0) | 0, HEAP32[i15 + 12 >> 2] = (HEAP32[i15 + 12 >> 2] | 0) + i4, i16 = HEAP32[i15 + 28 >> 2] | 0, HEAP32[i16 + 16 >> 2] = (HEAP32[i16 + 16 >> 2] | 0) + i4, HEAP32[i15 + 20 >> 2] = (HEAP32[i15 + 20 >> 2] | 0) + i4, HEAP32[i15 + 16 >> 2] = (HEAP32[i15 + 16 >> 2] | 0) - i4, i15 = HEAP32[i16 + 20 >> 2] | 0, HEAP32[i16 + 20 >> 2] = i15 - i4, (i15 | 0) == (i4 | 0)) : 0) HEAP32[i16 + 16 >> 2] = HEAP32[i16 + 8 >> 2];
  if (!(HEAP32[(HEAP32[i1 >> 2] | 0) + 16 >> 2] | 0)) {
   i6 = (i2 | 0) == 4 ? 2 : 0;
   return i6 | 0;
  } else {
   i6 = (i2 | 0) == 4 ? 3 : 1;
   return i6 | 0;
  }
 } else if ((i7 | 0) == 28) return i6 | 0;
 return 0;
}

function __tr_align(i1) {
 i1 = i1 | 0;
 var i2 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0;
 i2 = HEAP32[i1 + 5820 >> 2] | 0;
 i3 = HEAPU16[i1 + 5816 >> 1] | 0 | 2 << i2;
 HEAP16[i1 + 5816 >> 1] = i3;
 if ((i2 | 0) > 13) {
  i4 = HEAP32[i1 + 20 >> 2] | 0;
  HEAP32[i1 + 20 >> 2] = i4 + 1;
  HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i4 >> 0] = i3;
  i4 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
  i5 = HEAP32[i1 + 20 >> 2] | 0;
  HEAP32[i1 + 20 >> 2] = i5 + 1;
  HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i5 >> 0] = i4;
  i4 = HEAP32[i1 + 5820 >> 2] | 0;
  HEAP16[i1 + 5816 >> 1] = 2 >>> (16 - i4 | 0);
  i6 = 2 >>> (16 - i4 | 0) & 65535;
  i7 = i4 + -13 | 0;
 } else {
  i6 = i3 & 65535;
  i7 = i2 + 3 | 0;
 }
 HEAP32[i1 + 5820 >> 2] = i7;
 if ((i7 | 0) > 9) {
  i2 = HEAP32[i1 + 20 >> 2] | 0;
  HEAP32[i1 + 20 >> 2] = i2 + 1;
  HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i2 >> 0] = i6;
  i2 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
  i3 = HEAP32[i1 + 20 >> 2] | 0;
  HEAP32[i1 + 20 >> 2] = i3 + 1;
  HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i3 >> 0] = i2;
  HEAP16[i1 + 5816 >> 1] = 0;
  i8 = (HEAP32[i1 + 5820 >> 2] | 0) + -9 | 0;
  i9 = 0;
 } else {
  i8 = i7 + 7 | 0;
  i9 = i6;
 }
 HEAP32[i1 + 5820 >> 2] = i8;
 if ((i8 | 0) != 16) if ((i8 | 0) > 7) {
  i6 = HEAP32[i1 + 20 >> 2] | 0;
  HEAP32[i1 + 20 >> 2] = i6 + 1;
  HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i6 >> 0] = i9;
  i6 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8;
  HEAP16[i1 + 5816 >> 1] = i6;
  i7 = (HEAP32[i1 + 5820 >> 2] | 0) + -8 | 0;
  HEAP32[i1 + 5820 >> 2] = i7;
  i10 = i7;
  i11 = i6;
 } else {
  i10 = i8;
  i11 = i9;
 } else {
  i8 = HEAP32[i1 + 20 >> 2] | 0;
  HEAP32[i1 + 20 >> 2] = i8 + 1;
  HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i8 >> 0] = i9;
  i9 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
  i8 = HEAP32[i1 + 20 >> 2] | 0;
  HEAP32[i1 + 20 >> 2] = i8 + 1;
  HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i8 >> 0] = i9;
  HEAP16[i1 + 5816 >> 1] = 0;
  HEAP32[i1 + 5820 >> 2] = 0;
  i10 = 0;
  i11 = 0;
 }
 if ((11 - i10 + (HEAP32[i1 + 5812 >> 2] | 0) | 0) >= 9) {
  HEAP32[i1 + 5812 >> 2] = 7;
  return;
 }
 i9 = i11 & 65535 | 2 << i10;
 HEAP16[i1 + 5816 >> 1] = i9;
 if ((i10 | 0) > 13) {
  i11 = HEAP32[i1 + 20 >> 2] | 0;
  HEAP32[i1 + 20 >> 2] = i11 + 1;
  HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i11 >> 0] = i9;
  i11 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
  i8 = HEAP32[i1 + 20 >> 2] | 0;
  HEAP32[i1 + 20 >> 2] = i8 + 1;
  HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i8 >> 0] = i11;
  i11 = HEAP32[i1 + 5820 >> 2] | 0;
  HEAP16[i1 + 5816 >> 1] = 2 >>> (16 - i11 | 0);
  i12 = 2 >>> (16 - i11 | 0);
  i13 = i11 + -13 | 0;
 } else {
  i12 = i9;
  i13 = i10 + 3 | 0;
 }
 i10 = i12 & 255;
 HEAP32[i1 + 5820 >> 2] = i13;
 if ((i13 | 0) > 9) {
  i12 = HEAP32[i1 + 20 >> 2] | 0;
  HEAP32[i1 + 20 >> 2] = i12 + 1;
  HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i12 >> 0] = i10;
  i12 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
  i9 = HEAP32[i1 + 20 >> 2] | 0;
  HEAP32[i1 + 20 >> 2] = i9 + 1;
  HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i9 >> 0] = i12;
  HEAP16[i1 + 5816 >> 1] = 0;
  i14 = 0;
  i15 = (HEAP32[i1 + 5820 >> 2] | 0) + -9 | 0;
 } else {
  i14 = i10;
  i15 = i13 + 7 | 0;
 }
 HEAP32[i1 + 5820 >> 2] = i15;
 if ((i15 | 0) == 16) {
  i13 = HEAP32[i1 + 20 >> 2] | 0;
  HEAP32[i1 + 20 >> 2] = i13 + 1;
  HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i13 >> 0] = i14;
  i13 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
  i10 = HEAP32[i1 + 20 >> 2] | 0;
  HEAP32[i1 + 20 >> 2] = i10 + 1;
  HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i10 >> 0] = i13;
  HEAP16[i1 + 5816 >> 1] = 0;
  HEAP32[i1 + 5820 >> 2] = 0;
  HEAP32[i1 + 5812 >> 2] = 7;
  return;
 }
 if ((i15 | 0) <= 7) {
  HEAP32[i1 + 5812 >> 2] = 7;
  return;
 }
 i15 = HEAP32[i1 + 20 >> 2] | 0;
 HEAP32[i1 + 20 >> 2] = i15 + 1;
 HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i15 >> 0] = i14;
 HEAP16[i1 + 5816 >> 1] = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8;
 HEAP32[i1 + 5820 >> 2] = (HEAP32[i1 + 5820 >> 2] | 0) + -8;
 HEAP32[i1 + 5812 >> 2] = 7;
 return;
}

function _deflateInit2_(i1, i2, i3, i4, i5, i6, i7, i8) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 i5 = i5 | 0;
 i6 = i6 | 0;
 i7 = i7 | 0;
 i8 = i8 | 0;
 var i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0;
 if (!i7) {
  i9 = -6;
  return i9 | 0;
 }
 if ((i8 | 0) != 56 | (HEAP8[i7 >> 0] | 0) != 49) {
  i9 = -6;
  return i9 | 0;
 }
 if (!i1) {
  i9 = -2;
  return i9 | 0;
 }
 HEAP32[i1 + 24 >> 2] = 0;
 i7 = HEAP32[i1 + 32 >> 2] | 0;
 if (!i7) {
  HEAP32[i1 + 32 >> 2] = 3;
  HEAP32[i1 + 40 >> 2] = 0;
  i10 = 3;
 } else i10 = i7;
 if (!(HEAP32[i1 + 36 >> 2] | 0)) HEAP32[i1 + 36 >> 2] = 1;
 i7 = (i2 | 0) == -1 ? 6 : i2;
 if ((i4 | 0) < 0) {
  i11 = 0 - i4 | 0;
  i12 = 0;
 } else {
  i11 = (i4 | 0) > 15 ? i4 + -16 | 0 : i4;
  i12 = (i4 | 0) > 15 ? 2 : 1;
 }
 if (i6 >>> 0 > 4 | (i7 >>> 0 > 9 | ((i3 | 0) != 8 | (i5 + -1 | 0) >>> 0 > 8 | (i11 & -8 | 0) != 8))) {
  i9 = -2;
  return i9 | 0;
 }
 i3 = (i11 | 0) == 8 ? 9 : i11;
 i11 = FUNCTION_TABLE_iiii[i10 & 7](HEAP32[i1 + 40 >> 2] | 0, 1, 5828) | 0;
 if (!i11) {
  i9 = -4;
  return i9 | 0;
 }
 HEAP32[i1 + 28 >> 2] = i11;
 HEAP32[i11 >> 2] = i1;
 HEAP32[i11 + 24 >> 2] = i12;
 HEAP32[i11 + 28 >> 2] = 0;
 HEAP32[i11 + 48 >> 2] = i3;
 HEAP32[i11 + 44 >> 2] = 1 << i3;
 HEAP32[i11 + 52 >> 2] = (1 << i3) + -1;
 HEAP32[i11 + 80 >> 2] = i5 + 7;
 HEAP32[i11 + 76 >> 2] = 1 << i5 + 7;
 HEAP32[i11 + 84 >> 2] = (1 << i5 + 7) + -1;
 HEAP32[i11 + 88 >> 2] = ((i5 + 9 | 0) >>> 0) / 3 | 0;
 HEAP32[i11 + 56 >> 2] = FUNCTION_TABLE_iiii[HEAP32[i1 + 32 >> 2] & 7](HEAP32[i1 + 40 >> 2] | 0, 1 << i3, 2) | 0;
 i3 = FUNCTION_TABLE_iiii[HEAP32[i1 + 32 >> 2] & 7](HEAP32[i1 + 40 >> 2] | 0, HEAP32[i11 + 44 >> 2] | 0, 2) | 0;
 HEAP32[i11 + 64 >> 2] = i3;
 _memset(i3 | 0, 0, HEAP32[i11 + 44 >> 2] << 1 | 0) | 0;
 HEAP32[i11 + 68 >> 2] = FUNCTION_TABLE_iiii[HEAP32[i1 + 32 >> 2] & 7](HEAP32[i1 + 40 >> 2] | 0, HEAP32[i11 + 76 >> 2] | 0, 2) | 0;
 HEAP32[i11 + 5824 >> 2] = 0;
 HEAP32[i11 + 5788 >> 2] = 1 << i5 + 6;
 i3 = FUNCTION_TABLE_iiii[HEAP32[i1 + 32 >> 2] & 7](HEAP32[i1 + 40 >> 2] | 0, 1 << i5 + 6, 4) | 0;
 HEAP32[i11 + 8 >> 2] = i3;
 i5 = HEAP32[i11 + 5788 >> 2] | 0;
 HEAP32[i11 + 12 >> 2] = i5 << 2;
 if (((HEAP32[i11 + 56 >> 2] | 0) != 0 ? (HEAP32[i11 + 64 >> 2] | 0) != 0 : 0) ? !((HEAP32[i11 + 68 >> 2] | 0) == 0 | (i3 | 0) == 0) : 0) {
  HEAP32[i11 + 5796 >> 2] = i3 + (i5 >>> 1 << 1);
  HEAP32[i11 + 5784 >> 2] = i3 + (i5 * 3 | 0);
  HEAP32[i11 + 132 >> 2] = i7;
  HEAP32[i11 + 136 >> 2] = i6;
  HEAP8[i11 + 36 >> 0] = 8;
  i9 = _deflateReset(i1) | 0;
  return i9 | 0;
 }
 HEAP32[i11 + 4 >> 2] = 666;
 HEAP32[i1 + 24 >> 2] = HEAP32[192];
 i11 = HEAP32[i1 + 28 >> 2] | 0;
 if (!i11) {
  i9 = -4;
  return i9 | 0;
 }
 switch (HEAP32[i11 + 4 >> 2] | 0) {
 case 42:
 case 69:
 case 73:
 case 91:
 case 103:
 case 113:
 case 666:
  break;
 default:
  {
   i9 = -4;
   return i9 | 0;
  }
 }
 i6 = HEAP32[i11 + 8 >> 2] | 0;
 if (!i6) i13 = i11; else {
  FUNCTION_TABLE_vii[HEAP32[i1 + 36 >> 2] & 1](HEAP32[i1 + 40 >> 2] | 0, i6);
  i13 = HEAP32[i1 + 28 >> 2] | 0;
 }
 i6 = HEAP32[i13 + 68 >> 2] | 0;
 if (!i6) i14 = i13; else {
  FUNCTION_TABLE_vii[HEAP32[i1 + 36 >> 2] & 1](HEAP32[i1 + 40 >> 2] | 0, i6);
  i14 = HEAP32[i1 + 28 >> 2] | 0;
 }
 i6 = HEAP32[i14 + 64 >> 2] | 0;
 if (!i6) i15 = i14; else {
  FUNCTION_TABLE_vii[HEAP32[i1 + 36 >> 2] & 1](HEAP32[i1 + 40 >> 2] | 0, i6);
  i15 = HEAP32[i1 + 28 >> 2] | 0;
 }
 i6 = HEAP32[i15 + 56 >> 2] | 0;
 if (!i6) i16 = i15; else {
  FUNCTION_TABLE_vii[HEAP32[i1 + 36 >> 2] & 1](HEAP32[i1 + 40 >> 2] | 0, i6);
  i16 = HEAP32[i1 + 28 >> 2] | 0;
 }
 FUNCTION_TABLE_vii[HEAP32[i1 + 36 >> 2] & 1](HEAP32[i1 + 40 >> 2] | 0, i16);
 HEAP32[i1 + 28 >> 2] = 0;
 i9 = -4;
 return i9 | 0;
}

function _crc32(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 var i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0;
 if (!i2) {
  i4 = 0;
  return i4 | 0;
 }
 L4 : do if (i3) {
  i5 = i2;
  i6 = i3;
  i7 = ~i1;
  while (1) {
   if (!(i5 & 3)) {
    i8 = i5;
    i9 = i6;
    i10 = i7;
    break;
   }
   i11 = HEAP32[784 + (((HEAPU8[i5 >> 0] | 0) ^ i7 & 255) << 2) >> 2] ^ i7 >>> 8;
   i6 = i6 + -1 | 0;
   if (!i6) {
    i12 = i11;
    break L4;
   } else {
    i5 = i5 + 1 | 0;
    i7 = i11;
   }
  }
  if (i9 >>> 0 > 31) {
   i7 = i9;
   i5 = i8;
   i6 = i10;
   while (1) {
    i11 = HEAP32[i5 >> 2] ^ i6;
    i13 = HEAP32[2832 + ((i11 >>> 8 & 255) << 2) >> 2] ^ HEAP32[3856 + ((i11 & 255) << 2) >> 2] ^ HEAP32[1808 + ((i11 >>> 16 & 255) << 2) >> 2] ^ HEAP32[784 + (i11 >>> 24 << 2) >> 2] ^ HEAP32[i5 + 4 >> 2];
    i11 = HEAP32[2832 + ((i13 >>> 8 & 255) << 2) >> 2] ^ HEAP32[3856 + ((i13 & 255) << 2) >> 2] ^ HEAP32[1808 + ((i13 >>> 16 & 255) << 2) >> 2] ^ HEAP32[784 + (i13 >>> 24 << 2) >> 2] ^ HEAP32[i5 + 8 >> 2];
    i13 = HEAP32[2832 + ((i11 >>> 8 & 255) << 2) >> 2] ^ HEAP32[3856 + ((i11 & 255) << 2) >> 2] ^ HEAP32[1808 + ((i11 >>> 16 & 255) << 2) >> 2] ^ HEAP32[784 + (i11 >>> 24 << 2) >> 2] ^ HEAP32[i5 + 12 >> 2];
    i11 = HEAP32[2832 + ((i13 >>> 8 & 255) << 2) >> 2] ^ HEAP32[3856 + ((i13 & 255) << 2) >> 2] ^ HEAP32[1808 + ((i13 >>> 16 & 255) << 2) >> 2] ^ HEAP32[784 + (i13 >>> 24 << 2) >> 2] ^ HEAP32[i5 + 16 >> 2];
    i13 = HEAP32[2832 + ((i11 >>> 8 & 255) << 2) >> 2] ^ HEAP32[3856 + ((i11 & 255) << 2) >> 2] ^ HEAP32[1808 + ((i11 >>> 16 & 255) << 2) >> 2] ^ HEAP32[784 + (i11 >>> 24 << 2) >> 2] ^ HEAP32[i5 + 20 >> 2];
    i11 = HEAP32[2832 + ((i13 >>> 8 & 255) << 2) >> 2] ^ HEAP32[3856 + ((i13 & 255) << 2) >> 2] ^ HEAP32[1808 + ((i13 >>> 16 & 255) << 2) >> 2] ^ HEAP32[784 + (i13 >>> 24 << 2) >> 2] ^ HEAP32[i5 + 24 >> 2];
    i13 = HEAP32[2832 + ((i11 >>> 8 & 255) << 2) >> 2] ^ HEAP32[3856 + ((i11 & 255) << 2) >> 2] ^ HEAP32[1808 + ((i11 >>> 16 & 255) << 2) >> 2] ^ HEAP32[784 + (i11 >>> 24 << 2) >> 2] ^ HEAP32[i5 + 28 >> 2];
    i14 = HEAP32[2832 + ((i13 >>> 8 & 255) << 2) >> 2] ^ HEAP32[3856 + ((i13 & 255) << 2) >> 2] ^ HEAP32[1808 + ((i13 >>> 16 & 255) << 2) >> 2] ^ HEAP32[784 + (i13 >>> 24 << 2) >> 2];
    i7 = i7 + -32 | 0;
    if (i7 >>> 0 <= 31) break; else {
     i5 = i5 + 32 | 0;
     i6 = i14;
    }
   }
   i15 = i9 + -32 - (i9 + -32 & -32) | 0;
   i16 = i8 + ((i9 + -32 & -32) + 32) | 0;
   i17 = i14;
  } else {
   i15 = i9;
   i16 = i8;
   i17 = i10;
  }
  if (i15 >>> 0 > 3) {
   i6 = i15 + -4 | 0;
   i5 = i15;
   i7 = i16;
   i13 = i17;
   while (1) {
    i11 = HEAP32[i7 >> 2] ^ i13;
    i18 = HEAP32[2832 + ((i11 >>> 8 & 255) << 2) >> 2] ^ HEAP32[3856 + ((i11 & 255) << 2) >> 2] ^ HEAP32[1808 + ((i11 >>> 16 & 255) << 2) >> 2] ^ HEAP32[784 + (i11 >>> 24 << 2) >> 2];
    i5 = i5 + -4 | 0;
    if (i5 >>> 0 <= 3) break; else {
     i7 = i7 + 4 | 0;
     i13 = i18;
    }
   }
   i19 = i6 - (i6 >>> 2 << 2) | 0;
   i20 = i16 + ((i6 >>> 2) + 1 << 2) | 0;
   i21 = i18;
  } else {
   i19 = i15;
   i20 = i16;
   i21 = i17;
  }
  if (!i19) i12 = i21; else {
   i13 = i20;
   i7 = i19;
   i5 = i21;
   while (1) {
    i11 = HEAP32[784 + (((HEAPU8[i13 >> 0] | 0) ^ i5 & 255) << 2) >> 2] ^ i5 >>> 8;
    i7 = i7 + -1 | 0;
    if (!i7) {
     i12 = i11;
     break;
    } else {
     i13 = i13 + 1 | 0;
     i5 = i11;
    }
   }
  }
 } else i12 = ~i1; while (0);
 i4 = ~i12;
 return i4 | 0;
}

function _fill_window(i1) {
 i1 = i1 | 0;
 var i2 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0;
 i2 = HEAP32[i1 + 44 >> 2] | 0;
 i3 = HEAP32[i1 + 116 >> 2] | 0;
 i4 = i2;
 while (1) {
  i5 = HEAP32[i1 + 108 >> 2] | 0;
  i6 = (HEAP32[i1 + 60 >> 2] | 0) - i3 - i5 | 0;
  if (i5 >>> 0 < (i2 + -262 + i4 | 0) >>> 0) {
   i7 = i5;
   i8 = i6;
  } else {
   i5 = HEAP32[i1 + 56 >> 2] | 0;
   _memcpy(i5 | 0, i5 + i2 | 0, i2 | 0) | 0;
   HEAP32[i1 + 112 >> 2] = (HEAP32[i1 + 112 >> 2] | 0) - i2;
   i5 = (HEAP32[i1 + 108 >> 2] | 0) - i2 | 0;
   HEAP32[i1 + 108 >> 2] = i5;
   HEAP32[i1 + 92 >> 2] = (HEAP32[i1 + 92 >> 2] | 0) - i2;
   i9 = HEAP32[i1 + 76 >> 2] | 0;
   i10 = i9;
   i11 = (HEAP32[i1 + 68 >> 2] | 0) + (i9 << 1) | 0;
   do {
    i11 = i11 + -2 | 0;
    i9 = HEAPU16[i11 >> 1] | 0;
    HEAP16[i11 >> 1] = i9 >>> 0 < i2 >>> 0 ? 0 : i9 - i2 & 65535;
    i10 = i10 + -1 | 0;
   } while ((i10 | 0) != 0);
   i10 = i2;
   i11 = (HEAP32[i1 + 64 >> 2] | 0) + (i2 << 1) | 0;
   do {
    i11 = i11 + -2 | 0;
    i9 = HEAPU16[i11 >> 1] | 0;
    HEAP16[i11 >> 1] = i9 >>> 0 < i2 >>> 0 ? 0 : i9 - i2 & 65535;
    i10 = i10 + -1 | 0;
   } while ((i10 | 0) != 0);
   i7 = i5;
   i8 = i6 + i2 | 0;
  }
  i10 = HEAP32[i1 >> 2] | 0;
  i11 = HEAP32[i10 + 4 >> 2] | 0;
  if (!i11) {
   i12 = 24;
   break;
  }
  i9 = HEAP32[i1 + 116 >> 2] | 0;
  i13 = (HEAP32[i1 + 56 >> 2] | 0) + (i9 + i7) | 0;
  i14 = i11 >>> 0 > i8 >>> 0 ? i8 : i11;
  if (!i14) {
   i15 = 0;
   i16 = i9;
  } else {
   HEAP32[i10 + 4 >> 2] = i11 - i14;
   switch (HEAP32[(HEAP32[i10 + 28 >> 2] | 0) + 24 >> 2] | 0) {
   case 1:
    {
     HEAP32[i10 + 48 >> 2] = _adler32(HEAP32[i10 + 48 >> 2] | 0, HEAP32[i10 >> 2] | 0, i14) | 0;
     i17 = i10;
     break;
    }
   case 2:
    {
     HEAP32[i10 + 48 >> 2] = _crc32(HEAP32[i10 + 48 >> 2] | 0, HEAP32[i10 >> 2] | 0, i14) | 0;
     i17 = i10;
     break;
    }
   default:
    i17 = i10;
   }
   _memcpy(i13 | 0, HEAP32[i17 >> 2] | 0, i14 | 0) | 0;
   HEAP32[i17 >> 2] = (HEAP32[i17 >> 2] | 0) + i14;
   HEAP32[i10 + 8 >> 2] = (HEAP32[i10 + 8 >> 2] | 0) + i14;
   i15 = i14;
   i16 = HEAP32[i1 + 116 >> 2] | 0;
  }
  i14 = i16 + i15 | 0;
  HEAP32[i1 + 116 >> 2] = i14;
  if (i14 >>> 0 > 2 ? (i10 = HEAP32[i1 + 108 >> 2] | 0, i13 = HEAP32[i1 + 56 >> 2] | 0, i11 = HEAPU8[i13 + i10 >> 0] | 0, HEAP32[i1 + 72 >> 2] = i11, HEAP32[i1 + 72 >> 2] = ((HEAPU8[i13 + (i10 + 1) >> 0] | 0) ^ i11 << HEAP32[i1 + 88 >> 2]) & HEAP32[i1 + 84 >> 2], i14 >>> 0 >= 262) : 0) {
   i18 = i14;
   break;
  }
  if (!(HEAP32[(HEAP32[i1 >> 2] | 0) + 4 >> 2] | 0)) {
   i18 = i14;
   break;
  }
  i3 = i14;
  i4 = HEAP32[i1 + 44 >> 2] | 0;
 }
 if ((i12 | 0) == 24) return;
 i12 = HEAP32[i1 + 5824 >> 2] | 0;
 i4 = HEAP32[i1 + 60 >> 2] | 0;
 if (i4 >>> 0 <= i12 >>> 0) return;
 i3 = i18 + (HEAP32[i1 + 108 >> 2] | 0) | 0;
 if (i12 >>> 0 < i3 >>> 0) {
  i18 = (i4 - i3 | 0) >>> 0 > 258 ? 258 : i4 - i3 | 0;
  _memset((HEAP32[i1 + 56 >> 2] | 0) + i3 | 0, 0, i18 | 0) | 0;
  HEAP32[i1 + 5824 >> 2] = i18 + i3;
  return;
 }
 if ((i3 + 258 | 0) >>> 0 <= i12 >>> 0) return;
 i18 = (i3 + 258 - i12 | 0) >>> 0 > (i4 - i12 | 0) >>> 0 ? i4 - i12 | 0 : i3 + 258 - i12 | 0;
 _memset((HEAP32[i1 + 56 >> 2] | 0) + i12 | 0, 0, i18 | 0) | 0;
 HEAP32[i1 + 5824 >> 2] = (HEAP32[i1 + 5824 >> 2] | 0) + i18;
 return;
}

function _longest_match(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0, i22 = 0;
 i3 = HEAP32[i1 + 124 >> 2] | 0;
 i4 = HEAP32[i1 + 56 >> 2] | 0;
 i5 = HEAP32[i1 + 108 >> 2] | 0;
 i6 = HEAP32[i1 + 120 >> 2] | 0;
 i7 = HEAP32[i1 + 144 >> 2] | 0;
 i8 = (HEAP32[i1 + 44 >> 2] | 0) + -262 | 0;
 i9 = i5 >>> 0 > i8 >>> 0 ? i5 - i8 | 0 : 0;
 i8 = HEAP32[i1 + 64 >> 2] | 0;
 i10 = HEAP32[i1 + 52 >> 2] | 0;
 i11 = HEAP32[i1 + 116 >> 2] | 0;
 i12 = i7 >>> 0 > i11 >>> 0 ? i11 : i7;
 i7 = i2;
 i2 = i6;
 i13 = i6 >>> 0 < (HEAP32[i1 + 140 >> 2] | 0) >>> 0 ? i3 : i3 >>> 2;
 i3 = HEAP8[i4 + (i6 + i5) >> 0] | 0;
 i14 = HEAP8[i4 + (i5 + -1 + i6) >> 0] | 0;
 while (1) {
  i6 = i4 + i7 | 0;
  if ((((HEAP8[i4 + (i7 + i2) >> 0] | 0) == i3 << 24 >> 24 ? (HEAP8[i4 + (i2 + -1 + i7) >> 0] | 0) == i14 << 24 >> 24 : 0) ? (HEAP8[i6 >> 0] | 0) == (HEAP8[i4 + i5 >> 0] | 0) : 0) ? (HEAP8[i4 + (i7 + 1) >> 0] | 0) == (HEAP8[i4 + (i5 + 1) >> 0] | 0) : 0) {
   i6 = i4 + (i7 + 2) | 0;
   i15 = i4 + (i5 + 2) | 0;
   while (1) {
    i16 = i15 + 1 | 0;
    if ((HEAP8[i16 >> 0] | 0) != (HEAP8[i6 + 1 >> 0] | 0)) {
     i17 = i16;
     break;
    }
    i16 = i15 + 2 | 0;
    if ((HEAP8[i16 >> 0] | 0) != (HEAP8[i6 + 2 >> 0] | 0)) {
     i17 = i16;
     break;
    }
    i16 = i15 + 3 | 0;
    if ((HEAP8[i16 >> 0] | 0) != (HEAP8[i6 + 3 >> 0] | 0)) {
     i17 = i16;
     break;
    }
    i16 = i15 + 4 | 0;
    if ((HEAP8[i16 >> 0] | 0) != (HEAP8[i6 + 4 >> 0] | 0)) {
     i17 = i16;
     break;
    }
    i16 = i15 + 5 | 0;
    if ((HEAP8[i16 >> 0] | 0) != (HEAP8[i6 + 5 >> 0] | 0)) {
     i17 = i16;
     break;
    }
    i16 = i15 + 6 | 0;
    if ((HEAP8[i16 >> 0] | 0) != (HEAP8[i6 + 6 >> 0] | 0)) {
     i17 = i16;
     break;
    }
    i16 = i15 + 7 | 0;
    if ((HEAP8[i16 >> 0] | 0) != (HEAP8[i6 + 7 >> 0] | 0)) {
     i17 = i16;
     break;
    }
    i16 = i15 + 8 | 0;
    i6 = i6 + 8 | 0;
    if (!(i16 >>> 0 < (i4 + (i5 + 258) | 0) >>> 0 ? (HEAP8[i16 >> 0] | 0) == (HEAP8[i6 >> 0] | 0) : 0)) {
     i17 = i16;
     break;
    } else i15 = i16;
   }
   i15 = i17 - (i4 + (i5 + 258)) | 0;
   if ((i15 + 258 | 0) > (i2 | 0)) {
    HEAP32[i1 + 112 >> 2] = i7;
    if ((i15 + 258 | 0) >= (i12 | 0)) {
     i18 = i15 + 258 | 0;
     i19 = 20;
     break;
    }
    i20 = i15 + 258 | 0;
    i21 = HEAP8[i4 + (i15 + 258 + i5) >> 0] | 0;
    i22 = HEAP8[i4 + (i5 + 257 + i15) >> 0] | 0;
   } else {
    i20 = i2;
    i21 = i3;
    i22 = i14;
   }
  } else {
   i20 = i2;
   i21 = i3;
   i22 = i14;
  }
  i7 = HEAPU16[i8 + ((i7 & i10) << 1) >> 1] | 0;
  if (i7 >>> 0 <= i9 >>> 0) {
   i18 = i20;
   i19 = 20;
   break;
  }
  i13 = i13 + -1 | 0;
  if (!i13) {
   i18 = i20;
   i19 = 20;
   break;
  } else {
   i2 = i20;
   i3 = i21;
   i14 = i22;
  }
 }
 if ((i19 | 0) == 20) return (i18 >>> 0 > i11 >>> 0 ? i11 : i18) | 0;
 return 0;
}

function _pop_arg555(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 var i4 = 0, i5 = 0, i6 = 0, d7 = 0.0;
 L1 : do if (i2 >>> 0 <= 20) do switch (i2 | 0) {
 case 9:
  {
   i4 = (HEAP32[i3 >> 2] | 0) + (4 - 1) & ~(4 - 1);
   i5 = HEAP32[i4 >> 2] | 0;
   HEAP32[i3 >> 2] = i4 + 4;
   HEAP32[i1 >> 2] = i5;
   break L1;
   break;
  }
 case 10:
  {
   i5 = (HEAP32[i3 >> 2] | 0) + (4 - 1) & ~(4 - 1);
   i4 = HEAP32[i5 >> 2] | 0;
   HEAP32[i3 >> 2] = i5 + 4;
   HEAP32[i1 >> 2] = i4;
   HEAP32[i1 + 4 >> 2] = ((i4 | 0) < 0) << 31 >> 31;
   break L1;
   break;
  }
 case 11:
  {
   i4 = (HEAP32[i3 >> 2] | 0) + (4 - 1) & ~(4 - 1);
   i5 = HEAP32[i4 >> 2] | 0;
   HEAP32[i3 >> 2] = i4 + 4;
   HEAP32[i1 >> 2] = i5;
   HEAP32[i1 + 4 >> 2] = 0;
   break L1;
   break;
  }
 case 12:
  {
   i5 = (HEAP32[i3 >> 2] | 0) + (8 - 1) & ~(8 - 1);
   i4 = HEAP32[i5 >> 2] | 0;
   i6 = HEAP32[i5 + 4 >> 2] | 0;
   HEAP32[i3 >> 2] = i5 + 8;
   HEAP32[i1 >> 2] = i4;
   HEAP32[i1 + 4 >> 2] = i6;
   break L1;
   break;
  }
 case 13:
  {
   i6 = (HEAP32[i3 >> 2] | 0) + (4 - 1) & ~(4 - 1);
   i4 = HEAP32[i6 >> 2] | 0;
   HEAP32[i3 >> 2] = i6 + 4;
   HEAP32[i1 >> 2] = (i4 & 65535) << 16 >> 16;
   HEAP32[i1 + 4 >> 2] = (((i4 & 65535) << 16 >> 16 | 0) < 0) << 31 >> 31;
   break L1;
   break;
  }
 case 14:
  {
   i4 = (HEAP32[i3 >> 2] | 0) + (4 - 1) & ~(4 - 1);
   i6 = HEAP32[i4 >> 2] | 0;
   HEAP32[i3 >> 2] = i4 + 4;
   HEAP32[i1 >> 2] = i6 & 65535;
   HEAP32[i1 + 4 >> 2] = 0;
   break L1;
   break;
  }
 case 15:
  {
   i6 = (HEAP32[i3 >> 2] | 0) + (4 - 1) & ~(4 - 1);
   i4 = HEAP32[i6 >> 2] | 0;
   HEAP32[i3 >> 2] = i6 + 4;
   HEAP32[i1 >> 2] = (i4 & 255) << 24 >> 24;
   HEAP32[i1 + 4 >> 2] = (((i4 & 255) << 24 >> 24 | 0) < 0) << 31 >> 31;
   break L1;
   break;
  }
 case 16:
  {
   i4 = (HEAP32[i3 >> 2] | 0) + (4 - 1) & ~(4 - 1);
   i6 = HEAP32[i4 >> 2] | 0;
   HEAP32[i3 >> 2] = i4 + 4;
   HEAP32[i1 >> 2] = i6 & 255;
   HEAP32[i1 + 4 >> 2] = 0;
   break L1;
   break;
  }
 case 17:
  {
   i6 = (HEAP32[i3 >> 2] | 0) + (8 - 1) & ~(8 - 1);
   d7 = +HEAPF64[i6 >> 3];
   HEAP32[i3 >> 2] = i6 + 8;
   HEAPF64[i1 >> 3] = d7;
   break L1;
   break;
  }
 case 18:
  {
   i6 = (HEAP32[i3 >> 2] | 0) + (8 - 1) & ~(8 - 1);
   d7 = +HEAPF64[i6 >> 3];
   HEAP32[i3 >> 2] = i6 + 8;
   HEAPF64[i1 >> 3] = d7;
   break L1;
   break;
  }
 default:
  break L1;
 } while (0); while (0);
 return;
}

function __tr_stored_block(i1, i2, i3, i4) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 var i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0;
 i5 = HEAP32[i1 + 5820 >> 2] | 0;
 i6 = HEAPU16[i1 + 5816 >> 1] | 0 | (i4 & 65535) << i5;
 HEAP16[i1 + 5816 >> 1] = i6;
 if ((i5 | 0) > 13) {
  i7 = HEAP32[i1 + 20 >> 2] | 0;
  HEAP32[i1 + 20 >> 2] = i7 + 1;
  HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i7 >> 0] = i6;
  i7 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
  i8 = HEAP32[i1 + 20 >> 2] | 0;
  HEAP32[i1 + 20 >> 2] = i8 + 1;
  HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i8 >> 0] = i7;
  i7 = HEAP32[i1 + 5820 >> 2] | 0;
  HEAP16[i1 + 5816 >> 1] = (i4 & 65535) >>> (16 - i7 | 0);
  i9 = (i4 & 65535) >>> (16 - i7 | 0);
  i10 = i7 + -13 | 0;
 } else {
  i9 = i6;
  i10 = i5 + 3 | 0;
 }
 i5 = i9 & 255;
 HEAP32[i1 + 5820 >> 2] = i10;
 do if ((i10 | 0) <= 8) if ((i10 | 0) > 0) {
  i9 = HEAP32[i1 + 20 >> 2] | 0;
  HEAP32[i1 + 20 >> 2] = i9 + 1;
  HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i9 >> 0] = i5;
  i11 = i1 + 20 | 0;
  i12 = i1 + 8 | 0;
  break;
 } else {
  i11 = i1 + 20 | 0;
  i12 = i1 + 8 | 0;
  break;
 } else {
  i9 = HEAP32[i1 + 20 >> 2] | 0;
  HEAP32[i1 + 20 >> 2] = i9 + 1;
  HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i9 >> 0] = i5;
  i9 = (HEAPU16[i1 + 5816 >> 1] | 0) >>> 8 & 255;
  i6 = HEAP32[i1 + 20 >> 2] | 0;
  HEAP32[i1 + 20 >> 2] = i6 + 1;
  HEAP8[(HEAP32[i1 + 8 >> 2] | 0) + i6 >> 0] = i9;
  i11 = i1 + 20 | 0;
  i12 = i1 + 8 | 0;
 } while (0);
 HEAP16[i1 + 5816 >> 1] = 0;
 HEAP32[i1 + 5820 >> 2] = 0;
 HEAP32[i1 + 5812 >> 2] = 8;
 i1 = HEAP32[i11 >> 2] | 0;
 HEAP32[i11 >> 2] = i1 + 1;
 HEAP8[(HEAP32[i12 >> 2] | 0) + i1 >> 0] = i3;
 i1 = HEAP32[i11 >> 2] | 0;
 HEAP32[i11 >> 2] = i1 + 1;
 HEAP8[(HEAP32[i12 >> 2] | 0) + i1 >> 0] = i3 >>> 8;
 i1 = HEAP32[i11 >> 2] | 0;
 HEAP32[i11 >> 2] = i1 + 1;
 HEAP8[(HEAP32[i12 >> 2] | 0) + i1 >> 0] = i3 & 65535 ^ 65535;
 i1 = HEAP32[i11 >> 2] | 0;
 HEAP32[i11 >> 2] = i1 + 1;
 HEAP8[(HEAP32[i12 >> 2] | 0) + i1 >> 0] = (i3 & 65535 ^ 65535) >>> 8;
 if (!i3) return; else {
  i13 = i3;
  i14 = i2;
 }
 while (1) {
  i13 = i13 + -1 | 0;
  i2 = HEAP8[i14 >> 0] | 0;
  i3 = HEAP32[i11 >> 2] | 0;
  HEAP32[i11 >> 2] = i3 + 1;
  HEAP8[(HEAP32[i12 >> 2] | 0) + i3 >> 0] = i2;
  if (!i13) break; else i14 = i14 + 1 | 0;
 }
 return;
}

function ___stdio_write(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 var i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0;
 i4 = STACKTOP;
 STACKTOP = STACKTOP + 48 | 0;
 i5 = HEAP32[i1 + 28 >> 2] | 0;
 HEAP32[i4 + 32 >> 2] = i5;
 i6 = (HEAP32[i1 + 20 >> 2] | 0) - i5 | 0;
 HEAP32[i4 + 32 + 4 >> 2] = i6;
 HEAP32[i4 + 32 + 8 >> 2] = i2;
 HEAP32[i4 + 32 + 12 >> 2] = i3;
 i2 = i4 + 32 | 0;
 i5 = 2;
 i7 = i6 + i3 | 0;
 while (1) {
  if (!(HEAP32[2244] | 0)) {
   HEAP32[i4 + 16 >> 2] = HEAP32[i1 + 60 >> 2];
   HEAP32[i4 + 16 + 4 >> 2] = i2;
   HEAP32[i4 + 16 + 8 >> 2] = i5;
   i8 = ___syscall_ret(___syscall146(146, i4 + 16 | 0) | 0) | 0;
  } else {
   _pthread_cleanup_push(1, i1 | 0);
   HEAP32[i4 >> 2] = HEAP32[i1 + 60 >> 2];
   HEAP32[i4 + 4 >> 2] = i2;
   HEAP32[i4 + 8 >> 2] = i5;
   i6 = ___syscall_ret(___syscall146(146, i4 | 0) | 0) | 0;
   _pthread_cleanup_pop(0);
   i8 = i6;
  }
  if ((i7 | 0) == (i8 | 0)) {
   i9 = 6;
   break;
  }
  if ((i8 | 0) < 0) {
   i10 = i2;
   i11 = i5;
   i9 = 8;
   break;
  }
  i6 = i7 - i8 | 0;
  i12 = HEAP32[i2 + 4 >> 2] | 0;
  if (i8 >>> 0 <= i12 >>> 0) if ((i5 | 0) == 2) {
   HEAP32[i1 + 28 >> 2] = (HEAP32[i1 + 28 >> 2] | 0) + i8;
   i13 = i12;
   i14 = i8;
   i15 = i2;
   i16 = 2;
  } else {
   i13 = i12;
   i14 = i8;
   i15 = i2;
   i16 = i5;
  } else {
   i17 = HEAP32[i1 + 44 >> 2] | 0;
   HEAP32[i1 + 28 >> 2] = i17;
   HEAP32[i1 + 20 >> 2] = i17;
   i13 = HEAP32[i2 + 12 >> 2] | 0;
   i14 = i8 - i12 | 0;
   i15 = i2 + 8 | 0;
   i16 = i5 + -1 | 0;
  }
  HEAP32[i15 >> 2] = (HEAP32[i15 >> 2] | 0) + i14;
  HEAP32[i15 + 4 >> 2] = i13 - i14;
  i2 = i15;
  i5 = i16;
  i7 = i6;
 }
 if ((i9 | 0) == 6) {
  i7 = HEAP32[i1 + 44 >> 2] | 0;
  HEAP32[i1 + 16 >> 2] = i7 + (HEAP32[i1 + 48 >> 2] | 0);
  HEAP32[i1 + 28 >> 2] = i7;
  HEAP32[i1 + 20 >> 2] = i7;
  i18 = i3;
 } else if ((i9 | 0) == 8) {
  HEAP32[i1 + 16 >> 2] = 0;
  HEAP32[i1 + 28 >> 2] = 0;
  HEAP32[i1 + 20 >> 2] = 0;
  HEAP32[i1 >> 2] = HEAP32[i1 >> 2] | 32;
  if ((i11 | 0) == 2) i18 = 0; else i18 = i3 - (HEAP32[i10 + 4 >> 2] | 0) | 0;
 }
 STACKTOP = i4;
 return i18 | 0;
}

function _memchr(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 var i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0, i15 = 0, i16 = 0, i17 = 0, i18 = 0, i19 = 0, i20 = 0, i21 = 0;
 L1 : do if ((i3 | 0) != 0 & (i1 & 3 | 0) != 0) {
  i4 = i3;
  i5 = i1;
  while (1) {
   if ((HEAP8[i5 >> 0] | 0) == (i2 & 255) << 24 >> 24) {
    i6 = i4;
    i7 = i5;
    i8 = 6;
    break L1;
   }
   i9 = i5 + 1 | 0;
   i10 = i4 + -1 | 0;
   if ((i10 | 0) != 0 & (i9 & 3 | 0) != 0) {
    i4 = i10;
    i5 = i9;
   } else {
    i11 = i10;
    i12 = (i10 | 0) != 0;
    i13 = i9;
    i8 = 5;
    break;
   }
  }
 } else {
  i11 = i3;
  i12 = (i3 | 0) != 0;
  i13 = i1;
  i8 = 5;
 } while (0);
 if ((i8 | 0) == 5) if (i12) {
  i6 = i11;
  i7 = i13;
  i8 = 6;
 } else {
  i14 = 0;
  i15 = i13;
 }
 L8 : do if ((i8 | 0) == 6) if ((HEAP8[i7 >> 0] | 0) == (i2 & 255) << 24 >> 24) {
  i14 = i6;
  i15 = i7;
 } else {
  i13 = Math_imul(i2 & 255, 16843009) | 0;
  L11 : do if (i6 >>> 0 > 3) {
   i11 = i6;
   i12 = i7;
   while (1) {
    i1 = HEAP32[i12 >> 2] ^ i13;
    if ((i1 & -2139062144 ^ -2139062144) & i1 + -16843009) {
     i16 = i11;
     i17 = i12;
     break;
    }
    i1 = i12 + 4 | 0;
    i3 = i11 + -4 | 0;
    if (i3 >>> 0 > 3) {
     i11 = i3;
     i12 = i1;
    } else {
     i18 = i3;
     i19 = i1;
     i8 = 11;
     break L11;
    }
   }
   i20 = i16;
   i21 = i17;
  } else {
   i18 = i6;
   i19 = i7;
   i8 = 11;
  } while (0);
  if ((i8 | 0) == 11) if (!i18) {
   i14 = 0;
   i15 = i19;
   break;
  } else {
   i20 = i18;
   i21 = i19;
  }
  while (1) {
   if ((HEAP8[i21 >> 0] | 0) == (i2 & 255) << 24 >> 24) {
    i14 = i20;
    i15 = i21;
    break L8;
   }
   i13 = i21 + 1 | 0;
   i20 = i20 + -1 | 0;
   if (!i20) {
    i14 = 0;
    i15 = i13;
    break;
   } else i21 = i13;
  }
 } while (0);
 return ((i14 | 0) != 0 ? i15 : 0) | 0;
}

function _init_block(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = 0;
 do {
  HEAP16[i1 + 148 + (i2 << 2) >> 1] = 0;
  i2 = i2 + 1 | 0;
 } while ((i2 | 0) != 286);
 HEAP16[i1 + 2440 >> 1] = 0;
 HEAP16[i1 + 2444 >> 1] = 0;
 HEAP16[i1 + 2448 >> 1] = 0;
 HEAP16[i1 + 2452 >> 1] = 0;
 HEAP16[i1 + 2456 >> 1] = 0;
 HEAP16[i1 + 2460 >> 1] = 0;
 HEAP16[i1 + 2464 >> 1] = 0;
 HEAP16[i1 + 2468 >> 1] = 0;
 HEAP16[i1 + 2472 >> 1] = 0;
 HEAP16[i1 + 2476 >> 1] = 0;
 HEAP16[i1 + 2480 >> 1] = 0;
 HEAP16[i1 + 2484 >> 1] = 0;
 HEAP16[i1 + 2488 >> 1] = 0;
 HEAP16[i1 + 2492 >> 1] = 0;
 HEAP16[i1 + 2496 >> 1] = 0;
 HEAP16[i1 + 2500 >> 1] = 0;
 HEAP16[i1 + 2504 >> 1] = 0;
 HEAP16[i1 + 2508 >> 1] = 0;
 HEAP16[i1 + 2512 >> 1] = 0;
 HEAP16[i1 + 2516 >> 1] = 0;
 HEAP16[i1 + 2520 >> 1] = 0;
 HEAP16[i1 + 2524 >> 1] = 0;
 HEAP16[i1 + 2528 >> 1] = 0;
 HEAP16[i1 + 2532 >> 1] = 0;
 HEAP16[i1 + 2536 >> 1] = 0;
 HEAP16[i1 + 2540 >> 1] = 0;
 HEAP16[i1 + 2544 >> 1] = 0;
 HEAP16[i1 + 2548 >> 1] = 0;
 HEAP16[i1 + 2552 >> 1] = 0;
 HEAP16[i1 + 2556 >> 1] = 0;
 HEAP16[i1 + 2684 >> 1] = 0;
 HEAP16[i1 + 2688 >> 1] = 0;
 HEAP16[i1 + 2692 >> 1] = 0;
 HEAP16[i1 + 2696 >> 1] = 0;
 HEAP16[i1 + 2700 >> 1] = 0;
 HEAP16[i1 + 2704 >> 1] = 0;
 HEAP16[i1 + 2708 >> 1] = 0;
 HEAP16[i1 + 2712 >> 1] = 0;
 HEAP16[i1 + 2716 >> 1] = 0;
 HEAP16[i1 + 2720 >> 1] = 0;
 HEAP16[i1 + 2724 >> 1] = 0;
 HEAP16[i1 + 2728 >> 1] = 0;
 HEAP16[i1 + 2732 >> 1] = 0;
 HEAP16[i1 + 2736 >> 1] = 0;
 HEAP16[i1 + 2740 >> 1] = 0;
 HEAP16[i1 + 2744 >> 1] = 0;
 HEAP16[i1 + 2748 >> 1] = 0;
 HEAP16[i1 + 2752 >> 1] = 0;
 HEAP16[i1 + 2756 >> 1] = 0;
 HEAP16[i1 + 1172 >> 1] = 1;
 HEAP32[i1 + 5804 >> 2] = 0;
 HEAP32[i1 + 5800 >> 2] = 0;
 HEAP32[i1 + 5808 >> 2] = 0;
 HEAP32[i1 + 5792 >> 2] = 0;
 return;
}

function _deflateReset(i1) {
 i1 = i1 | 0;
 var i2 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0;
 if (!i1) {
  i2 = -2;
  return i2 | 0;
 }
 i3 = HEAP32[i1 + 28 >> 2] | 0;
 if (!i3) {
  i2 = -2;
  return i2 | 0;
 }
 if (!(HEAP32[i1 + 32 >> 2] | 0)) {
  i2 = -2;
  return i2 | 0;
 }
 if (!(HEAP32[i1 + 36 >> 2] | 0)) {
  i2 = -2;
  return i2 | 0;
 }
 HEAP32[i1 + 20 >> 2] = 0;
 HEAP32[i1 + 8 >> 2] = 0;
 HEAP32[i1 + 24 >> 2] = 0;
 HEAP32[i1 + 44 >> 2] = 2;
 HEAP32[i3 + 20 >> 2] = 0;
 HEAP32[i3 + 16 >> 2] = HEAP32[i3 + 8 >> 2];
 i4 = HEAP32[i3 + 24 >> 2] | 0;
 if ((i4 | 0) < 0) {
  HEAP32[i3 + 24 >> 2] = 0 - i4;
  i5 = 0 - i4 | 0;
 } else i5 = i4;
 HEAP32[i3 + 4 >> 2] = (i5 | 0) != 0 ? 42 : 113;
 if ((i5 | 0) == 2) i6 = _crc32(0, 0, 0) | 0; else i6 = _adler32(0, 0, 0) | 0;
 HEAP32[i1 + 48 >> 2] = i6;
 HEAP32[i3 + 40 >> 2] = 0;
 __tr_init(i3);
 HEAP32[i3 + 60 >> 2] = HEAP32[i3 + 44 >> 2] << 1;
 i6 = HEAP32[i3 + 76 >> 2] | 0;
 i1 = HEAP32[i3 + 68 >> 2] | 0;
 HEAP16[i1 + (i6 + -1 << 1) >> 1] = 0;
 _memset(i1 | 0, 0, (i6 << 1) + -2 | 0) | 0;
 i6 = HEAP32[i3 + 132 >> 2] | 0;
 HEAP32[i3 + 128 >> 2] = HEAPU16[16 + (i6 * 12 | 0) + 2 >> 1];
 HEAP32[i3 + 140 >> 2] = HEAPU16[16 + (i6 * 12 | 0) >> 1];
 HEAP32[i3 + 144 >> 2] = HEAPU16[16 + (i6 * 12 | 0) + 4 >> 1];
 HEAP32[i3 + 124 >> 2] = HEAPU16[16 + (i6 * 12 | 0) + 6 >> 1];
 HEAP32[i3 + 108 >> 2] = 0;
 HEAP32[i3 + 92 >> 2] = 0;
 HEAP32[i3 + 116 >> 2] = 0;
 HEAP32[i3 + 120 >> 2] = 2;
 HEAP32[i3 + 96 >> 2] = 2;
 HEAP32[i3 + 112 >> 2] = 0;
 HEAP32[i3 + 104 >> 2] = 0;
 HEAP32[i3 + 72 >> 2] = 0;
 i2 = 0;
 return i2 | 0;
}

function _updatewindow(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0;
 i3 = HEAP32[i1 + 28 >> 2] | 0;
 i4 = HEAP32[i3 + 52 >> 2] | 0;
 if (!i4) {
  i5 = FUNCTION_TABLE_iiii[HEAP32[i1 + 32 >> 2] & 7](HEAP32[i1 + 40 >> 2] | 0, 1 << HEAP32[i3 + 36 >> 2], 1) | 0;
  HEAP32[i3 + 52 >> 2] = i5;
  if (!i5) {
   i6 = 1;
   return i6 | 0;
  } else i7 = i5;
 } else i7 = i4;
 i4 = HEAP32[i3 + 40 >> 2] | 0;
 if (!i4) {
  i5 = 1 << HEAP32[i3 + 36 >> 2];
  HEAP32[i3 + 40 >> 2] = i5;
  HEAP32[i3 + 48 >> 2] = 0;
  HEAP32[i3 + 44 >> 2] = 0;
  i8 = i5;
 } else i8 = i4;
 i4 = i2 - (HEAP32[i1 + 16 >> 2] | 0) | 0;
 if (i4 >>> 0 >= i8 >>> 0) {
  _memcpy(i7 | 0, (HEAP32[i1 + 12 >> 2] | 0) + (0 - i8) | 0, i8 | 0) | 0;
  HEAP32[i3 + 48 >> 2] = 0;
  HEAP32[i3 + 44 >> 2] = HEAP32[i3 + 40 >> 2];
  i6 = 0;
  return i6 | 0;
 }
 i2 = HEAP32[i3 + 48 >> 2] | 0;
 i5 = i8 - i2 | 0;
 i8 = i5 >>> 0 > i4 >>> 0 ? i4 : i5;
 _memcpy(i7 + i2 | 0, (HEAP32[i1 + 12 >> 2] | 0) + (0 - i4) | 0, i8 | 0) | 0;
 if ((i4 | 0) != (i8 | 0)) {
  _memcpy(HEAP32[i3 + 52 >> 2] | 0, (HEAP32[i1 + 12 >> 2] | 0) + (0 - (i4 - i8)) | 0, i4 - i8 | 0) | 0;
  HEAP32[i3 + 48 >> 2] = i4 - i8;
  HEAP32[i3 + 44 >> 2] = HEAP32[i3 + 40 >> 2];
  i6 = 0;
  return i6 | 0;
 }
 i8 = (HEAP32[i3 + 48 >> 2] | 0) + i4 | 0;
 i1 = HEAP32[i3 + 40 >> 2] | 0;
 HEAP32[i3 + 48 >> 2] = (i8 | 0) == (i1 | 0) ? 0 : i8;
 i8 = HEAP32[i3 + 44 >> 2] | 0;
 if (i8 >>> 0 >= i1 >>> 0) {
  i6 = 0;
  return i6 | 0;
 }
 HEAP32[i3 + 44 >> 2] = i8 + i4;
 i6 = 0;
 return i6 | 0;
}

function _vfprintf(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 var i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0;
 i4 = STACKTOP;
 STACKTOP = STACKTOP + 224 | 0;
 i5 = i4 + 96 | 0;
 i6 = i5 + 40 | 0;
 do {
  HEAP32[i5 >> 2] = 0;
  i5 = i5 + 4 | 0;
 } while ((i5 | 0) < (i6 | 0));
 HEAP32[i4 + 80 >> 2] = HEAP32[i3 >> 2];
 if ((_printf_core(0, i2, i4 + 80 | 0, i4, i4 + 96 | 0) | 0) < 0) i7 = -1; else {
  if ((HEAP32[i1 + 76 >> 2] | 0) > -1) i8 = ___lockfile(i1) | 0; else i8 = 0;
  i3 = HEAP32[i1 >> 2] | 0;
  if ((HEAP8[i1 + 74 >> 0] | 0) < 1) HEAP32[i1 >> 2] = i3 & -33;
  if (!(HEAP32[i1 + 48 >> 2] | 0)) {
   i5 = HEAP32[i1 + 44 >> 2] | 0;
   HEAP32[i1 + 44 >> 2] = i4 + 136;
   HEAP32[i1 + 28 >> 2] = i4 + 136;
   HEAP32[i1 + 20 >> 2] = i4 + 136;
   HEAP32[i1 + 48 >> 2] = 80;
   HEAP32[i1 + 16 >> 2] = i4 + 136 + 80;
   i6 = _printf_core(i1, i2, i4 + 80 | 0, i4, i4 + 96 | 0) | 0;
   if (!i5) i9 = i6; else {
    FUNCTION_TABLE_iiii[HEAP32[i1 + 36 >> 2] & 7](i1, 0, 0) | 0;
    i10 = (HEAP32[i1 + 20 >> 2] | 0) == 0 ? -1 : i6;
    HEAP32[i1 + 44 >> 2] = i5;
    HEAP32[i1 + 48 >> 2] = 0;
    HEAP32[i1 + 16 >> 2] = 0;
    HEAP32[i1 + 28 >> 2] = 0;
    HEAP32[i1 + 20 >> 2] = 0;
    i9 = i10;
   }
  } else i9 = _printf_core(i1, i2, i4 + 80 | 0, i4, i4 + 96 | 0) | 0;
  i2 = HEAP32[i1 >> 2] | 0;
  HEAP32[i1 >> 2] = i2 | i3 & 32;
  if (i8) ___unlockfile(i1);
  i7 = (i2 & 32 | 0) == 0 ? i9 : -1;
 }
 STACKTOP = i4;
 return i7 | 0;
}

function _deflateEnd(i1) {
 i1 = i1 | 0;
 var i2 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0;
 if (!i1) {
  i2 = -2;
  return i2 | 0;
 }
 i3 = HEAP32[i1 + 28 >> 2] | 0;
 if (!i3) {
  i2 = -2;
  return i2 | 0;
 }
 i4 = HEAP32[i3 + 4 >> 2] | 0;
 switch (i4 | 0) {
 case 42:
 case 69:
 case 73:
 case 91:
 case 103:
 case 113:
 case 666:
  break;
 default:
  {
   i2 = -2;
   return i2 | 0;
  }
 }
 i5 = HEAP32[i3 + 8 >> 2] | 0;
 if (!i5) i6 = i3; else {
  FUNCTION_TABLE_vii[HEAP32[i1 + 36 >> 2] & 1](HEAP32[i1 + 40 >> 2] | 0, i5);
  i6 = HEAP32[i1 + 28 >> 2] | 0;
 }
 i5 = HEAP32[i6 + 68 >> 2] | 0;
 if (!i5) i7 = i6; else {
  FUNCTION_TABLE_vii[HEAP32[i1 + 36 >> 2] & 1](HEAP32[i1 + 40 >> 2] | 0, i5);
  i7 = HEAP32[i1 + 28 >> 2] | 0;
 }
 i5 = HEAP32[i7 + 64 >> 2] | 0;
 if (!i5) i8 = i7; else {
  FUNCTION_TABLE_vii[HEAP32[i1 + 36 >> 2] & 1](HEAP32[i1 + 40 >> 2] | 0, i5);
  i8 = HEAP32[i1 + 28 >> 2] | 0;
 }
 i5 = HEAP32[i8 + 56 >> 2] | 0;
 if (!i5) {
  i9 = i1 + 40 | 0;
  i10 = i1 + 36 | 0;
  i11 = i8;
 } else {
  FUNCTION_TABLE_vii[HEAP32[i1 + 36 >> 2] & 1](HEAP32[i1 + 40 >> 2] | 0, i5);
  i9 = i1 + 40 | 0;
  i10 = i1 + 36 | 0;
  i11 = HEAP32[i1 + 28 >> 2] | 0;
 }
 FUNCTION_TABLE_vii[HEAP32[i10 >> 2] & 1](HEAP32[i9 >> 2] | 0, i11);
 HEAP32[i1 + 28 >> 2] = 0;
 i2 = (i4 | 0) == 113 ? -3 : 0;
 return i2 | 0;
}

function _inflateInit_(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 var i4 = 0, i5 = 0;
 if (!i2) {
  i4 = -6;
  return i4 | 0;
 }
 if ((i3 | 0) != 56 | (HEAP8[i2 >> 0] | 0) != 49) {
  i4 = -6;
  return i4 | 0;
 }
 if (!i1) {
  i4 = -2;
  return i4 | 0;
 }
 HEAP32[i1 + 24 >> 2] = 0;
 i2 = HEAP32[i1 + 32 >> 2] | 0;
 if (!i2) {
  HEAP32[i1 + 32 >> 2] = 3;
  HEAP32[i1 + 40 >> 2] = 0;
  i5 = 3;
 } else i5 = i2;
 if (!(HEAP32[i1 + 36 >> 2] | 0)) HEAP32[i1 + 36 >> 2] = 1;
 i2 = FUNCTION_TABLE_iiii[i5 & 7](HEAP32[i1 + 40 >> 2] | 0, 1, 7116) | 0;
 if (!i2) {
  i4 = -4;
  return i4 | 0;
 }
 HEAP32[i1 + 28 >> 2] = i2;
 HEAP32[i2 + 52 >> 2] = 0;
 HEAP32[i2 + 8 >> 2] = 1;
 HEAP32[i2 + 36 >> 2] = 15;
 HEAP32[i2 + 28 >> 2] = 0;
 HEAP32[i1 + 20 >> 2] = 0;
 HEAP32[i1 + 8 >> 2] = 0;
 HEAP32[i1 + 24 >> 2] = 0;
 HEAP32[i1 + 48 >> 2] = 1;
 HEAP32[i2 >> 2] = 0;
 HEAP32[i2 + 4 >> 2] = 0;
 HEAP32[i2 + 12 >> 2] = 0;
 HEAP32[i2 + 20 >> 2] = 32768;
 HEAP32[i2 + 32 >> 2] = 0;
 HEAP32[i2 + 40 >> 2] = 0;
 HEAP32[i2 + 44 >> 2] = 0;
 HEAP32[i2 + 48 >> 2] = 0;
 HEAP32[i2 + 56 >> 2] = 0;
 HEAP32[i2 + 60 >> 2] = 0;
 HEAP32[i2 + 108 >> 2] = i2 + 1328;
 HEAP32[i2 + 80 >> 2] = i2 + 1328;
 HEAP32[i2 + 76 >> 2] = i2 + 1328;
 HEAP32[i2 + 7104 >> 2] = 1;
 HEAP32[i2 + 7108 >> 2] = -1;
 i4 = 0;
 return i4 | 0;
}

function ___fwritex(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 var i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0, i13 = 0, i14 = 0;
 i4 = HEAP32[i3 + 16 >> 2] | 0;
 if (!i4) if (!(___towrite(i3) | 0)) {
  i5 = HEAP32[i3 + 16 >> 2] | 0;
  i6 = 4;
 } else i7 = 0; else {
  i5 = i4;
  i6 = 4;
 }
 L4 : do if ((i6 | 0) == 4) {
  i4 = HEAP32[i3 + 20 >> 2] | 0;
  if ((i5 - i4 | 0) >>> 0 < i2 >>> 0) {
   i7 = FUNCTION_TABLE_iiii[HEAP32[i3 + 36 >> 2] & 7](i3, i1, i2) | 0;
   break;
  }
  L9 : do if ((HEAP8[i3 + 75 >> 0] | 0) > -1) {
   i8 = i2;
   while (1) {
    if (!i8) {
     i9 = i2;
     i10 = i1;
     i11 = i4;
     i12 = 0;
     break L9;
    }
    i13 = i8 + -1 | 0;
    if ((HEAP8[i1 + i13 >> 0] | 0) == 10) {
     i14 = i8;
     break;
    } else i8 = i13;
   }
   if ((FUNCTION_TABLE_iiii[HEAP32[i3 + 36 >> 2] & 7](i3, i1, i14) | 0) >>> 0 < i14 >>> 0) {
    i7 = i14;
    break L4;
   }
   i9 = i2 - i14 | 0;
   i10 = i1 + i14 | 0;
   i11 = HEAP32[i3 + 20 >> 2] | 0;
   i12 = i14;
  } else {
   i9 = i2;
   i10 = i1;
   i11 = i4;
   i12 = 0;
  } while (0);
  _memcpy(i11 | 0, i10 | 0, i9 | 0) | 0;
  HEAP32[i3 + 20 >> 2] = (HEAP32[i3 + 20 >> 2] | 0) + i9;
  i7 = i12 + i9 | 0;
 } while (0);
 return i7 | 0;
}

function _fflush(i1) {
 i1 = i1 | 0;
 var i2 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0;
 do if (i1) {
  if ((HEAP32[i1 + 76 >> 2] | 0) <= -1) {
   i2 = ___fflush_unlocked(i1) | 0;
   break;
  }
  i3 = (___lockfile(i1) | 0) == 0;
  i4 = ___fflush_unlocked(i1) | 0;
  if (i3) i2 = i4; else {
   ___unlockfile(i1);
   i2 = i4;
  }
 } else {
  if (!(HEAP32[2256] | 0)) i5 = 0; else i5 = _fflush(HEAP32[2256] | 0) | 0;
  ___lock(9004);
  i4 = HEAP32[2250] | 0;
  if (!i4) i6 = i5; else {
   i3 = i4;
   i4 = i5;
   while (1) {
    if ((HEAP32[i3 + 76 >> 2] | 0) > -1) i7 = ___lockfile(i3) | 0; else i7 = 0;
    if ((HEAP32[i3 + 20 >> 2] | 0) >>> 0 > (HEAP32[i3 + 28 >> 2] | 0) >>> 0) i8 = ___fflush_unlocked(i3) | 0 | i4; else i8 = i4;
    if (i7) ___unlockfile(i3);
    i3 = HEAP32[i3 + 56 >> 2] | 0;
    if (!i3) {
     i6 = i8;
     break;
    } else i4 = i8;
   }
  }
  ___unlock(9004);
  i2 = i6;
 } while (0);
 return i2 | 0;
}

function _uncompress(i1, i2, i3, i4) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 var i5 = 0, i6 = 0, i7 = 0;
 i5 = STACKTOP;
 STACKTOP = STACKTOP + 64 | 0;
 HEAP32[i5 >> 2] = i3;
 HEAP32[i5 + 4 >> 2] = i4;
 HEAP32[i5 + 12 >> 2] = i1;
 HEAP32[i5 + 16 >> 2] = HEAP32[i2 >> 2];
 HEAP32[i5 + 32 >> 2] = 0;
 HEAP32[i5 + 36 >> 2] = 0;
 i1 = _inflateInit_(i5, 14249, 56) | 0;
 if (i1) {
  i6 = i1;
  STACKTOP = i5;
  return i6 | 0;
 }
 i1 = _inflate(i5, 4) | 0;
 if ((i1 | 0) == 1) {
  HEAP32[i2 >> 2] = HEAP32[i5 + 20 >> 2];
  i6 = _inflateEnd(i5) | 0;
  STACKTOP = i5;
  return i6 | 0;
 }
 _inflateEnd(i5) | 0;
 switch (i1 | 0) {
 case -5:
  {
   i7 = 4;
   break;
  }
 case 2:
  {
   i6 = -3;
   STACKTOP = i5;
   return i6 | 0;
  }
 default:
  {}
 }
 if ((i7 | 0) == 4 ? (HEAP32[i5 + 4 >> 2] | 0) == 0 : 0) {
  i6 = -3;
  STACKTOP = i5;
  return i6 | 0;
 }
 i6 = i1;
 STACKTOP = i5;
 return i6 | 0;
}

function _doit(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 var i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0;
 i4 = STACKTOP;
 STACKTOP = STACKTOP + 16 | 0;
 i5 = _compressBound(i2) | 0;
 i6 = HEAP32[2] | 0;
 if (!i6) {
  i7 = _malloc(i5) | 0;
  HEAP32[2] = i7;
  i8 = i7;
 } else i8 = i6;
 if (!(HEAP32[3] | 0)) HEAP32[3] = _malloc(i2) | 0;
 HEAP32[i4 + 12 >> 2] = i5;
 _compress(i8, i4 + 12 | 0, i1, i2) | 0;
 if (!i3) {
  i8 = HEAP32[i4 + 12 >> 2] | 0;
  HEAP32[i4 >> 2] = i2;
  HEAP32[i4 + 4 >> 2] = i8;
  _printf(13378, i4) | 0;
 }
 HEAP32[i4 + 8 >> 2] = i2;
 _uncompress(HEAP32[3] | 0, i4 + 8 | 0, HEAP32[2] | 0, HEAP32[i4 + 12 >> 2] | 0) | 0;
 if ((HEAP32[i4 + 8 >> 2] | 0) != (i2 | 0)) ___assert_fail(13392, 13417, 24, 13424);
 if (i3) {
  STACKTOP = i4;
  return;
 }
 if (!(_strcmp(i1, HEAP32[3] | 0) | 0)) {
  STACKTOP = i4;
  return;
 } else ___assert_fail(13429, 13417, 25, 13424);
}

function _pad(i1, i2, i3, i4, i5) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 i5 = i5 | 0;
 var i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0, i12 = 0;
 i6 = STACKTOP;
 STACKTOP = STACKTOP + 256 | 0;
 do if ((i3 | 0) > (i4 | 0) & (i5 & 73728 | 0) == 0) {
  _memset(i6 | 0, i2 | 0, ((i3 - i4 | 0) >>> 0 > 256 ? 256 : i3 - i4 | 0) | 0) | 0;
  i7 = HEAP32[i1 >> 2] | 0;
  if ((i3 - i4 | 0) >>> 0 > 255) {
   i8 = i3 - i4 | 0;
   i9 = i7;
   i10 = (i7 & 32 | 0) == 0;
   while (1) {
    if (i10) {
     ___fwritex(i6, 256, i1) | 0;
     i11 = HEAP32[i1 >> 2] | 0;
    } else i11 = i9;
    i8 = i8 + -256 | 0;
    i10 = (i11 & 32 | 0) == 0;
    if (i8 >>> 0 <= 255) break; else i9 = i11;
   }
   if (i10) i12 = i3 - i4 & 255; else break;
  } else if (!(i7 & 32)) i12 = i3 - i4 | 0; else break;
  ___fwritex(i6, i12, i1) | 0;
 } while (0);
 STACKTOP = i6;
 return;
}

function _wcrtomb(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 var i4 = 0;
 do if (i1) {
  if (i2 >>> 0 < 128) {
   HEAP8[i1 >> 0] = i2;
   i4 = 1;
   break;
  }
  if (i2 >>> 0 < 2048) {
   HEAP8[i1 >> 0] = i2 >>> 6 | 192;
   HEAP8[i1 + 1 >> 0] = i2 & 63 | 128;
   i4 = 2;
   break;
  }
  if (i2 >>> 0 < 55296 | (i2 & -8192 | 0) == 57344) {
   HEAP8[i1 >> 0] = i2 >>> 12 | 224;
   HEAP8[i1 + 1 >> 0] = i2 >>> 6 & 63 | 128;
   HEAP8[i1 + 2 >> 0] = i2 & 63 | 128;
   i4 = 3;
   break;
  }
  if ((i2 + -65536 | 0) >>> 0 < 1048576) {
   HEAP8[i1 >> 0] = i2 >>> 18 | 240;
   HEAP8[i1 + 1 >> 0] = i2 >>> 12 & 63 | 128;
   HEAP8[i1 + 2 >> 0] = i2 >>> 6 & 63 | 128;
   HEAP8[i1 + 3 >> 0] = i2 & 63 | 128;
   i4 = 4;
   break;
  } else {
   HEAP32[(___errno_location() | 0) >> 2] = 84;
   i4 = -1;
   break;
  }
 } else i4 = 1; while (0);
 return i4 | 0;
}

function _fmt_u(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 var i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0, i11 = 0;
 if (i2 >>> 0 > 0 | (i2 | 0) == 0 & i1 >>> 0 > 4294967295) {
  i4 = i3;
  i5 = i1;
  i6 = i2;
  while (1) {
   i2 = ___uremdi3(i5 | 0, i6 | 0, 10, 0) | 0;
   i7 = i4 + -1 | 0;
   HEAP8[i7 >> 0] = i2 | 48;
   i8 = ___udivdi3(i5 | 0, i6 | 0, 10, 0) | 0;
   if (i6 >>> 0 > 9 | (i6 | 0) == 9 & i5 >>> 0 > 4294967295) {
    i4 = i7;
    i5 = i8;
    i6 = tempRet0;
   } else break;
  }
  i9 = i7;
  i10 = i8;
 } else {
  i9 = i3;
  i10 = i1;
 }
 if (!i10) i11 = i9; else {
  i1 = i9;
  i9 = i10;
  while (1) {
   i10 = i1 + -1 | 0;
   HEAP8[i10 >> 0] = (i9 >>> 0) % 10 | 0 | 48;
   if (i9 >>> 0 < 10) {
    i11 = i10;
    break;
   } else {
    i1 = i10;
    i9 = (i9 >>> 0) / 10 | 0;
   }
  }
 }
 return i11 | 0;
}

function _strlen(i1) {
 i1 = i1 | 0;
 var i2 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0;
 L1 : do if (!(i1 & 3)) {
  i2 = i1;
  i3 = 4;
 } else {
  i4 = i1;
  i5 = i1;
  while (1) {
   if (!(HEAP8[i4 >> 0] | 0)) {
    i6 = i5;
    break L1;
   }
   i7 = i4 + 1 | 0;
   i5 = i7;
   if (!(i5 & 3)) {
    i2 = i7;
    i3 = 4;
    break;
   } else i4 = i7;
  }
 } while (0);
 if ((i3 | 0) == 4) {
  i3 = i2;
  while (1) {
   i8 = HEAP32[i3 >> 2] | 0;
   if (!((i8 & -2139062144 ^ -2139062144) & i8 + -16843009)) i3 = i3 + 4 | 0; else {
    i9 = i3;
    break;
   }
  }
  if (!((i8 & 255) << 24 >> 24)) i10 = i9; else {
   i8 = i9;
   while (1) {
    i9 = i8 + 1 | 0;
    if (!(HEAP8[i9 >> 0] | 0)) {
     i10 = i9;
     break;
    } else i8 = i9;
   }
  }
  i6 = i10;
 }
 return i6 - i1 | 0;
}
function _frexp(d1, i2) {
 d1 = +d1;
 i2 = i2 | 0;
 var i3 = 0, i4 = 0, i5 = 0, d6 = 0.0, d7 = 0.0, i8 = 0, d9 = 0.0;
 HEAPF64[tempDoublePtr >> 3] = d1;
 i3 = HEAP32[tempDoublePtr >> 2] | 0;
 i4 = HEAP32[tempDoublePtr + 4 >> 2] | 0;
 i5 = _bitshift64Lshr(i3 | 0, i4 | 0, 52) | 0;
 switch (i5 & 2047 | 0) {
 case 0:
  {
   if (d1 != 0.0) {
    d6 = +_frexp(d1 * 18446744073709551616.0, i2);
    d7 = d6;
    i8 = (HEAP32[i2 >> 2] | 0) + -64 | 0;
   } else {
    d7 = d1;
    i8 = 0;
   }
   HEAP32[i2 >> 2] = i8;
   d9 = d7;
   break;
  }
 case 2047:
  {
   d9 = d1;
   break;
  }
 default:
  {
   HEAP32[i2 >> 2] = (i5 & 2047) + -1022;
   HEAP32[tempDoublePtr >> 2] = i3;
   HEAP32[tempDoublePtr + 4 >> 2] = i4 & -2146435073 | 1071644672;
   d9 = +HEAPF64[tempDoublePtr >> 3];
  }
 }
 return +d9;
}

function _compress(i1, i2, i3, i4) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 var i5 = 0, i6 = 0;
 i5 = STACKTOP;
 STACKTOP = STACKTOP + 64 | 0;
 HEAP32[i5 >> 2] = i3;
 HEAP32[i5 + 4 >> 2] = i4;
 HEAP32[i5 + 12 >> 2] = i1;
 HEAP32[i5 + 16 >> 2] = HEAP32[i2 >> 2];
 HEAP32[i5 + 32 >> 2] = 0;
 HEAP32[i5 + 36 >> 2] = 0;
 HEAP32[i5 + 40 >> 2] = 0;
 i1 = _deflateInit_(i5, -1, 14249, 56) | 0;
 if (i1) {
  i6 = i1;
  STACKTOP = i5;
  return i6 | 0;
 }
 i1 = _deflate(i5, 4) | 0;
 if ((i1 | 0) == 1) {
  HEAP32[i2 >> 2] = HEAP32[i5 + 20 >> 2];
  i6 = _deflateEnd(i5) | 0;
  STACKTOP = i5;
  return i6 | 0;
 } else {
  _deflateEnd(i5) | 0;
  i6 = (i1 | 0) == 0 ? -5 : i1;
  STACKTOP = i5;
  return i6 | 0;
 }
 return 0;
}

function ___remdi3(i1, i2, i3, i4) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 var i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0, i10 = 0;
 i5 = STACKTOP;
 STACKTOP = STACKTOP + 16 | 0;
 i6 = i2 >> 31 | ((i2 | 0) < 0 ? -1 : 0) << 1;
 i7 = ((i2 | 0) < 0 ? -1 : 0) >> 31 | ((i2 | 0) < 0 ? -1 : 0) << 1;
 i8 = i4 >> 31 | ((i4 | 0) < 0 ? -1 : 0) << 1;
 i9 = ((i4 | 0) < 0 ? -1 : 0) >> 31 | ((i4 | 0) < 0 ? -1 : 0) << 1;
 i10 = _i64Subtract(i6 ^ i1, i7 ^ i2, i6, i7) | 0;
 i2 = tempRet0;
 ___udivmoddi4(i10, i2, _i64Subtract(i8 ^ i3, i9 ^ i4, i8, i9) | 0, tempRet0, i5 | 0) | 0;
 i9 = _i64Subtract(HEAP32[i5 >> 2] ^ i6, HEAP32[i5 + 4 >> 2] ^ i7, i6, i7) | 0;
 i7 = tempRet0;
 STACKTOP = i5;
 return (tempRet0 = i7, i9) | 0;
}

function ___overflow(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0;
 i3 = STACKTOP;
 STACKTOP = STACKTOP + 16 | 0;
 HEAP8[i3 >> 0] = i2;
 i4 = HEAP32[i1 + 16 >> 2] | 0;
 if (!i4) if (!(___towrite(i1) | 0)) {
  i5 = HEAP32[i1 + 16 >> 2] | 0;
  i6 = 4;
 } else i7 = -1; else {
  i5 = i4;
  i6 = 4;
 }
 do if ((i6 | 0) == 4) {
  i4 = HEAP32[i1 + 20 >> 2] | 0;
  if (i4 >>> 0 < i5 >>> 0 ? (i2 & 255 | 0) != (HEAP8[i1 + 75 >> 0] | 0) : 0) {
   HEAP32[i1 + 20 >> 2] = i4 + 1;
   HEAP8[i4 >> 0] = i2;
   i7 = i2 & 255;
   break;
  }
  if ((FUNCTION_TABLE_iiii[HEAP32[i1 + 36 >> 2] & 7](i1, i3, 1) | 0) == 1) i7 = HEAPU8[i3 >> 0] | 0; else i7 = -1;
 } while (0);
 STACKTOP = i3;
 return i7 | 0;
}

function _strerror(i1) {
 i1 = i1 | 0;
 var i2 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0, i8 = 0;
 i2 = 0;
 while (1) {
  if ((HEAPU8[14833 + i2 >> 0] | 0) == (i1 | 0)) {
   i3 = i2;
   i4 = 2;
   break;
  }
  i2 = i2 + 1 | 0;
  if ((i2 | 0) == 87) {
   i5 = 87;
   i6 = 14921;
   i4 = 5;
   break;
  }
 }
 if ((i4 | 0) == 2) if (!i3) i7 = 14921; else {
  i5 = i3;
  i6 = 14921;
  i4 = 5;
 }
 if ((i4 | 0) == 5) while (1) {
  i4 = 0;
  i3 = i6;
  while (1) {
   i8 = i3 + 1 | 0;
   if (!(HEAP8[i3 >> 0] | 0)) break; else i3 = i8;
  }
  i5 = i5 + -1 | 0;
  if (!i5) {
   i7 = i8;
   break;
  } else {
   i6 = i8;
   i4 = 5;
  }
 }
 return i7 | 0;
}

function _memcpy(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 var i4 = 0;
 if ((i3 | 0) >= 4096) return _emscripten_memcpy_big(i1 | 0, i2 | 0, i3 | 0) | 0;
 i4 = i1 | 0;
 if ((i1 & 3) == (i2 & 3)) {
  while (i1 & 3) {
   if (!i3) return i4 | 0;
   HEAP8[i1 >> 0] = HEAP8[i2 >> 0] | 0;
   i1 = i1 + 1 | 0;
   i2 = i2 + 1 | 0;
   i3 = i3 - 1 | 0;
  }
  while ((i3 | 0) >= 4) {
   HEAP32[i1 >> 2] = HEAP32[i2 >> 2];
   i1 = i1 + 4 | 0;
   i2 = i2 + 4 | 0;
   i3 = i3 - 4 | 0;
  }
 }
 while ((i3 | 0) > 0) {
  HEAP8[i1 >> 0] = HEAP8[i2 >> 0] | 0;
  i1 = i1 + 1 | 0;
  i2 = i2 + 1 | 0;
  i3 = i3 - 1 | 0;
 }
 return i4 | 0;
}

function _main() {
 var i1 = 0, i2 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0;
 i1 = _malloc(1e5) | 0;
 i2 = 0;
 i3 = 0;
 i4 = 17;
 while (1) {
  do if ((i3 | 0) <= 0) if (!(i2 & 7)) {
   i5 = i2 & 31;
   i6 = 0;
   break;
  } else {
   i5 = i3;
   i6 = (((Math_imul(i2, i2) | 0) >>> 0) % 6714 | 0) & 255;
   break;
  } else {
   i5 = i3 + -1 | 0;
   i6 = i4;
  } while (0);
  HEAP8[i1 + i2 >> 0] = i6;
  i2 = i2 + 1 | 0;
  if ((i2 | 0) == 1e5) {
   i7 = 0;
   break;
  } else {
   i3 = i5;
   i4 = i6;
  }
 }
 do {
  _doit(i1, 1e5, i7);
  i7 = i7 + 1 | 0;
 } while ((i7 | 0) != 5e3);
 _puts(13458) | 0;
 return 0;
}

function _inflateEnd(i1) {
 i1 = i1 | 0;
 var i2 = 0, i3 = 0, i4 = 0, i5 = 0, i6 = 0, i7 = 0;
 if (!i1) {
  i2 = -2;
  return i2 | 0;
 }
 i3 = HEAP32[i1 + 28 >> 2] | 0;
 if (!i3) {
  i2 = -2;
  return i2 | 0;
 }
 i4 = HEAP32[i1 + 36 >> 2] | 0;
 if (!i4) {
  i2 = -2;
  return i2 | 0;
 }
 i5 = HEAP32[i3 + 52 >> 2] | 0;
 if (!i5) {
  i6 = i4;
  i7 = i3;
 } else {
  FUNCTION_TABLE_vii[i4 & 1](HEAP32[i1 + 40 >> 2] | 0, i5);
  i6 = HEAP32[i1 + 36 >> 2] | 0;
  i7 = HEAP32[i1 + 28 >> 2] | 0;
 }
 FUNCTION_TABLE_vii[i6 & 1](HEAP32[i1 + 40 >> 2] | 0, i7);
 HEAP32[i1 + 28 >> 2] = 0;
 i2 = 0;
 return i2 | 0;
}

function ___divdi3(i1, i2, i3, i4) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 var i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0;
 i5 = i2 >> 31 | ((i2 | 0) < 0 ? -1 : 0) << 1;
 i6 = ((i2 | 0) < 0 ? -1 : 0) >> 31 | ((i2 | 0) < 0 ? -1 : 0) << 1;
 i7 = i4 >> 31 | ((i4 | 0) < 0 ? -1 : 0) << 1;
 i8 = ((i4 | 0) < 0 ? -1 : 0) >> 31 | ((i4 | 0) < 0 ? -1 : 0) << 1;
 i9 = _i64Subtract(i5 ^ i1, i6 ^ i2, i5, i6) | 0;
 i2 = tempRet0;
 return _i64Subtract((___udivmoddi4(i9, i2, _i64Subtract(i7 ^ i3, i8 ^ i4, i7, i8) | 0, tempRet0, 0) | 0) ^ (i7 ^ i5), tempRet0 ^ (i8 ^ i6), i7 ^ i5, i8 ^ i6) | 0;
}

function ___fflush_unlocked(i1) {
 i1 = i1 | 0;
 var i2 = 0, i3 = 0, i4 = 0;
 if ((HEAP32[i1 + 20 >> 2] | 0) >>> 0 > (HEAP32[i1 + 28 >> 2] | 0) >>> 0 ? (FUNCTION_TABLE_iiii[HEAP32[i1 + 36 >> 2] & 7](i1, 0, 0) | 0, (HEAP32[i1 + 20 >> 2] | 0) == 0) : 0) i2 = -1; else {
  i3 = HEAP32[i1 + 4 >> 2] | 0;
  i4 = HEAP32[i1 + 8 >> 2] | 0;
  if (i3 >>> 0 < i4 >>> 0) FUNCTION_TABLE_iiii[HEAP32[i1 + 40 >> 2] & 7](i1, i3 - i4 | 0, 1) | 0;
  HEAP32[i1 + 16 >> 2] = 0;
  HEAP32[i1 + 28 >> 2] = 0;
  HEAP32[i1 + 20 >> 2] = 0;
  HEAP32[i1 + 8 >> 2] = 0;
  HEAP32[i1 + 4 >> 2] = 0;
  i2 = 0;
 }
 return i2 | 0;
}

function _puts(i1) {
 i1 = i1 | 0;
 var i2 = 0, i3 = 0, i4 = 0, i5 = 0;
 i2 = HEAP32[2255] | 0;
 if ((HEAP32[i2 + 76 >> 2] | 0) > -1) i3 = ___lockfile(i2) | 0; else i3 = 0;
 do if ((_fputs(i1, i2) | 0) < 0) i4 = 1; else {
  if ((HEAP8[i2 + 75 >> 0] | 0) != 10 ? (i5 = HEAP32[i2 + 20 >> 2] | 0, i5 >>> 0 < (HEAP32[i2 + 16 >> 2] | 0) >>> 0) : 0) {
   HEAP32[i2 + 20 >> 2] = i5 + 1;
   HEAP8[i5 >> 0] = 10;
   i4 = 0;
   break;
  }
  i4 = (___overflow(i2, 10) | 0) < 0;
 } while (0);
 if (i3) ___unlockfile(i2);
 return i4 << 31 >> 31 | 0;
}

function _memset(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 var i4 = 0, i5 = 0, i6 = 0;
 i4 = i1 + i3 | 0;
 if ((i3 | 0) >= 20) {
  i2 = i2 & 255;
  i5 = i1 & 3;
  i6 = i2 | i2 << 8 | i2 << 16 | i2 << 24;
  if (i5) {
   i5 = i1 + 4 - i5 | 0;
   while ((i1 | 0) < (i5 | 0)) {
    HEAP8[i1 >> 0] = i2;
    i1 = i1 + 1 | 0;
   }
  }
  while ((i1 | 0) < (i4 & ~3 | 0)) {
   HEAP32[i1 >> 2] = i6;
   i1 = i1 + 4 | 0;
  }
 }
 while ((i1 | 0) < (i4 | 0)) {
  HEAP8[i1 >> 0] = i2;
  i1 = i1 + 1 | 0;
 }
 return i1 - i3 | 0;
}

function _strcmp(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i3 = 0, i4 = 0, i5 = 0, i6 = 0;
 i3 = HEAP8[i1 >> 0] | 0;
 i4 = HEAP8[i2 >> 0] | 0;
 if (i3 << 24 >> 24 == 0 ? 1 : i3 << 24 >> 24 != i4 << 24 >> 24) {
  i5 = i3;
  i6 = i4;
 } else {
  i4 = i1;
  i1 = i2;
  do {
   i4 = i4 + 1 | 0;
   i1 = i1 + 1 | 0;
   i2 = HEAP8[i4 >> 0] | 0;
   i3 = HEAP8[i1 >> 0] | 0;
  } while (!(i2 << 24 >> 24 == 0 ? 1 : i2 << 24 >> 24 != i3 << 24 >> 24));
  i5 = i2;
  i6 = i3;
 }
 return (i5 & 255) - (i6 & 255) | 0;
}

function ___stdio_seek(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 var i4 = 0, i5 = 0;
 i4 = STACKTOP;
 STACKTOP = STACKTOP + 32 | 0;
 HEAP32[i4 >> 2] = HEAP32[i1 + 60 >> 2];
 HEAP32[i4 + 4 >> 2] = 0;
 HEAP32[i4 + 8 >> 2] = i2;
 HEAP32[i4 + 12 >> 2] = i4 + 20;
 HEAP32[i4 + 16 >> 2] = i3;
 if ((___syscall_ret(___syscall140(140, i4 | 0) | 0) | 0) < 0) {
  HEAP32[i4 + 20 >> 2] = -1;
  i5 = -1;
 } else i5 = HEAP32[i4 + 20 >> 2] | 0;
 STACKTOP = i4;
 return i5 | 0;
}

function _fwrite(i1, i2, i3, i4) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 var i5 = 0, i6 = 0, i7 = 0, i8 = 0, i9 = 0;
 i5 = Math_imul(i3, i2) | 0;
 if ((HEAP32[i4 + 76 >> 2] | 0) > -1) {
  i6 = (___lockfile(i4) | 0) == 0;
  i7 = ___fwritex(i1, i5, i4) | 0;
  if (i6) i8 = i7; else {
   ___unlockfile(i4);
   i8 = i7;
  }
 } else i8 = ___fwritex(i1, i5, i4) | 0;
 if ((i8 | 0) == (i5 | 0)) i9 = i3; else i9 = (i8 >>> 0) / (i2 >>> 0) | 0;
 return i9 | 0;
}

function ___towrite(i1) {
 i1 = i1 | 0;
 var i2 = 0, i3 = 0, i4 = 0;
 i2 = HEAP8[i1 + 74 >> 0] | 0;
 HEAP8[i1 + 74 >> 0] = i2 + 255 | i2;
 i2 = HEAP32[i1 >> 2] | 0;
 if (!(i2 & 8)) {
  HEAP32[i1 + 8 >> 2] = 0;
  HEAP32[i1 + 4 >> 2] = 0;
  i3 = HEAP32[i1 + 44 >> 2] | 0;
  HEAP32[i1 + 28 >> 2] = i3;
  HEAP32[i1 + 20 >> 2] = i3;
  HEAP32[i1 + 16 >> 2] = i3 + (HEAP32[i1 + 48 >> 2] | 0);
  i4 = 0;
 } else {
  HEAP32[i1 >> 2] = i2 | 32;
  i4 = -1;
 }
 return i4 | 0;
}

function copyTempDouble(i1) {
 i1 = i1 | 0;
 HEAP8[tempDoublePtr >> 0] = HEAP8[i1 >> 0];
 HEAP8[tempDoublePtr + 1 >> 0] = HEAP8[i1 + 1 >> 0];
 HEAP8[tempDoublePtr + 2 >> 0] = HEAP8[i1 + 2 >> 0];
 HEAP8[tempDoublePtr + 3 >> 0] = HEAP8[i1 + 3 >> 0];
 HEAP8[tempDoublePtr + 4 >> 0] = HEAP8[i1 + 4 >> 0];
 HEAP8[tempDoublePtr + 5 >> 0] = HEAP8[i1 + 5 >> 0];
 HEAP8[tempDoublePtr + 6 >> 0] = HEAP8[i1 + 6 >> 0];
 HEAP8[tempDoublePtr + 7 >> 0] = HEAP8[i1 + 7 >> 0];
}

function ___stdout_write(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 var i4 = 0, i5 = 0;
 i4 = STACKTOP;
 STACKTOP = STACKTOP + 80 | 0;
 HEAP32[i1 + 36 >> 2] = 4;
 if ((HEAP32[i1 >> 2] & 64 | 0) == 0 ? (HEAP32[i4 >> 2] = HEAP32[i1 + 60 >> 2], HEAP32[i4 + 4 >> 2] = 21505, HEAP32[i4 + 8 >> 2] = i4 + 12, (___syscall54(54, i4 | 0) | 0) != 0) : 0) HEAP8[i1 + 75 >> 0] = -1;
 i5 = ___stdio_write(i1, i2, i3) | 0;
 STACKTOP = i4;
 return i5 | 0;
}

function ___muldsi3(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i3 = 0, i4 = 0, i5 = 0;
 i3 = Math_imul(i2 & 65535, i1 & 65535) | 0;
 i4 = (i3 >>> 16) + (Math_imul(i2 & 65535, i1 >>> 16) | 0) | 0;
 i5 = Math_imul(i2 >>> 16, i1 & 65535) | 0;
 return (tempRet0 = (i4 >>> 16) + (Math_imul(i2 >>> 16, i1 >>> 16) | 0) + (((i4 & 65535) + i5 | 0) >>> 16) | 0, i4 + i5 << 16 | i3 & 65535 | 0) | 0;
}

function _llvm_cttz_i32(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = HEAP8[cttz_i8 + (i1 & 255) >> 0] | 0;
 if ((i2 | 0) < 8) return i2 | 0;
 i2 = HEAP8[cttz_i8 + (i1 >> 8 & 255) >> 0] | 0;
 if ((i2 | 0) < 8) return i2 + 8 | 0;
 i2 = HEAP8[cttz_i8 + (i1 >> 16 & 255) >> 0] | 0;
 if ((i2 | 0) < 8) return i2 + 16 | 0;
 return (HEAP8[cttz_i8 + (i1 >>> 24) >> 0] | 0) + 24 | 0;
}

function _init_mparams() {
 var i1 = 0;
 do if (!(HEAP32[2375] | 0)) {
  i1 = _sysconf(30) | 0;
  if (!(i1 + -1 & i1)) {
   HEAP32[2377] = i1;
   HEAP32[2376] = i1;
   HEAP32[2378] = -1;
   HEAP32[2379] = -1;
   HEAP32[2380] = 0;
   HEAP32[2368] = 0;
   HEAP32[2375] = (_time(0) | 0) & -16 ^ 1431655768;
   break;
  } else _abort();
 } while (0);
 return;
}

function __tr_init(i1) {
 i1 = i1 | 0;
 HEAP32[i1 + 2840 >> 2] = i1 + 148;
 HEAP32[i1 + 2848 >> 2] = 136;
 HEAP32[i1 + 2852 >> 2] = i1 + 2440;
 HEAP32[i1 + 2860 >> 2] = 156;
 HEAP32[i1 + 2864 >> 2] = i1 + 2684;
 HEAP32[i1 + 2872 >> 2] = 176;
 HEAP16[i1 + 5816 >> 1] = 0;
 HEAP32[i1 + 5820 >> 2] = 0;
 HEAP32[i1 + 5812 >> 2] = 8;
 _init_block(i1);
 return;
}

function ___uremdi3(i1, i2, i3, i4) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 var i5 = 0;
 i5 = STACKTOP;
 STACKTOP = STACKTOP + 16 | 0;
 ___udivmoddi4(i1, i2, i3, i4, i5 | 0) | 0;
 STACKTOP = i5;
 return (tempRet0 = HEAP32[i5 + 4 >> 2] | 0, HEAP32[i5 >> 2] | 0) | 0;
}

function ___muldi3(i1, i2, i3, i4) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 var i5 = 0, i6 = 0;
 i5 = ___muldsi3(i1, i3) | 0;
 i6 = tempRet0;
 return (tempRet0 = (Math_imul(i2, i3) | 0) + (Math_imul(i4, i1) | 0) + i6 | i6 & 0, i5 | 0 | 0) | 0;
}

function runPostSets() {}
function _i64Subtract(i1, i2, i3, i4) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 var i5 = 0;
 i5 = i2 - i4 >>> 0;
 i5 = i2 - i4 - (i3 >>> 0 > i1 >>> 0 | 0) >>> 0;
 return (tempRet0 = i5, i1 - i3 >>> 0 | 0) | 0;
}

function copyTempFloat(i1) {
 i1 = i1 | 0;
 HEAP8[tempDoublePtr >> 0] = HEAP8[i1 >> 0];
 HEAP8[tempDoublePtr + 1 >> 0] = HEAP8[i1 + 1 >> 0];
 HEAP8[tempDoublePtr + 2 >> 0] = HEAP8[i1 + 2 >> 0];
 HEAP8[tempDoublePtr + 3 >> 0] = HEAP8[i1 + 3 >> 0];
}

function _bitshift64Ashr(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 if ((i3 | 0) < 32) {
  tempRet0 = i2 >> i3;
  return i1 >>> i3 | (i2 & (1 << i3) - 1) << 32 - i3;
 }
 tempRet0 = (i2 | 0) < 0 ? -1 : 0;
 return i2 >> i3 - 32 | 0;
}

function _bitshift64Shl(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 if ((i3 | 0) < 32) {
  tempRet0 = i2 << i3 | (i1 & (1 << i3) - 1 << 32 - i3) >>> 32 - i3;
  return i1 << i3;
 }
 tempRet0 = i1 << i3 - 32;
 return 0;
}

function ___stdio_close(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 STACKTOP = STACKTOP + 16 | 0;
 HEAP32[i2 >> 2] = HEAP32[i1 + 60 >> 2];
 i1 = ___syscall_ret(___syscall6(6, i2 | 0) | 0) | 0;
 STACKTOP = i2;
 return i1 | 0;
}

function _bitshift64Lshr(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 if ((i3 | 0) < 32) {
  tempRet0 = i2 >>> i3;
  return i1 >>> i3 | (i2 & (1 << i3) - 1) << 32 - i3;
 }
 tempRet0 = 0;
 return i2 >>> i3 - 32 | 0;
}

function _printf(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i3 = 0;
 i3 = STACKTOP;
 STACKTOP = STACKTOP + 16 | 0;
 HEAP32[i3 >> 2] = i2;
 i2 = _vfprintf(HEAP32[2255] | 0, i1, i3) | 0;
 STACKTOP = i3;
 return i2 | 0;
}

function _i64Add(i1, i2, i3, i4) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 return (tempRet0 = i2 + i4 + (i1 + i3 >>> 0 >>> 0 < i1 >>> 0 | 0) >>> 0, i1 + i3 >>> 0 | 0) | 0;
}

function ___syscall_ret(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 if (i1 >>> 0 > 4294963200) {
  HEAP32[(___errno_location() | 0) >> 2] = 0 - i1;
  i2 = -1;
 } else i2 = i1;
 return i2 | 0;
}

function dynCall_iiii(i1, i2, i3, i4) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 return FUNCTION_TABLE_iiii[i1 & 7](i2 | 0, i3 | 0, i4 | 0) | 0;
}

function _deflateInit_(i1, i2, i3, i4) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 return _deflateInit2_(i1, i2, 8, 15, 8, 0, i3, i4) | 0;
}

function ___errno_location() {
 var i1 = 0;
 if (!(HEAP32[2244] | 0)) i1 = 9524; else i1 = HEAP32[(_pthread_self() | 0) + 60 >> 2] | 0;
 return i1 | 0;
}
function stackAlloc(i1) {
 i1 = i1 | 0;
 var i2 = 0;
 i2 = STACKTOP;
 STACKTOP = STACKTOP + i1 | 0;
 STACKTOP = STACKTOP + 15 & -16;
 return i2 | 0;
}

function ___udivdi3(i1, i2, i3, i4) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 i4 = i4 | 0;
 return ___udivmoddi4(i1, i2, i3, i4, 0) | 0;
}

function _wctomb(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 var i3 = 0;
 if (!i1) i3 = 0; else i3 = _wcrtomb(i1, i2, 0) | 0;
 return i3 | 0;
}

function _llvm_bswap_i32(i1) {
 i1 = i1 | 0;
 return (i1 & 255) << 24 | (i1 >> 8 & 255) << 16 | (i1 >> 16 & 255) << 8 | i1 >>> 24 | 0;
}

function dynCall_iii(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 return FUNCTION_TABLE_iii[i1 & 3](i2 | 0, i3 | 0) | 0;
}

function dynCall_vii(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 FUNCTION_TABLE_vii[i1 & 1](i2 | 0, i3 | 0);
}

function _zcalloc(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 return _malloc(Math_imul(i3, i2) | 0) | 0;
}

function setThrew(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 if (!__THREW__) {
  __THREW__ = i1;
  threwValue = i2;
 }
}

function _fputs(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 return (_fwrite(i1, _strlen(i1) | 0, 1, i2) | 0) + -1 | 0;
}

function _compressBound(i1) {
 i1 = i1 | 0;
 return i1 + 13 + (i1 >>> 12) + (i1 >>> 14) + (i1 >>> 25) | 0;
}

function dynCall_ii(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 return FUNCTION_TABLE_ii[i1 & 1](i2 | 0) | 0;
}

function _cleanup505(i1) {
 i1 = i1 | 0;
 if (!(HEAP32[i1 + 68 >> 2] | 0)) ___unlockfile(i1);
 return;
}

function establishStackSpace(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 STACKTOP = i1;
 STACK_MAX = i2;
}

function dynCall_vi(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 FUNCTION_TABLE_vi[i1 & 1](i2 | 0);
}

function b1(i1, i2, i3) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 i3 = i3 | 0;
 abort(1);
 return 0;
}

function _frexpl(d1, i2) {
 d1 = +d1;
 i2 = i2 | 0;
 return +(+_frexp(d1, i2));
}

function _zcfree(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 _free(i2);
 return;
}

function b3(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 abort(3);
 return 0;
}

function b2(i1, i2) {
 i1 = i1 | 0;
 i2 = i2 | 0;
 abort(2);
}

function stackRestore(i1) {
 i1 = i1 | 0;
 STACKTOP = i1;
}

function setTempRet0(i1) {
 i1 = i1 | 0;
 tempRet0 = i1;
}

function b0(i1) {
 i1 = i1 | 0;
 abort(0);
 return 0;
}

function ___unlockfile(i1) {
 i1 = i1 | 0;
 return;
}

function ___lockfile(i1) {
 i1 = i1 | 0;
 return 0;
}

function getTempRet0() {
 return tempRet0 | 0;
}

function stackSave() {
 return STACKTOP | 0;
}

function b4(i1) {
 i1 = i1 | 0;
 abort(4);
}

// EMSCRIPTEN_END_FUNCS
var FUNCTION_TABLE_ii = [b0,___stdio_close];
var FUNCTION_TABLE_iiii = [b1,___stdout_write,___stdio_seek,_zcalloc,___stdio_write,b1,b1,b1];
var FUNCTION_TABLE_vii = [b2,_zcfree];
var FUNCTION_TABLE_iii = [b3,_deflate_stored,_deflate_fast,_deflate_slow];
var FUNCTION_TABLE_vi = [b4,_cleanup505];

  return { _i64Subtract: _i64Subtract, _fflush: _fflush, _main: _main, _i64Add: _i64Add, _memset: _memset, _malloc: _malloc, _memcpy: _memcpy, _llvm_bswap_i32: _llvm_bswap_i32, _bitshift64Lshr: _bitshift64Lshr, _free: _free, ___errno_location: ___errno_location, _bitshift64Shl: _bitshift64Shl, runPostSets: runPostSets, stackAlloc: stackAlloc, stackSave: stackSave, stackRestore: stackRestore, establishStackSpace: establishStackSpace, setThrew: setThrew, setTempRet0: setTempRet0, getTempRet0: getTempRet0, dynCall_ii: dynCall_ii, dynCall_iiii: dynCall_iiii, dynCall_vii: dynCall_vii, dynCall_iii: dynCall_iii, dynCall_vi: dynCall_vi };
})
// EMSCRIPTEN_END_ASM
(Module.asmGlobalArg, Module.asmLibraryArg, buffer);
var _i64Subtract = Module["_i64Subtract"] = asm["_i64Subtract"];
var _fflush = Module["_fflush"] = asm["_fflush"];
var _main = Module["_main"] = asm["_main"];
var _i64Add = Module["_i64Add"] = asm["_i64Add"];
var _memset = Module["_memset"] = asm["_memset"];
var runPostSets = Module["runPostSets"] = asm["runPostSets"];
var _malloc = Module["_malloc"] = asm["_malloc"];
var _memcpy = Module["_memcpy"] = asm["_memcpy"];
var ___errno_location = Module["___errno_location"] = asm["___errno_location"];
var _bitshift64Lshr = Module["_bitshift64Lshr"] = asm["_bitshift64Lshr"];
var _free = Module["_free"] = asm["_free"];
var _llvm_bswap_i32 = Module["_llvm_bswap_i32"] = asm["_llvm_bswap_i32"];
var _bitshift64Shl = Module["_bitshift64Shl"] = asm["_bitshift64Shl"];
var dynCall_ii = Module["dynCall_ii"] = asm["dynCall_ii"];
var dynCall_iiii = Module["dynCall_iiii"] = asm["dynCall_iiii"];
var dynCall_vii = Module["dynCall_vii"] = asm["dynCall_vii"];
var dynCall_iii = Module["dynCall_iii"] = asm["dynCall_iii"];
var dynCall_vi = Module["dynCall_vi"] = asm["dynCall_vi"];
Runtime.stackAlloc = asm["stackAlloc"];
Runtime.stackSave = asm["stackSave"];
Runtime.stackRestore = asm["stackRestore"];
Runtime.establishStackSpace = asm["establishStackSpace"];
Runtime.setTempRet0 = asm["setTempRet0"];
Runtime.getTempRet0 = asm["getTempRet0"];
if (memoryInitializer) {
 if (typeof Module["locateFile"] === "function") {
  memoryInitializer = Module["locateFile"](memoryInitializer);
 } else if (Module["memoryInitializerPrefixURL"]) {
  memoryInitializer = Module["memoryInitializerPrefixURL"] + memoryInitializer;
 }
 if (ENVIRONMENT_IS_NODE || ENVIRONMENT_IS_SHELL) {
  var data = Module["readBinary"](memoryInitializer);
  HEAPU8.set(data, Runtime.GLOBAL_BASE);
 } else {
  addRunDependency("memory initializer");
  var applyMemoryInitializer = (function(data) {
   if (data.byteLength) data = new Uint8Array(data);
   HEAPU8.set(data, Runtime.GLOBAL_BASE);
   removeRunDependency("memory initializer");
  });
  function doBrowserLoad() {
   Browser.asyncLoad(memoryInitializer, applyMemoryInitializer, (function() {
    throw "could not load memory initializer " + memoryInitializer;
   }));
  }
  var request = Module["memoryInitializerRequest"];
  if (request) {
   function useRequest() {
    if (request.status !== 200 && request.status !== 0) {
     console.warn("a problem seems to have happened with Module.memoryInitializerRequest, status: " + request.status + ", retrying " + memoryInitializer);
     doBrowserLoad();
     return;
    }
    applyMemoryInitializer(request.response);
   }
   if (request.response) {
    setTimeout(useRequest, 0);
   } else {
    request.addEventListener("load", useRequest);
   }
  } else {
   doBrowserLoad();
  }
 }
}
function ExitStatus(status) {
 this.name = "ExitStatus";
 this.message = "Program terminated with exit(" + status + ")";
 this.status = status;
}
ExitStatus.prototype = new Error;
ExitStatus.prototype.constructor = ExitStatus;
var initialStackTop;
var preloadStartTime = null;
var calledMain = false;
dependenciesFulfilled = function runCaller() {
 if (!Module["calledRun"]) run();
 if (!Module["calledRun"]) dependenciesFulfilled = runCaller;
};
Module["callMain"] = Module.callMain = function callMain(args) {
 assert(runDependencies == 0, "cannot call main when async dependencies remain! (listen on __ATMAIN__)");
 assert(__ATPRERUN__.length == 0, "cannot call main when preRun functions remain to be called");
 args = args || [];
 ensureInitRuntime();
 var argc = args.length + 1;
 function pad() {
  for (var i = 0; i < 4 - 1; i++) {
   argv.push(0);
  }
 }
 var argv = [ allocate(intArrayFromString(Module["thisProgram"]), "i8", ALLOC_NORMAL) ];
 pad();
 for (var i = 0; i < argc - 1; i = i + 1) {
  argv.push(allocate(intArrayFromString(args[i]), "i8", ALLOC_NORMAL));
  pad();
 }
 argv.push(0);
 argv = allocate(argv, "i32", ALLOC_NORMAL);
 initialStackTop = Runtime.stackSave();
 try {
  var ret = Module["_main"](argc, argv, 0);
  exit(ret, true);
 } catch (e) {
  if (e instanceof ExitStatus) {
   return;
  } else if (e == "SimulateInfiniteLoop") {
   Module["noExitRuntime"] = true;
   Runtime.stackRestore(initialStackTop);
   return;
  } else {
   if (e && typeof e === "object" && e.stack) Module.printErr("exception thrown: " + [ e, e.stack ]);
   throw e;
  }
 } finally {
  calledMain = true;
 }
};
function run(args) {
 args = args || Module["arguments"];
 if (preloadStartTime === null) preloadStartTime = Date.now();
 if (runDependencies > 0) {
  return;
 }
 preRun();
 if (runDependencies > 0) return;
 if (Module["calledRun"]) return;
 function doRun() {
  if (Module["calledRun"]) return;
  Module["calledRun"] = true;
  if (ABORT) return;
  ensureInitRuntime();
  preMain();
  if (Module["onRuntimeInitialized"]) Module["onRuntimeInitialized"]();
  if (Module["_main"] && shouldRunNow) Module["callMain"](args);
  postRun();
 }
 if (Module["setStatus"]) {
  Module["setStatus"]("Running...");
  setTimeout((function() {
   setTimeout((function() {
    Module["setStatus"]("");
   }), 1);
   doRun();
  }), 1);
 } else {
  doRun();
 }
}
Module["run"] = Module.run = run;
function exit(status, implicit) {
 if (implicit && Module["noExitRuntime"]) {
  return;
 }
 if (Module["noExitRuntime"]) {} else {
  ABORT = true;
  EXITSTATUS = status;
  STACKTOP = initialStackTop;
  exitRuntime();
  if (Module["onExit"]) Module["onExit"](status);
 }
 if (ENVIRONMENT_IS_NODE) {
  process["stdout"]["once"]("drain", (function() {
   process["exit"](status);
  }));
  console.log(" ");
  setTimeout((function() {
   process["exit"](status);
  }), 500);
 } else if (ENVIRONMENT_IS_SHELL && typeof quit === "function") {
  quit(status);
 }
 throw new ExitStatus(status);
}
Module["exit"] = Module.exit = exit;
var abortDecorators = [];
function abort(what) {
 if (what !== undefined) {
  Module.print(what);
  Module.printErr(what);
  what = JSON.stringify(what);
 } else {
  what = "";
 }
 ABORT = true;
 EXITSTATUS = 1;
 var extra = "\nIf this abort() is unexpected, build with -s ASSERTIONS=1 which can give more information.";
 var output = "abort(" + what + ") at " + stackTrace() + extra;
 if (abortDecorators) {
  abortDecorators.forEach((function(decorator) {
   output = decorator(output, what);
  }));
 }
 throw output;
}
Module["abort"] = Module.abort = abort;
if (Module["preInit"]) {
 if (typeof Module["preInit"] == "function") Module["preInit"] = [ Module["preInit"] ];
 while (Module["preInit"].length > 0) {
  Module["preInit"].pop()();
 }
}
var shouldRunNow = true;
if (Module["noInitialRun"]) {
 shouldRunNow = false;
}
run();




