loop in main

0000000F6C0F0100  lea         eax,[rdi+rbp]  
0000000F6C0F0103  mov         edx,dword ptr [rbx+rax]  
0000000F6C0F0106  and         edx,3  
0000000F6C0F0109  xor         ecx,ecx  
0000000F6C0F010B  call        qword ptr [r12+rdx*8]  
0000000F6C0F010F  add         esi,eax  
0000000F6C0F0111  add         rbp,4  
0000000F6C0F0115  cmp         rbp,9C40h  


function a

0000000F6C0F0000  test        ecx,ecx  
0000000F6C0F0002  jne         0000000F6C0F000A  
0000000F6C0F0004  mov         eax,1  
0000000F6C0F0009  ret  


function b

0000000F6C0F0010  test        ecx,ecx  
0000000F6C0F0012  jne         0000000F6C0F001A  
0000000F6C0F0014  mov         eax,0FFFFFFFFh  
0000000F6C0F0019  ret  

