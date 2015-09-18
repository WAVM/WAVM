loop in main

0000006063AE0110  lea         eax,[rsi+rbp]  
0000006063AE0113  mov         eax,dword ptr [rbx+rax]  
0000006063AE0116  and         eax,3  
0000006063AE0119  mov         rcx,qword ptr [r13+rax*8]  
0000006063AE011E  cmp         dword ptr [rcx-4],0  
0000006063AE0122  je          0000006063AE0127  
0000006063AE0124  mov         rcx,r15  
0000006063AE0127  call        rcx  
0000006063AE0129  add         edi,eax  
0000006063AE012B  add         rbp,4  
0000006063AE012F  cmp         rbp,9C40h  
0000006063AE0136  jne         0000006063AE0110  


function a

0000006063AE0004  mov         eax,1  
0000006063AE0009  ret  

function b

0000006063AE0014  mov         eax,0FFFFFFFFh  
0000006063AE0019  ret  
