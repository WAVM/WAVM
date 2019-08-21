.code

getStackPointer PROC
	lea rax, [rsp+8]
	ret
getStackPointer ENDP

End
