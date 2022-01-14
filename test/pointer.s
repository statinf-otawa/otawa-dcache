	.global main
	.global _exit

_exit:
	b	_exit
	
main:
	stmfd sp!, {r0-r2}
	mov	r0, #0
	adr	r1, t
loop:
	cmp	r0, #10
	bhs	end
	str	r2, [r1], #4
	add	r0, r0, #1
	b	loop
end:
	ldmfd sp!, {r0-r2}
	mov	pc, lr

t:
	.fill 10, 4

