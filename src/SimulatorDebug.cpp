#include "../include/Simulator.h"
#include <cstdarg>
#include <iomanip>
#include <iostream>
#include <sstream>

template <typename T>
void printElement(T t, const int &width)
{
	std::cout << std::left << std::setw(width) << std::setfill(' ') << t;
}

std::string getPRegName(int regNum)
{
	if (regNum == NUM_PHYS_REG)
		return "$ZERO";
	return "$" + std::to_string(regNum);
}

namespace
{
std::string archRegName(const ArchitecturalRegister& reg)
{
	return std::string(1, reg.type == ArchitecturalRegister::X ? 'X' : 'F') + std::to_string(reg.num);
}

std::string boolName(bool value)
{
	return value ? "true" : "false";
}
}

void dumpDataMemory(double *dataMemory)
{
	std::cout << "Data Cache\n";
	std::cout << "\tAddress\tValue" << std::endl;
	for (int i = 0; i < MAX_MEM_SIZE - 1; i++)
	{
		if (dataMemory[i] != 0)
			std::cout << "\t" << i << ":\t" << (int)dataMemory[i] << "\n";
	}
}

void dumpRegisters(RegisterFileEntry *registers, std::map<ArchitecturalRegister, int> registerMapTable)
{
	// std::cout
	// 	<< "Register Mapping\nNote: only registers that are written to (at least once) are shown."
	// 	<< "\nFor registers used only as operands (not written to), assume they are mapped to "
	// 	   "register with value = 0."
	// 	<< std::endl;

	std::cout << "Register Mapping" << std::endl;
	for (auto &v : registerMapTable)
	{
		switch (v.first.type)
		{
		case ArchitecturalRegister::X:
			std::cout << "\tX" << v.first.num << " -> " << getPRegName(v.second) << " = "
					  << registers[v.second].value << std::endl;
			break;
		case ArchitecturalRegister::F:
			std::cout << "\tF" << v.first.num << " -> " << getPRegName(v.second) << " = "
					  << registers[v.second].value << std::endl;
			break;
		}
	}

	if (debug)
	{
		std::cout << "=========Physical Registers=========" << std::endl;
		std::cout << "\tRegister ,  Value" << std::endl;
		std::cout << std::boolalpha;
		for (int i = 0; i < NUM_PHYS_REG; i++)
		{
			std::cout << "\t";
			printElement(i, 10);
			// printElement(registers[i].busy, 6);
			// std::cout << " ";
			// printElement(registers[i].ROBNum, 5);
			std::cout << " ";
			printElement(registers[i].value, 5);
			std::cout << std::endl;
		}
	}
}

/**
 * Prints the current state of the data and instruction cache
 * Used for debugging purposes
 */
void Simulator::dump(int argc...)
{
	va_list args;
	va_start(args, argc);
	while (argc > 0)
	{
		argc--;
		enum DebugArg arg = (enum DebugArg)va_arg(args, int);
		switch (arg)
		{
		case DEBUG_DCACHE:
			dumpDataMemory(this->dataMemory);
			break;
		case DEBUG_REGISTERS:
			dumpRegisters(this->registerFile, this->registerMapTable);
			break;
		}
	}
	va_end(args);
}

void Simulator::snapshotRsForTrace()
{
	traceRsInt = rsInt;
	traceRsLoad = rsLoad;
	traceRsStore = rsStore;
	traceRsFpAdd = rsFpAdd;
	traceRsFpMul = rsFpMul;
	traceRsFpDiv = rsFpDiv;
	traceRsBu = rsBu;
}

void Simulator::printTraceInitial()
{
	std::cout << "[Initial Memory State]" << std::endl;
	std::cout << "Instruction Cache" << std::endl;
	for (size_t i = 0; i < programInstructions.size(); ++i)
	{
		std::cout << "\t" << programInstructions[i].pc << ": " << programInstructions[i].rawText << std::endl;
	}
	dumpDataMemory(this->dataMemory);
	std::cout << std::endl;
	std::cout << "[Simulation Results]" << std::endl;
}

void Simulator::printTraceCycle()
{
	auto opcodeMnemonic = [](Opcode opcode) -> std::string
	{
		switch (opcode)
		{
		case OP_FLD:
			return "fld";
		case OP_FSD:
			return "fsd";
		case OP_ADD:
			return "add";
		case OP_ADDI:
			return "addi";
		case OP_SLT:
			return "slt";
		case OP_FADD:
			return "fadd";
		case OP_FSUB:
			return "fsub";
		case OP_FMUL:
			return "fmul";
		case OP_FDIV:
			return "fdiv";
		case OP_BNE:
			return "bne";
		default:
			return "nop";
		}
	};

	auto opcodeName = [&opcodeMnemonic](const DecodedInstruction& decoded) -> std::string
	{
		if (decoded.rawText.empty())
		{
			return opcodeMnemonic(decoded.opcode);
		}
		std::stringstream ss(decoded.rawText);
		std::string op;
		ss >> op;
		return op;
	};

	auto renamedInstText = [&](const RenamedInstruction& inst) -> std::string
	{
		std::ostringstream oss;
		switch (inst.decoded.opcode)
		{
		case OP_FLD:
			oss << "fld  " << getPRegName(inst.dstPhys) << ", " << inst.decoded.imm << "(" << getPRegName(inst.src1Phys) << ")";
			break;
		case OP_FSD:
			oss << "fsd  " << getPRegName(inst.src1Phys) << ", " << inst.decoded.imm << "(" << getPRegName(inst.src2Phys) << ")";
			break;
		case OP_ADD:
			oss << "add  " << getPRegName(inst.dstPhys) << ", " << getPRegName(inst.src1Phys) << ", " << getPRegName(inst.src2Phys);
			break;
		case OP_ADDI:
			oss << "addi " << getPRegName(inst.dstPhys) << ", " << getPRegName(inst.src1Phys) << ", " << inst.decoded.imm;
			break;
		case OP_SLT:
			oss << "slt  " << getPRegName(inst.dstPhys) << ", " << getPRegName(inst.src1Phys) << ", " << getPRegName(inst.src2Phys);
			break;
		case OP_FADD:
			oss << "fadd " << getPRegName(inst.dstPhys) << ", " << getPRegName(inst.src1Phys) << ", " << getPRegName(inst.src2Phys);
			break;
		case OP_FSUB:
			oss << "fsub " << getPRegName(inst.dstPhys) << ", " << getPRegName(inst.src1Phys) << ", " << getPRegName(inst.src2Phys);
			break;
		case OP_FMUL:
			oss << "fmul " << getPRegName(inst.dstPhys) << ", " << getPRegName(inst.src1Phys) << ", " << getPRegName(inst.src2Phys);
			break;
		case OP_FDIV:
			oss << "fdiv " << getPRegName(inst.dstPhys) << ", " << getPRegName(inst.src1Phys) << ", " << getPRegName(inst.src2Phys);
			break;
		case OP_BNE:
			oss << "bne  " << getPRegName(inst.src1Phys) << ", " << getPRegName(inst.src2Phys) << ", " << inst.decoded.targetPc;
			break;
		default:
			oss << "nop";
			break;
		}
		return oss.str();
	};

	auto printRsRow = [&](const std::string& name, const RsEntry& entry) -> void
	{
		std::cout << "\t";
		printElement(name, 7);
		printElement(boolName(entry.busy), 6);
		printElement(entry.remainingCycles, 15);
		printElement(entry.busy ? opcodeName(entry.inst.decoded) : "nop", 7);
		printElement(entry.busy ? getPRegName(entry.inst.dstPhys >= 0 ? entry.inst.dstPhys : 0) : "$0", 6);
		printElement(entry.busy ? getPRegName(entry.inst.src1Phys) : "$0", 6);
		printElement(entry.busy ? getPRegName(entry.inst.src2Phys) : "$0", 6);
		printElement(entry.busy ? entry.inst.robIndex : -1, 5);
		std::cout << std::endl;
	};

	std::cout << "Clock Cycle: " << cycle << std::endl;
	std::cout << " PC: " << ((fetchHalted || frontEndBlockedOnBranch) ? -1 : fetchPc) << " ================================================" << std::endl;

	std::cout << "Fetch Queue" << std::endl;
	for (size_t i = 0; i < fetchQueue.size(); ++i)
	{
		const int pc = fetchQueue[i].pc;
		if (pc >= 0 && pc / 4 < static_cast<int>(programInstructions.size()))
		{
			std::cout << "\t" << pc << ": " << programInstructions[pc / 4].rawText << std::endl;
		}
	}

	std::cout << "Instruction (Decode) Queue" << std::endl;
	for (size_t i = 0; i < decodeQueue.size(); ++i)
	{
		std::cout << "\t" << renamedInstText(decodeQueue[i]) << std::endl;
	}

	std::cout << "Reservation Station" << std::endl;
	std::cout << "\tType\tBusy\tRemaining CC\tOp    \tDest\tSrc1\tSrc2\tROB Index" << std::endl;
	printRsRow("Load1", rsLoad[0]);
	printRsRow("Load2", rsLoad[1]);
	printRsRow("BU1", rsBu[0]);
	printRsRow("BU2", rsBu[1]);
	printRsRow("INT1", rsInt[0]);
	printRsRow("INT2", rsInt[1]);
	printRsRow("INT3", rsInt[2]);
	printRsRow("INT4", rsInt[3]);
	printRsRow("FPadd1", rsFpAdd[0]);
	printRsRow("FPadd2", rsFpAdd[1]);
	printRsRow("FPadd3", rsFpAdd[2]);
	printRsRow("FPmult1", rsFpMul[0]);
	printRsRow("FPmult2", rsFpMul[1]);
	printRsRow("FPDiv1", rsFpDiv[0]);
	printRsRow("Store1", rsStore[0]);
	printRsRow("Store2", rsStore[1]);

	std::cout << "Reorder Buffer" << std::endl;
	std::cout << "\tHEAD:\t" << robHead << "\tTAIL:\t" << robTail << std::endl;
	std::cout << "\tROB  BUSY  DEST  VALUE OP" << std::endl;
	for (size_t i = 0; i < rob.size(); ++i)
	{
		std::string dest = "";
		if (rob[i].busy && rob[i].hasDest)
		{
			dest = archRegName(rob[i].archDest);
		}
		const Opcode robOpcode = rob[i].isStore ? OP_FSD : (rob[i].isBranch ? OP_BNE : rob[i].robOpcode);
		const std::string op = rob[i].busy ? opcodeMnemonic(robOpcode) : "nop";
		const int value = static_cast<int>(rob[i].isBranch ? rob[i].actualNextPc : rob[i].value);
		std::cout << "\t";
		printElement(static_cast<int>(i), 5);
		printElement(boolName(rob[i].busy), 6);
		printElement(dest, 6);
		printElement(value, 6);
		printElement(op, 4);
		std::cout << std::endl;
	}

	dumpRegisters(this->registerFile, this->registerMapTable);
}
