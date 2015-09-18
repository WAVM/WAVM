0000004E0D490100  lea         eax,[rbx+rbp]  
0000004E0D490103  mov         eax,dword ptr [rdi+rax]  
0000004E0D490106  and         eax,3  
0000004E0D490109  mov         rcx,qword ptr [r14+rax*8]  
0000004E0D49010D  cmp         dword ptr [rcx-4],0  
0000004E0D490111  je          0000004E0D490116  
0000004E0D490113  mov         rcx,r15  
0000004E0D490116  call        rcx  