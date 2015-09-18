loop in main

000000B1DD8F00F3  mov         edx,dword ptr [rdi+rax]  
000000B1DD8F00F6  and         edx,3  
000000B1DD8F00F9  xor         ecx,ecx  
000000B1DD8F00FB  call        qword ptr [r14+rdx*8]  
000000B1DD8F00FF  add         esi,eax  
000000B1DD8F0101  add         rbp,4  
000000B1DD8F0105  cmp         rbp,9C40h  
000000B1DD8F010C  jne         000000B1DD8F00F0  


function b

000000B1DD8F000E  xchg        ax,ax  
000000B1DD8F0010  test        ecx,ecx  
000000B1DD8F0012  jne         000000B1DD8F0017  
000000B1DD8F0014  xor         eax,eax  
000000B1DD8F0016  ret  
000000B1DD8F0017  ud2  