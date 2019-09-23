; method std.kernel#Runtime.__srt_init_

; starting save state
push rbp
mov rbp, rsp
push r12
push r13
push r14
push r15
push r8
push r9
sub rsp, 160
mov qword [rbp-56], rcx
xor r12, r12
xor r13, r13
mov qword [rbp-72], 0
mov qword [rbp-64], 0
mov qword [rbp-80], 0
mov r14, qword [rcx+8]
mov r15, qword [rcx]
mov rcx, qword [rcx+16]
mov rcx, qword [rcx]
mov qword [rbp-64], rcx
mov rcx, qword [rcx]
test rcx, rcx
jne code_start
jmp init_addr_tbl
code_start:
; instr 0
L0:
mov rcx, 10
call 4679096
mov qword [rbp-80], rax
lea r12, [L0]
mov r13, 0
mov rcx, r15
mov ecx, qword [rcx+80]
cmp ecx, 0
jne .thread_check
mov rcx, r15
mov rdx, qword [rcx+16]
lea rdx, [rdx+16]
mov qword [rcx+16], rdx
mov rcx, qword [rbp-80]
call 4680648
nop
; instr 1
L1:
mov rcx, 31
call 4679096
mov qword [rbp-80], rax
lea r12, [L1]
mov r13, 1
mov rcx, r15
mov ecx, qword [rcx+80]
cmp ecx, 0
jne .thread_check
mov rcx, r15
mov rdx, qword [rcx+16]
lea rdx, [rdx+16]
mov qword [rcx+16], rdx
mov rcx, qword [rbp-80]
call 4680648
nop
; instr 2
L2:
mov rcx, r15
mov rax, qword [rcx+16]
mov rdx, qword [rcx+8]
sub rax, rdx
cmp rax, 2
jae L16
mov r13, 2
cmp r13, -1
je L17
mov rcx, r15
mov rax, qword [rcx+64]
imul r13, 8
add rax, r13
mov qword [rcx+72], rax
L17:
mov r13, 0
mov rcx, r15
call 4680196
mov r13, -1
jmp .thread_check
L16:
mov rcx, r15
mov rcx, qword [rcx+16]
lea rax, qword [rcx+8]
mov r12, qword [rax]
sub rcx, 16
lea rdx, qword [rcx+8]
mov qword [rbp-80], rdx
mov rcx, rax
call 4680740
mov rcx, qword [rbp-80]
mov rdx, r12
call 4680780
nop
; instr 3
L3:
nop
nop
; instr 4
L4:
nop
nop
; instr 5
L5:
nop
nop
; instr 6
L6:
nop
nop
; instr 7
L7:
L8:
pxor xmm0, xmm0
mov rcx, r14
add rcx, 24
movsd qword [rcx], xmm0
nop
; instr 9
L9:
mov rcx, r15
mov rcx, qword [rcx+24]
mov rax, rcx
mov rcx, r14
add rcx, 24
movsd xmm0, qword [rcx]
mov rcx, rax
movsd qword [rcx], xmm0
nop
; instr 10
L10:
mov r13, 10
cmp r13, -1
je L18
mov rcx, r15
mov rax, qword [rcx+64]
imul r13, 8
add rax, r13
mov qword [rcx+72], rax
L18:
mov r13, 0
jmp func_end
nop
mov r13, 10
cmp r13, -1
je L19
mov rcx, r15
mov rax, qword [rcx+64]
imul r13, 8
add rax, r13
mov qword [rcx+72], rax
L19:
mov r13, 0
jmp func_end
init_addr_tbl:
nop
; setting label values
mov rax, qword [rbp-64]
lea rcx, [L0]
mov qword [rax], rcx
add rax, 8
lea rcx, [L1]
mov qword [rax], rcx
add rax, 8
lea rcx, [L2]
mov qword [rax], rcx
add rax, 8
lea rcx, [L3]
mov qword [rax], rcx
add rax, 8
lea rcx, [L4]
mov qword [rax], rcx
add rax, 8
lea rcx, [L5]
mov qword [rax], rcx
add rax, 8
lea rcx, [L6]
mov qword [rax], rcx
add rax, 8
lea rcx, [L7]
mov qword [rax], rcx
add rax, 8
lea rcx, [L8]
mov qword [rax], rcx
add rax, 8
lea rcx, [L9]
mov qword [rax], rcx
add rax, 8
lea rcx, [L10]
mov qword [rax], rcx
nop
jmp code_start
func_end:
mov rcx, r15
call 4328758
mov rcx, r15
mov rdx, qword [rcx+72]
lea rdx, [rdx+8]
mov qword [rcx+72], rdx
add rsp, 160
pop r9
pop r8
pop r15
pop r14
pop r13
pop r12
pop rbp
ret
.thread_check:
mov rcx, r15
mov eax, qword [rcx+80]
sar eax, 2
and eax, 1
test eax, eax
je L20
call 4232772
L20:
mov rcx, r15
mov eax, qword [rcx+136]
cmp eax, 3
jne L21
short jmp func_end
L21:
mov rcx, r15
mov eax, qword [rcx+80]
sar eax, 1
and eax, 1
test eax, eax
je L22
cmp r13, -1
je L24
mov rcx, r15
mov rax, qword [rcx+64]
imul r13, 8
add rax, r13
mov qword [rcx+72], rax
L24:
mov r13, 0
mov rcx, qword [rcx+32]
call 4681616
cmp rax, 1
je L23
jmp func_end
L23:
mov rcx, r15
call 4681580
mov rdx, qword [rbp-64]
imul rax, 8
add rdx, rax
mov r12, [rdx]
jmp r12
L22:
jmp r12
nop
nop
nop
align 64
.data:
; data section start
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      ov r12, [rdx]
jmp r12
L28:
jmp r12
nop
nop
nop
align 64
.data:
; data section start
L20:
db 000000000000F03F
