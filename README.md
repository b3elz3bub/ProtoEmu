# ProtoEmu
A General Purpose Protocol Emulator ASIC

Target Frequency of operation = 100MHz

## General Purpose registers

| Index | Register | Purpose      | Encoding |
| ----- | -------- | ------------ | -------- |
| 0     | R0       | Data Reg     |          |
| 1     | R1       | Shift Reg    |          |
| 2     | R2       | SRAM Pointer |          |
| 3     | R3       | Loop Counter |          |

## Special Function Registers

| Index | Register   | SIze | Purpose                                                                                                         | Encoding |
| ----- | ---------- | ---- | --------------------------------------------------------------------------------------------------------------- | -------- |
| 0     | GPIO_CFG   | 32   | Mode[2:0]+packet size-1[5:3]+N[21:6]<br>+RESERVED[31:22]                                                        |          |
| 1     | GPIO_STAT  | 32   | 8x4(posedge,negedge,level,ctr_zero)                                                                             |          |
| 2     | GPIO_FAULT | 32   | Fault_En[0]+SKEW_EN[1]+BIT_FLIP_EN[2]+<br>GLITCH_EN[3]+SKEW_DELAY[19:4]<br>+GLITCH_WIDTH[27:20]+Reserved[31:28] |          |
| 3     | CRC_ACC    |      | CRC Shift accumulator                                                                                           |          |
| 4     | CRC_POLY   |      | Generator Polynomial                                                                                            |          |
| 5     | CRC_SEED   |      | CRC Initial Seed                                                                                                |          |
| 6     | FLAG       |      | Flag register internal (ALU flags)                                                                              |          |
| 7     | FIFO_BUFF  |      | FIFO access (read and write)                                                                                    |          |

## Instructions

| Index | Instruction | OPCODE | Description             | Encoding |
| ----- | ----------- | ------ | ----------------------- | -------- |
| 0     | DELAY       |        |                         |          |
| 1     | SET_MODE    |        | GPIO[imm] <= GPIO_CFG   |          |
|       | SET_FAULT   |        | GPIO[imm] <= GPIO_FAULT |          |
| 2     | JMP         |        |                         |          |
| 3     | JNZ         |        |                         |          |
| 4     | JZ          |        |                         |          |
| 5     | WAIT        |        |                         |          |
| 6     | ALU         |        |                         |          |
| 7     | LOAD        |        |                         |          |
| 8     | STORE       |        |                         |          |
| 9     | CRC_FEED    |        |                         |          |
| 10    | CRC_RST     |        |                         |          |
| 11    | IN_FIFO     |        |                         |          |
| 12    | OUT_FIFO    |        |                         |          |
| 13    | MOV_SFR     |        |                         |          |
| 2     | SET_FAULT   |        |                         |          |

## GPIO Pin Modes

| Index | Mode      | Description                                                                                                                                                                          | Encoding |
| ----- | --------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | -------- |
| 0     | RELEASE   | High Impedance, External Driver                                                                                                                                                      | 000      |
| 1     | WAIT      | Output and Output enable are whatever kept previously and GPIO does nothing new                                                                                                      | 001      |
| 2     | SET0      | Drive the PIN to LOW                                                                                                                                                                 | 010      |
| 3     | SET1      | Drive the PIN to HIGH                                                                                                                                                                | 011      |
| 4     | SHIFT_IN  | Releases the PIN. On each tick it samples the synchronized pin into `osr`. After `pckt_len` ticks it pushes the zero-padded byte to the FIFO and starts the next word, back-to-back. | 100      |
| 5     | SHIFT_OUT | On a word boundary it pops the FIFO and drives bit `pckt_len-1`. The next ticks drive the remaining bits down to bit 0, so a word takes `pckt_len` ticks with no gaps.               | 101      |
| 6     | CLK_GEN   | Toggles on each tick. The half period is N+1, so the frequency is F/(2(N+1)). The initial level is whatever `pin_out` already was.                                                   | 110      |
| 7     | BAUD_GEN  | The pin is high for exactly one clock every N+1 clocks.                                                                                                                              | 111      |
