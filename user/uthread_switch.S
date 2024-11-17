.text
.globl thread_switch
thread_switch:
  /* Save callee-saved registers */
  sd x1,   0(a0)    # Save ra
  sd x2,   8(a0)    # Save sp
  sd x8,  16(a0)    # Save s0/fp
  sd x9,  24(a0)    # Save s1
  sd x18, 32(a0)    # Save s2
  sd x19, 40(a0)    # Save s3
  sd x20, 48(a0)    # Save s4
  sd x21, 56(a0)    # Save s5
  sd x22, 64(a0)    # Save s6
  sd x23, 72(a0)    # Save s7
  sd x24, 80(a0)    # Save s8
  sd x25, 88(a0)    # Save s9
  sd x26, 96(a0)    # Save s10
  sd x27, 104(a0)   # Save s11

  /* Restore callee-saved registers */
  ld x1,   0(a1)    # Restore ra
  ld x2,   8(a1)    # Restore sp
  ld x8,  16(a1)    # Restore s0/fp
  ld x9,  24(a1)    # Restore s1
  ld x18, 32(a1)    # Restore s2
  ld x19, 40(a1)    # Restore s3
  ld x20, 48(a1)    # Restore s4
  ld x21, 56(a1)    # Restore s5
  ld x22, 64(a1)    # Restore s6
  ld x23, 72(a1)    # Restore s7
  ld x24, 80(a1)    # Restore s8
  ld x25, 88(a1)    # Restore s9
  ld x26, 96(a1)    # Restore s10
  ld x27, 104(a1)   # Restore s11

  ret               # Return to the new thread
