.model flat
.code

_getStackPointer PROC
	lea eax, [esp+8]
	ret
_getStackPointer ENDP

End
