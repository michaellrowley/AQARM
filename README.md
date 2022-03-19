<img src="https://i.ibb.co/y8LF3Ym/AQArm.png" style="background-color: white;vertical-align: center;width: 15%;float: left;top:0;margin-top: 0;"/> <img src="https://i.ibb.co/ggDLMJd/Screenshot-2022-03-19-at-21-28-07.png" style="margin-top:-40px;margin-right: -40px;vertical-align: center;width: 35%;float: right;"/>
<p style="float:right;">AQArm is an interpreter for the AQA version of the Arm instruction set, it was developed as a way for me to learn more about interpreters, low-level languages, and my A-Level computer science syllabus.</p>

### Breakdown
The interpreter itself is very basic and the hardest part of developing this program was designing it so that the modular nature of the codebase would allow simple addition of the AQA Arm instruction set.
The instruction set from AQA can be found [here](https://filestore.aqa.org.uk/resources/computing/AQA-75162-75172-ALI.PDF), AQArm follows the instruction set to the best of its ability and I am not aware of any deviation from the exam specification - comments, labels, branches, and all of the listed opcodes are supported.

### UI
The text area above the code area is a 'command line' (in the loosest sense due to its simplicity) which is used to setup the 'application' memory before running the user-provided code (useful in instances where a question specifies that values are stored in memory locations before execution) using the command ``SETMEM/XXXX/YYYY`` ``XXXX`` refers to the memory address having a value written to it and ``YYYY`` refers to the value being assigned to the memory address.
The right half of the window shows the registers after execution (0 through 12) but it does not currently show carry/comparison data.

### Example
```ARM
MOV R0, #20
STR R0, 19900
MOV R1, #0
  LOOP_START:
SUB R0, R0, #1
ADD R1, R1, #1
CMP R0, R1
BNE LOOP_START
HALT
```
