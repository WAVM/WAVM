(function(global,env,buffer){'use asm';
var u8Mem=new Uint8Array(buffer);
var u16Mem=new Uint16Array(buffer);
var u32Mem=new Uint32Array(buffer);
var s8Mem=new Int8Array(buffer);
var s16Mem=new Int16Array(buffer);
var s32Mem=new Int32Array(buffer);
var f32Mem=new Float32Array(buffer);
var f64Mem=new Float64Array(buffer);
var f32=global.Math.fround;
function _eq_float32(x,y){
x=f32(x);
y=f32(y);
if(0){return 0;}
return (x==y);
}
function _eq_float64(x,y){
x=+x;
y=+y;
if(0){return 0;}
return (x==y);
}
function _div_float32(x,y){
x=f32(x);
y=f32(y);
if(0){return f32(0);}
return (x/y);
}
function _div_float64(x,y){
x=+x;
y=+y;
if(0){return 0.0;}
return (x/y);
}
;})