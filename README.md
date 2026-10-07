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

| Index | Register  | SIze | Purpose                             | Encoding |
| ----- | --------- | ---- | ----------------------------------- | -------- |
| 0     | GPIO_CFG  | 32   | Mode(3)+packet size(5)+N(16)        |          |
| 1     | GPIO_STAT | 32   | 8x4(posedge,negedge,level,ctr_zero) |          |
| 2     | CRC_ACC   |      | CRC Shift accumulator               |          |
| 3     | CRC_POLY  |      | Generator Polynomial                |          |
| 4     | CRC_SEED  |      | CRC Initial Seed                    |          |
| 5     | FLAG      |      | Flag register internal (ALU flags)  |          |
| 6     | FIFO_BUFF |      | FIFO access (read and write)        |          |

## Instructions

| Index | Instruction | OPCODE | Encoding |
| ----- | ----------- | ------ | -------- |
| 0     | DELAY       |        |          |
| 1     | SET_MODE    |        |          |
| 2     | JMP         |        |          |
| 3     | JNZ         |        |          |
| 4     | JZ          |        |          |
| 5     | WAIT        |        |          |
| 6     | ALU         |        |          |
| 7     | LOAD        |        |          |
| 8     | STORE       |        |          |
| 9     | CRC_FEED    |        |          |
| 10    | CRC_RST     |        |          |
| 11    | IN_FIFO     |        |          |
| 12    | OUT_FIFO    |        |          |
| 13    | MOV_SFR     |        |          |
