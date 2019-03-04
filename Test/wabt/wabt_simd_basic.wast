;; Tests from wabt: https://github.com/WebAssembly/wabt/tree/master/test/interp
;; Distributed under the terms of the wabt license: https://github.com/WebAssembly/wabt/blob/master/LICENSE
;; Modified for compatibility with WAVM's interpretation of the proposed spec.

(module
  (func (export "main") (result v128)
    v128.const i32x4 0x00000001 0x00000002 0x00000003 0x00000004
    return))
	
(assert_return (invoke "main") (v128.const i32x4 0x00000001 0x00000002 0x00000003 0x00000004))