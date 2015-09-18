loop in main

0000005130AD0100  lea         eax,[rbx+rbp]  
0000005130AD0103  mov         eax,dword ptr [rdi+rax]  
0000005130AD0106  and         eax,3  
0000005130AD0109  mov         rcx,qword ptr [r14+rax*8]  
0000005130AD010D  cmp         dword ptr [rcx-4],0  
0000005130AD0111  je          0000005130AD0116  
0000005130AD0113  mov         rcx,r15  
0000005130AD0116  call        rcx  
0000005130AD0118  add         esi,eax  
0000005130AD011A  add         rbp,4  
0000005130AD011E  cmp         rbp,9C40h  
0000005130AD0125  jne         0000005130AD0100  


function b

0000005130AD0014  xor         eax,eax  
0000005130AD0016  ret  