	.global main
	.global _exit

_exit:
	b	_exit
	
main:
	stmfd sp!, {r0-r2}
	adr	r0, t
	adr	r1, t + 40
loop:
	cmp	r0, r1
	bhs	end
	str	r2, [r0], #4
	b	loop
end:
	ldmfd sp!, {r0-r2}
	mov	pc, lr

t:
	.fill 10, 4

