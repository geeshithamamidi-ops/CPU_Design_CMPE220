/* ============================================================================
 *  fact.c  —  8! (factorial of 8) = 40320
 *  CMPE 220 Team 02 — written in C, then hand-translated to fact.asm.
 *
 *  Iterative version: multiply 8 * 7 * 6 * ... * 2.
 *  (The *recursive* factorial is a separate program for the Dec 5 assignment.)
 *
 *  Expected return value: 40320  (0x9D80)
 * ==========================================================================*/
int main(void)
{
    int result = 1;                 /* R0 in the assembly translation      */

    for (int i = 8; i > 1; i--) {   /* i -> R1 ; loop bound 1 -> R3         */
        result = result * i;        /* ALU MUL                             */
    }

    return result;                  /* stored to DATA segment word 0x40    */
}
