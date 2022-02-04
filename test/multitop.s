	.global main
	.global _exit

_exit:
	b	_exit
	
main:
	stmfd sp!, {r0-r2}
	mov sp, r0
	bl subprog
	ldmfd sp!, {r0-r2}
	mov	pc, lr

subprog:
	stmfd sp!, {r0-r5}
	mov	r0, #0
	mov r1, #0
loop:
	cmp	r0, #100
	bhs	end_loop
	add	r1, r1, r0
	add	r0, r0, #1
	b	loop
end_loop:
	ldmfd sp!, {r0-r5}
	mov	pc, lr
