	.global main
	.global _exit

_exit:
	b	_exit
	
main:
	stmfd sp!, {r0-r2}
	mov	r0, #0
	adr	r1, x
loop:
	cmp	r0, #10
	bhs	end
	ldr	r2, [r1]
	add	r2, r2, r0
	str	r2, [r1]
	add	r0, r0, #1
	b	loop
end:
	ldmfd sp!, {r0-r2}
	mov	pc, lr

x:
	.int	0


