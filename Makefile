all: asm iss mult mult_table

asm: asm.c
	gcc -Wall asm.c -o asm
iss: iss.c
	gcc -Wall iss.c -o iss
mult: mult.c
	gcc -Wall mult.c -o mult
mult_table: mult_table.c
	gcc -Wall mult_table.c -o mult_table
