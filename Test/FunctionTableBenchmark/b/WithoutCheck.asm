loop in main

00000087B4940100  lea         eax,[rsi+rbp]  
00000087B4940103  mov         ecx,dword ptr [rbx+rax]  
00000087B4940106  and         ecx,3  
00000087B4940109  call        qword ptr [r12+rcx*8]  
00000087B494010D  add         edi,eax  
00000087B494010F  add         rbp,4  
00000087B4940113  cmp         rbp,9C40h  


function a

00000087B4940000  mov         eax,1  
00000087B4940005  ret  

function b

00000087B4940006  nop         word ptr cs:[rax+rax]  
00000087B4940010  mov         eax,0FFFFFFFFh  
00000087B4940015  ret  