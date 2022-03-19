#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <map>
#include <QString>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    QObject::connect(ui->runCodeButton, &QPushButton::pressed, this, &MainWindow::RunCode);
    QObject::connect(ui->executeCommandButton, &QPushButton::pressed, this, &MainWindow::ExecCommand);
}

MainWindow::~MainWindow()
{
    delete ui;
}

struct Opcode {
    QString mnemonic;
    size_t operandCount;
    bool(*handlerFn)(const QStringList& operands);
};

struct MemoryByte {
    size_t address;
    uint8_t value;
};
std::vector<MemoryByte> preApplicationMemory(0);
std::vector<MemoryByte> applicationMemory(0);
uint8_t getMemoryAddressValue(size_t address) {
    for (const MemoryByte& iterativeMemByte : applicationMemory) {
        if (iterativeMemByte.address == address) {
            return iterativeMemByte.value;
        }
    }
    return 0;
}
void setMemoryAddressValue(size_t address, uint8_t value) {
    for (size_t i = 0; i < applicationMemory.size(); i++) {
        MemoryByte& iterativeMemByte = applicationMemory[i];
        if (iterativeMemByte.address == address) {
            if (value == 0) {
                applicationMemory.erase(applicationMemory.begin() + i);
                return;
            }
            iterativeMemByte.value = value;
            return;
        }
    }
    MemoryByte newMemByte = {.address = address, .value = value};
    applicationMemory.push_back(newMemByte);
    return;
}

enum comparisonResult {
    EQUAL_TO ,
    LESS_THAN,
    GRTR_THAN
};
enum specialRegisters {
    COMPARISON,
    MAX // DO NOT USE, MEMORY HASN'T BEEN ALLOCATED FOR IT!.
};
std::vector<uint8_t> specialRegisterValues(specialRegisters::MAX);
// "The available general purpose registers that the programmer can use are numbered
// 0 to 12."
std::vector<uint8_t> registerValues(13);
[[nodiscard]] bool getRegisterIndexFromStr(const QString& registerString, size_t& registerIndex) {
    if (!registerString.startsWith("R") || registerString.length() == 1) {
        return false;
    }
    bool validInteger = false;
    if ((registerIndex = registerString.mid(1, registerString.length() - 1).toUInt(&validInteger))> 12
            || !validInteger) {
return false;
    }
    return true;
}

uint8_t universalGetValue(const QString& operand) {
    if (operand.isEmpty()) {
        return 0; // This is a horrible way to account for errors, TODO: Improve/fix this.
    }
    // TODO: Use the 'ok' validity pointer for both of these cases, I have been
    // lazy and not used it.
    if (operand[0] == '#') {
        // Raw value
        return operand.mid(1, operand.length() - 1).toUInt() % 256;
    }
    else {
        // Register
        size_t registerIndex;
        if (!getRegisterIndexFromStr(operand, registerIndex)) {
            return 0;
        }
        return registerValues[registerIndex];
    }
}

bool basicBranchHandler(const QStringList& operands, size_t* currentInstruction, const QStringList& instructionList) {
    for (size_t i = 0; i < instructionList.size(); i++) {
        if (instructionList[i].startsWith(operands[0] + ":")) {
            *currentInstruction = i;
            return true;
        }
    }
    return false; // No suitable label.
}

bool equalBranchHandler(const QStringList& operands, size_t* currentInstruction, const QStringList& instructionList) {
    if (specialRegisterValues[specialRegisters::COMPARISON] != comparisonResult::EQUAL_TO) {
        return true;
    }
    return basicBranchHandler(operands, currentInstruction, instructionList);
}

bool notEqualBranchHandler(const QStringList& operands, size_t* currentInstruction, const QStringList& instructionList) {
    if (specialRegisterValues[specialRegisters::COMPARISON] == comparisonResult::EQUAL_TO) {
        return true;
    }
    return basicBranchHandler(operands, currentInstruction, instructionList);
}

bool lessBranchHandler(const QStringList& operands, size_t* currentInstruction, const QStringList& instructionList) {
    if (specialRegisterValues[specialRegisters::COMPARISON] != comparisonResult::LESS_THAN) {
        return true;
    }
    return basicBranchHandler(operands, currentInstruction, instructionList);
}

bool greaterBranchHandler(const QStringList& operands, size_t* currentInstruction, const QStringList& instructionList) {
    if (specialRegisterValues[specialRegisters::COMPARISON] != comparisonResult::GRTR_THAN) {
        return true;
    }
    return basicBranchHandler(operands, currentInstruction, instructionList);
}

// https://filestore.aqa.org.uk/resources/computing/AQA-75162-75172-ALI.PDF
// Also, not too sure whether registers are 'immediate' or 'direct' addresses
// so TODO: Find out? (https://www.tutorialspoint.com/assembly_programming/assembly_addressing_modes.htm)
std::vector<Opcode> opcodeList = {
    {.mnemonic = "LDR", .operandCount = 2, .handlerFn = [](const QStringList& operands) {
         // LDR Rd, <memory ref>:
         //  Load the value stored in the memory location
         //  specified by <memory ref> into register d.
         size_t destinationRegisterIndex = 0;
         if (!getRegisterIndexFromStr(operands[0], destinationRegisterIndex)) {
             return false;
         }
         size_t sourceMemoryAddress = operands[1].toUInt(); // TODO: Validation
         registerValues[destinationRegisterIndex] = getMemoryAddressValue(sourceMemoryAddress);
         return true;
    }},
    {.mnemonic = "STR", .operandCount = 2, .handlerFn = [](const QStringList& operands) {
         // STR Rd, <memory ref>:
         //  Store the value that is in register d into
         //  the memory location specified by <memory ref>.
         size_t sourceRegisterIndex = 0;
         if (!getRegisterIndexFromStr(operands[0], sourceRegisterIndex)) {
             return false;
         }
         size_t destinationMemoryAddress = operands[1].toUInt(); // TODO: Validation (See 'LDR'.)
         setMemoryAddressValue(destinationMemoryAddress, registerValues[sourceRegisterIndex]);
         return true;
     }},
    {.mnemonic = "ADD", .operandCount = 3, .handlerFn = [](const QStringList& operands) {
         // ADD Rd, Rn, <operand2>:
         //  Add the value specified in <operand2> to
         //  the value in register n and store the
         //  result in register d.
         size_t destRegisterIndex = 0;
         size_t firstRegisterIndex = 0;
         if (!getRegisterIndexFromStr(operands[0], destRegisterIndex) ||
             !getRegisterIndexFromStr(operands[1], firstRegisterIndex)) {
             return false;
         }
         registerValues[destRegisterIndex] = registerValues[firstRegisterIndex] + universalGetValue(operands[2]);
         return true;
     }},
    {.mnemonic = "SUB", .operandCount = 3, .handlerFn = [](const QStringList& operands) {
         // ADD Rd, Rn, <operand2>:
         //  Add the value specified in <operand2> to
         //  the value in register n and store the
         //  result in register d.
         size_t destRegisterIndex = 0;
         size_t firstRegisterIndex = 0;
         if (!getRegisterIndexFromStr(operands[0], destRegisterIndex) ||
             !getRegisterIndexFromStr(operands[1], firstRegisterIndex)) {
             return false;
         }
         registerValues[destRegisterIndex] = registerValues[firstRegisterIndex] - universalGetValue(operands[2]);
         return true;
     }},
    {.mnemonic = "MOV", .operandCount = 2, .handlerFn = [](const QStringList& operands) {
         // MOV Rd, <operand2>:
         //  Copy the value specified by <operand2>
         //  into register d.
         size_t destinationRegisterIndex = 0;
         if (!getRegisterIndexFromStr(operands[0], destinationRegisterIndex)) {
             return false;
         }
         registerValues[destinationRegisterIndex] = universalGetValue(operands[1]);
         return true;
     }},
    {.mnemonic = "CMP", .operandCount = 2, .handlerFn = [](const QStringList& operands) {
         // CMP Rn, <operand2>:
         //  Compare the value stored in register n with the
         //  value specified by <operand2>.
         size_t registerIndex = 0;
         if (!getRegisterIndexFromStr(operands[0], registerIndex)) {
             return false;
         }
         const uint8_t firstVal = registerValues[registerIndex];
         const uint8_t secondVal = universalGetValue(operands[1]);
         specialRegisterValues[specialRegisters::COMPARISON] = (firstVal == secondVal) ?
            (comparisonResult::EQUAL_TO) : ( (firstVal < secondVal) ? (comparisonResult::LESS_THAN) :
            (comparisonResult::GRTR_THAN) );
         return true;
     }},
    {.mnemonic = "B", .operandCount = 1, .handlerFn = (bool(*)(const QStringList&))&basicBranchHandler},
    {.mnemonic = "BEQ", .operandCount = 1, .handlerFn = (bool(*)(const QStringList&))&equalBranchHandler},
    {.mnemonic = "BNE", .operandCount = 1, .handlerFn = (bool(*)(const QStringList&))&notEqualBranchHandler},
    {.mnemonic = "BLT", .operandCount = 1, .handlerFn = (bool(*)(const QStringList&))&lessBranchHandler},
    {.mnemonic = "BGT", .operandCount = 1, .handlerFn = (bool(*)(const QStringList&))&greaterBranchHandler},
    {.mnemonic = "AND", .operandCount = 3, .handlerFn = [](const QStringList& operands){
         // AND Rd, Rn, <operand2>:
         //  Perform a bitwise logical AND operation between
         //  the value in register n and the value specified
         //  by <operand2> and store the result in register d.
         size_t destRegisterIndex = 0;
         size_t firstRegisterIndex = 0;
         if (!getRegisterIndexFromStr(operands[0], destRegisterIndex) ||
             !getRegisterIndexFromStr(operands[1], firstRegisterIndex)) {
             return false;
         }
         registerValues[destRegisterIndex] = registerValues[firstRegisterIndex] & universalGetValue(operands[2]);
         return true;
     }},
    {.mnemonic = "ORR", .operandCount = 3, .handlerFn = [](const QStringList& operands){
         // ORR Rd, Rn, <operand2>:
         //  Perform a bitwise logical OR between the value in register
         //  n and the value specified by <operand2> and store the result
         //  in register d.
         size_t destRegisterIndex = 0;
         size_t firstRegisterIndex = 0;
         if (!getRegisterIndexFromStr(operands[0], destRegisterIndex) ||
             !getRegisterIndexFromStr(operands[1], firstRegisterIndex)) {
             return false;
         }
         registerValues[destRegisterIndex] = registerValues[firstRegisterIndex] | universalGetValue(operands[2]);
         return true;
     }},
    {.mnemonic = "EOR", .operandCount = 3, .handlerFn = [](const QStringList& operands){
         // EOR Rd, Rn, <operand2>:
         //  Perform a bitwise logical XOR (exclusive or) operation between
         //  the value in register n and the value specified by <operand2>
         //  store the result in register d.
         size_t destRegisterIndex = 0;
         size_t firstRegisterIndex = 0;
         if (!getRegisterIndexFromStr(operands[0], destRegisterIndex) ||
             !getRegisterIndexFromStr(operands[1], firstRegisterIndex)) {
             return false;
         }
         registerValues[destRegisterIndex] = registerValues[firstRegisterIndex] ^ universalGetValue(operands[2]);
         return true;
     }},
    {.mnemonic = "MVN", .operandCount = 2, .handlerFn = [](const QStringList& operands){
         // MVN Rd, <operand2>:
         //  Perform a bitwise logical NOT operation on the
         //  value specified by <operand2> and store the
         //  result in register d.
         size_t destRegisterIndex = 0;
         if (!getRegisterIndexFromStr(operands[0], destRegisterIndex)) {
             return false;
         }
         registerValues[destRegisterIndex] = ~universalGetValue(operands[2]);
         return true;
     }},
    {.mnemonic = "LSL", .operandCount = 3, .handlerFn = [](const QStringList& operands){
         // LSL Rd, Rn, <operand2>:
         //  Logically shift left the value stored in register n
         //  by the number of bits specified by <operand2>
         //  and store the result in register d.
         size_t destRegisterIndex = 0;
         size_t firstRegisterIndex = 0;
         if (!getRegisterIndexFromStr(operands[0], destRegisterIndex) ||
             !getRegisterIndexFromStr(operands[1], firstRegisterIndex)) {
             return false;
         }
         registerValues[destRegisterIndex] = registerValues[firstRegisterIndex] << universalGetValue(operands[2]);
         return true;
     }},
    {.mnemonic = "LSR", .operandCount = 3, .handlerFn = [](const QStringList& operands){
         // LSR Rd, Rn, <operand2>:
         //  Logically shift right the value stored in register n
         //  by the number of bits specified by <operand2>
         //  and store the result in register d.
         size_t destRegisterIndex = 0;
         size_t firstRegisterIndex = 0;
         if (!getRegisterIndexFromStr(operands[0], destRegisterIndex) ||
             !getRegisterIndexFromStr(operands[1], firstRegisterIndex)) {
             return false;
         }
         registerValues[destRegisterIndex] = registerValues[firstRegisterIndex] >> universalGetValue(operands[2]);
         return true;
     }}
};

struct Instruction {
    off_t opcodeIndex = 0; // __darwin_off_t -> INT64 (signed!)
    QStringList operands;
};

[[nodiscard]] bool expandInstructionStr(const QString& instruction, Instruction& output) {
    // Lots of sources say that 'goto' statements are horrible but I don't see any
    // issue(s) with using one here as the code remains legible and efficient.
    QString chunkBuffer = "";
    for (size_t characterIndex = 0; characterIndex < instruction.length(); characterIndex++) {
        QChar iterativeCharacter = instruction[characterIndex];
        if (iterativeCharacter == ' ' || iterativeCharacter == ':' || iterativeCharacter == ';') {
            processChunk:
            if (iterativeCharacter == ':') {
                output.opcodeIndex = 0;
                return true;
            }
            if (output.opcodeIndex == 0) {
                // No opcode has been assigned, this must be the first chunk (intended to represent
                // the opcode).
                for (size_t iterativeOpcodeIndex = 0; iterativeOpcodeIndex < opcodeList.size(); iterativeOpcodeIndex++) {
                    if (opcodeList[iterativeOpcodeIndex].mnemonic == chunkBuffer) {
                        output.opcodeIndex = iterativeOpcodeIndex + 1;
                    }
                }
                if (output.opcodeIndex == 0) {
                    return false; // Invalid opcode! 😭
                }
            }
            else {
                output.operands.push_back(chunkBuffer);
            }
            if (iterativeCharacter == ';') {
                characterIndex++;
                while (characterIndex < instruction.length() && instruction[characterIndex++] != '\n') { }
            }
            chunkBuffer.clear();
            continue;
        }
        else if (iterativeCharacter == ',') {
            if (characterIndex == instruction.length() - 1) {
                // We're at the end of the string.
                return false;
            }
            else if (instruction[characterIndex + 1] != ' ') {
                return false;
            }
            characterIndex++;
            goto processChunk;
        }
        else {
            chunkBuffer += iterativeCharacter;
            if (characterIndex == instruction.length() - 1) {
                // This is the last character.
                goto processChunk;
            }
        }
    }
    return output.operands.size() == opcodeList[output.opcodeIndex - 1].operandCount;
}

void MainWindow::RunCode() {
    const QString codeString = this->ui->codeInputTextEdit->toPlainText();
    const QStringList instructionStrList = codeString.split("\n");
    applicationMemory = preApplicationMemory;
    for (size_t currentInstructionIndex = 0; currentInstructionIndex < instructionStrList.size(); currentInstructionIndex++) {
        const QString& iterativeInstructionStr = instructionStrList[currentInstructionIndex];
        Instruction iterativeInstructionRaw = {NULL};
        if (!expandInstructionStr(iterativeInstructionStr, iterativeInstructionRaw)) {
            // TODO: Log error.
            break;
        }
        if (iterativeInstructionRaw.opcodeIndex == 0) {
            continue;
        }
        const Opcode& instructionOpcode = opcodeList[iterativeInstructionRaw.opcodeIndex - 1];
        qInfo(instructionOpcode.mnemonic.toStdString().c_str());
        if (instructionOpcode.mnemonic.startsWith("B")) {
            // B, BEQ, BLT, BGT, BNE
            ((bool(*)(const QStringList&, size_t*, const QStringList&))instructionOpcode.handlerFn)(iterativeInstructionRaw.operands, &currentInstructionIndex, instructionStrList);
        }
        else {
            instructionOpcode.handlerFn(iterativeInstructionRaw.operands);
        }
        // TODO: Only update UI if something meaningful has changed.
        ui->registerListView->clear();
        for (size_t registerIndex = 0; registerIndex < registerValues.size(); registerIndex++) {
            ui->registerListView->addItem("Register " + QString::number(registerIndex, 10) + ": " + QString::number(registerValues[registerIndex], 10));
        }
    }
    specialRegisterValues.clear();
    applicationMemory.clear();
    return;
}

void MainWindow::ExecCommand() {
    const QString command = ui->commandLineEdit->text();
    // There are a million more modular ways to do this
    // but the command-line part of the program is primarily
    // for debugging so I'll leave it for later. (TODO:)
    // Also, TODO: Error handling!
    QStringList commandSegments = command.split("/");
    if (commandSegments.size() == 0) {
        return;
    }
    else if (commandSegments[0] == "SETMEM") {
        if (commandSegments.size() != 3) {
            return;
        }
        uint32_t address = commandSegments[1].toUInt();
        for (MemoryByte memByte : preApplicationMemory) {
            if (memByte.address == address) {
                memByte.value = commandSegments[2].toShort();
                return;
            }
        }
        MemoryByte NewByte = {.address = address, .value = (uint8_t)commandSegments[2].toShort()};
        preApplicationMemory.push_back(NewByte);
    }
}
