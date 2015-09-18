loop in main

0000007D527400F0  lea         eax,[rbx+rbp]  
0000007D527400F3  mov         ecx,dword ptr [rdi+rax]  
0000007D527400F6  and         ecx,3  
0000007D527400F9  call        qword ptr [r14+rcx*8]  
0000007D527400FD  add         esi,eax  
0000007D527400FF  add         rbp,4  
0000007D52740103  cmp         rbp,9C40h  
0000007D5274010A  jne         0000007D527400F0  


function b

0000007D52740010  xor         eax,eax  
0000007D52740012  ret  