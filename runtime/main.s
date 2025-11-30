.global _start
.extern axen_main

_start:
	call main
	mov  %eax, %edi
	mov  $60, %eax
	syscall

