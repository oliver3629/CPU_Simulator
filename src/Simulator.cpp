#include "Simulator.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace
{
std::string trim(const std::string& s)
{
	size_t start = 0;
	while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])))
	{
		++start;
	}
	size_t end = s.size();
	while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1])))
	{
		--end;
	}
	return s.substr(start, end - start);
}

std::string lowerCopy(std::string s)
{
	for (char& c : s)
	{
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}
	return s;
}
}

// Initialize simulator: load program and build initial microarchitecture state.
Simulator::Simulator(std::ifstream* program, Config *c)
{
	configuration = c;
	parseProgram(program);
	initializeState();
}

// Main clock loop: advance stages in fixed order until the pipeline drains.
void Simulator::run()
{
	while (!simulationDone)
	{
		++cycle;
		Writeback();
		Commit();
		Issue();
		Execute();
		Decode();
		Fetch();
		simulationDone = fetchHalted && !hasInFlightWork();
	}
}

// Parse program input: data init section first, then code and labels.
void Simulator::parseProgram(std::ifstream* program)
{
	if (program == NULL || !program->is_open())
	{
		throw std::runtime_error("Program input stream is not available.");
	}

	std::vector<std::pair<int, std::string> > rawInstructions;
	std::string line;
	bool inCode = false;
	int pc = 0;
	while (std::getline(*program, line))
	{
		line = trim(line);
		if (line.empty())
		{
			continue;
		}
		if (line == "%%")
		{
			inCode = true;
			continue;
		}

		if (!inCode)
		{
			// First half: initialize data memory from address,value pairs.
			if (line[0] == '%')
			{
				continue;
			}
			size_t comma = line.find(',');
			if (comma == std::string::npos)
			{
				continue;
			}
			int address = std::stoi(trim(line.substr(0, comma)));
			double value = std::stod(trim(line.substr(comma + 1)));
			if (address >= 0 && address < MAX_MEM_SIZE)
			{
				dataMemory[address] = value;
			}
			continue;
		}

		if (line[0] == '%')
		{
			continue;
		}

		size_t colon = line.find(':');
		if (colon != std::string::npos)
		{
			// Record label address, then parse the rest of the line as an instruction.
			std::string label = trim(line.substr(0, colon));
			if (!label.empty())
			{
				labelToPc[label] = pc;
			}
			line = trim(line.substr(colon + 1));
			if (line.empty())
			{
				continue;
			}
		}

		rawInstructions.push_back(std::make_pair(pc, line));
		pc += 4;
	}

	for (size_t i = 0; i < rawInstructions.size(); ++i)
	{
		programInstructions.push_back(parseInstructionLine(rawInstructions[i].second, rawInstructions[i].first));
	}
}

// Parse one assembly line into the internal DecodedInstruction representation.
Simulator::DecodedInstruction Simulator::parseInstructionLine(const std::string& line, int pc)
{
	auto parseReg = [](std::string regText) -> ArchitecturalRegister
	{
		regText = trim(regText);
		if (!regText.empty() && regText[0] == '$')
		{
			regText = regText.substr(1);
		}
		if (regText.empty())
		{
			throw std::runtime_error("Empty register field.");
		}
		char type = static_cast<char>(std::toupper(static_cast<unsigned char>(regText[0])));
		if (regText == "0")
		{
			return ArchitecturalRegister{ArchitecturalRegister::X, 0};
		}
		int num = 0;
		if (regText.size() > 1)
		{
			num = std::stoi(regText.substr(1));
		}
		if (type == 'X')
		{
			return ArchitecturalRegister{ArchitecturalRegister::X, num};
		}
		if (type == 'F')
		{
			return ArchitecturalRegister{ArchitecturalRegister::F, num};
		}
		throw std::runtime_error("Unsupported register: " + regText);
	};

	DecodedInstruction inst;
	inst.pc = pc;
	inst.nextPc = pc + 4;
	inst.rawText = line;

	std::string normalized = line;
	for (size_t i = 0; i < normalized.size(); ++i)
	{
		if (normalized[i] == ',' || normalized[i] == '(' || normalized[i] == ')')
		{
			normalized[i] = ' ';
		}
	}
	std::stringstream ss(normalized);
	std::vector<std::string> tokens;
	std::string token;
	while (ss >> token)
	{
		tokens.push_back(token);
	}
	// Tokenize first, then decode by opcode so formatting stays consistent.
	if (tokens.empty())
	{
		throw std::runtime_error("Invalid instruction line: " + line);
	}
	const std::string op = lowerCopy(tokens[0]);

	if (op == "fld")
	{
		inst.opcode = OP_FLD;
		inst.hasDst = true;
		inst.dst = parseReg(tokens.at(1));
		inst.hasSrc1 = true;
		inst.imm = std::stoi(tokens.at(2));
		inst.src1 = parseReg(tokens.at(3));
	}
	else if (op == "fsd")
	{
		inst.opcode = OP_FSD;
		inst.hasSrc1 = true;
		inst.src1 = parseReg(tokens.at(1));
		inst.imm = std::stoi(tokens.at(2));
		inst.hasSrc2 = true;
		inst.src2 = parseReg(tokens.at(3));
	}
	else if (op == "add")
	{
		inst.opcode = OP_ADD;
		inst.hasDst = true;
		inst.dst = parseReg(tokens.at(1));
		inst.hasSrc1 = true;
		inst.src1 = parseReg(tokens.at(2));
		inst.hasSrc2 = true;
		inst.src2 = parseReg(tokens.at(3));
	}
	else if (op == "addi")
	{
		inst.opcode = OP_ADDI;
		inst.hasDst = true;
		inst.dst = parseReg(tokens.at(1));
		inst.hasSrc1 = true;
		inst.src1 = parseReg(tokens.at(2));
		inst.imm = std::stoi(tokens.at(3));
	}
	else if (op == "slt")
	{
		inst.opcode = OP_SLT;
		inst.hasDst = true;
		inst.dst = parseReg(tokens.at(1));
		inst.hasSrc1 = true;
		inst.src1 = parseReg(tokens.at(2));
		inst.hasSrc2 = true;
		inst.src2 = parseReg(tokens.at(3));
	}
	else if (op == "fadd")
	{
		inst.opcode = OP_FADD;
		inst.hasDst = true;
		inst.dst = parseReg(tokens.at(1));
		inst.hasSrc1 = true;
		inst.src1 = parseReg(tokens.at(2));
		inst.hasSrc2 = true;
		inst.src2 = parseReg(tokens.at(3));
	}
	else if (op == "fsub")
	{
		inst.opcode = OP_FSUB;
		inst.hasDst = true;
		inst.dst = parseReg(tokens.at(1));
		inst.hasSrc1 = true;
		inst.src1 = parseReg(tokens.at(2));
		inst.hasSrc2 = true;
		inst.src2 = parseReg(tokens.at(3));
	}
	else if (op == "fmul")
	{
		inst.opcode = OP_FMUL;
		inst.hasDst = true;
		inst.dst = parseReg(tokens.at(1));
		inst.hasSrc1 = true;
		inst.src1 = parseReg(tokens.at(2));
		inst.hasSrc2 = true;
		inst.src2 = parseReg(tokens.at(3));
	}
	else if (op == "fdiv")
	{
		inst.opcode = OP_FDIV;
		inst.hasDst = true;
		inst.dst = parseReg(tokens.at(1));
		inst.hasSrc1 = true;
		inst.src1 = parseReg(tokens.at(2));
		inst.hasSrc2 = true;
		inst.src2 = parseReg(tokens.at(3));
	}
	else if (op == "bne")
	{
		inst.opcode = OP_BNE;
		inst.hasSrc1 = true;
		inst.src1 = parseReg(tokens.at(1));
		inst.hasSrc2 = true;
		inst.src2 = parseReg(tokens.at(2));
		const std::string label = tokens.at(3);
		if (labelToPc.count(label) == 0)
		{
			throw std::runtime_error("Unknown branch target label: " + label);
		}
		inst.targetPc = labelToPc[label];
	}
	else
	{
		throw std::runtime_error("Unsupported opcode: " + op);
	}

	return inst;
}

// Initialize runtime microarchitecture state.
void Simulator::initializeState()
{
	for (int i = 0; i < NUM_PHYS_REG_INCLUDING_X0; ++i)
	{
		registerFile[i].value = 0.0;
	}
	registerFile[NUM_PHYS_REG].value = 0.0;
	registerMapTable.clear();
	registerMapTable[ArchitecturalRegister{ArchitecturalRegister::X, 0}] = NUM_PHYS_REG;
	// X0 is always mapped to the reserved constant-zero physical register.

	freeList.clear();
	for (int i = 0; i < NUM_PHYS_REG; ++i)
	{
		freeList.push_back(i);
	}
	committedFreeListHistory.clear();

	physReady.assign(NUM_PHYS_REG_INCLUDING_X0, true);
	physReady[NUM_PHYS_REG] = true;

	rob.assign(configuration->NR, RobEntry());
	robHead = 0;
	robTail = 0;
	robCount = 0;

	rsInt.assign(4, RsEntry());
	rsLoad.assign(2, RsEntry());
	rsStore.assign(2, RsEntry());
	rsFpAdd.assign(3, RsEntry());
	rsFpMul.assign(2, RsEntry());
	rsFpDiv.assign(1, RsEntry());
	rsBu.assign(2, RsEntry());

	pendingCdbEvents.clear();
	pendingBranchEvents.clear();
	fetchQueue.clear();
	decodeQueue.clear();
	fetchPc = 0;
	fetchHalted = false;
	simulationDone = false;
	cycle = 0;
	rsStalls = 0;
	robStalls = 0;
	cdbWrites = 0;
	cdbCapacityUsed = 0;
	instructionSeq = 0;

	for (int i = 0; i < 16; ++i)
	{
		btb[i] = BranchPredictorEntry();
	}
	rebuildFetchSpecFromMap();
}

bool Simulator::hasInFlightWork() const
{
	auto hasBusy = [](const std::vector<RsEntry>& rs) -> bool
	{
		for (size_t i = 0; i < rs.size(); ++i)
		{
			if (rs[i].busy)
			{
				return true;
			}
		}
		return false;
	};
	return !fetchQueue.empty() || !decodeQueue.empty() || robCount > 0 || !pendingCdbEvents.empty() || !pendingBranchEvents.empty() ||
		hasBusy(rsInt) || hasBusy(rsLoad) || hasBusy(rsStore) || hasBusy(rsFpAdd) || hasBusy(rsFpMul) || hasBusy(rsFpDiv) || hasBusy(rsBu);
}

int Simulator::btbIndexForPc(int pc) const
{
	// The 16-entry BTB uses middle PC bits so adjacent instructions do not all collide.
	return (pc >> 4) & 0xF;
}

bool Simulator::predictBranch(int pc, int& predictedNextPc) const
{
	const int idx = btbIndexForPc(pc);
	const BranchPredictorEntry& entry = btb[idx];
	// Counter values 2 and 3 represent taken in the saturating 2-bit predictor.
	const bool predictTaken = entry.valid && entry.tagPc == pc && entry.counter >= 2;
	if (predictTaken)
	{
		predictedNextPc = entry.targetPc;
		return true;
	}
	predictedNextPc = pc + 4;
	return false;
}

void Simulator::updateBranchPredictor(int pc, bool taken, int targetPc)
{
	const int idx = btbIndexForPc(pc);
	BranchPredictorEntry& entry = btb[idx];
	if (!entry.valid || entry.tagPc != pc)
	{
		// New BTB entries start weakly taken after the first observed branch.
		entry.valid = true;
		entry.tagPc = pc;
		entry.targetPc = targetPc;
		entry.counter = 2;
	}
	entry.targetPc = targetPc;
	if (taken)
	{
		entry.counter = std::min(3, entry.counter + 1);
	}
	else
	{
		entry.counter = std::max(0, entry.counter - 1);
	}
}

Simulator::FuType Simulator::classifyFu(const DecodedInstruction& decoded) const
{
	switch (decoded.opcode)
	{
	case OP_FLD:
		return FU_LS_LOAD;
	case OP_FSD:
		return FU_LS_STORE;
	case OP_ADD:
	case OP_ADDI:
	case OP_SLT:
		return FU_INT;
	case OP_FADD:
	case OP_FSUB:
		return FU_FP_ADD;
	case OP_FMUL:
		return FU_FP_MUL;
	case OP_FDIV:
		return FU_FP_DIV;
	case OP_BNE:
		return FU_BU;
	default:
		return FU_INT;
	}
}

int Simulator::latencyFor(const DecodedInstruction& decoded) const
{
	switch (decoded.opcode)
	{
	case OP_FLD:
	case OP_FSD:
		return 1;
	case OP_FADD:
	case OP_FSUB:
		return 3;
	case OP_FMUL:
		return 4;
	case OP_FDIV:
		return 6;
	default:
		return 1;
	}
}

int Simulator::getMappedPhys(const ArchitecturalRegister& reg) const
{
	if (reg.type == ArchitecturalRegister::X && reg.num == 0)
	{
		return NUM_PHYS_REG;
	}
	std::map<ArchitecturalRegister, int>::const_iterator it = registerMapTable.find(reg);
	if (it == registerMapTable.end())
	{
		return NUM_PHYS_REG;
	}
	return it->second;
}

double Simulator::readPhysValue(int phys) const
{
	return registerFile[phys].value;
}

void Simulator::writePhysValue(int phys, double value)
{
	registerFile[phys].value = value;
}

bool Simulator::isPhysReady(int phys) const
{
	return physReady[phys];
}

void Simulator::setPhysReady(int phys, bool ready)
{
	physReady[phys] = ready;
}

void Simulator::rebuildFetchSpecFromMap()
{
	// Mirror committed/renamed integer state for fetch-time branch checks.
	fetchSpecXValues.assign(32, 0.0);
	fetchSpecXReady.assign(32, true);
	for (int i = 1; i < 32; ++i)
	{
		const ArchitecturalRegister reg = {ArchitecturalRegister::X, i};
		const int phys = getMappedPhys(reg);
		const bool ready = isPhysReady(phys);
		fetchSpecXReady[i] = ready;
		fetchSpecXValues[i] = ready ? readPhysValue(phys) : 0.0;
	}
}

void Simulator::applyFetchSpecUpdate(const DecodedInstruction& decoded)
{
	if (!decoded.hasDst || decoded.dst.type != ArchitecturalRegister::X || decoded.dst.num == 0 ||
		decoded.dst.num < 0 || decoded.dst.num >= 32)
	{
		return;
	}

	const int dst = decoded.dst.num;
	auto readX = [this](const ArchitecturalRegister& reg, int64_t& value) -> bool
	{
		if (reg.type != ArchitecturalRegister::X || reg.num < 0 || reg.num >= 32 || !fetchSpecXReady[reg.num])
		{
			return false;
		}
		value = static_cast<int64_t>(fetchSpecXValues[reg.num]);
		return true;
	};

	int64_t src1 = 0;
	int64_t src2 = 0;
	switch (decoded.opcode)
	{
	case OP_ADD:
		if (readX(decoded.src1, src1) && readX(decoded.src2, src2))
		{
			fetchSpecXValues[dst] = static_cast<double>(src1 + src2);
			fetchSpecXReady[dst] = true;
			return;
		}
		break;
	case OP_ADDI:
		if (readX(decoded.src1, src1))
		{
			fetchSpecXValues[dst] = static_cast<double>(src1 + decoded.imm);
			fetchSpecXReady[dst] = true;
			return;
		}
		break;
	default:
		break;
	}

	// Non-trivial integer producers make the speculative value unknown.
	fetchSpecXValues[dst] = 0.0;
	fetchSpecXReady[dst] = false;
}

bool Simulator::tryEvaluateFetchBranch(const DecodedInstruction& decoded, bool& branchTaken) const
{
	if (decoded.opcode != OP_BNE ||
		decoded.src1.type != ArchitecturalRegister::X || decoded.src2.type != ArchitecturalRegister::X ||
		decoded.src1.num < 0 || decoded.src1.num >= 32 || decoded.src2.num < 0 || decoded.src2.num >= 32 ||
		!fetchSpecXReady[decoded.src1.num] || !fetchSpecXReady[decoded.src2.num])
	{
		return false;
	}
	branchTaken = static_cast<int64_t>(fetchSpecXValues[decoded.src1.num]) !=
		static_cast<int64_t>(fetchSpecXValues[decoded.src2.num]);
	return true;
}

// Rename architectural registers into physical ones at decode time.
Simulator::RenamedInstruction Simulator::decodeStageRename(const DecodedInstruction& decoded, bool& ok)
{
	ok = false;

	RenamedInstruction inst;
	inst.decoded = decoded;
	inst.fuType = classifyFu(decoded);

	if (decoded.hasSrc1)
	{
		// At decode/rename: read current RAT mapping and physical-register ready bits.
		inst.src1Phys = getMappedPhys(decoded.src1);
		inst.src1Ready = isPhysReady(inst.src1Phys);
	}
	if (decoded.hasSrc2)
	{
		inst.src2Phys = getMappedPhys(decoded.src2);
		inst.src2Ready = isPhysReady(inst.src2Phys);
	}

	if (decoded.opcode == OP_BNE)
	{
		// Branches checkpoint rename state so mispredictions can roll back younger work.
		inst.hasBranchSnapshot = true;
		inst.branchRatSnapshot = registerMapTable;
		inst.branchFreeListSnapshot = freeList;
		inst.branchFreeListHistorySize = committedFreeListHistory.size();
		inst.branchRegValueSnapshot.assign(NUM_PHYS_REG_INCLUDING_X0, 0.0);
		for (int i = 0; i < NUM_PHYS_REG_INCLUDING_X0; ++i)
		{
			inst.branchRegValueSnapshot[i] = registerFile[i].value;
		}
	}

	if (decoded.hasDst && !(decoded.dst.type == ArchitecturalRegister::X && decoded.dst.num == 0))
	{
		if (freeList.empty())
		{
			return RenamedInstruction();
		}
		inst.oldDstPhys = getMappedPhys(decoded.dst);
		inst.dstPhys = freeList.front();
		freeList.pop_front();
		// New destination physical register is not readable until writeback.
		registerMapTable[decoded.dst] = inst.dstPhys;
		setPhysReady(inst.dstPhys, false);
	}

	ok = true;
	return inst;
}

// Fetch up to NF instructions per cycle; update PC using BTB and 2-bit predictor.
void Simulator::Fetch()
{
	if (frontEndBlockedOnBranch)
	{
		return;
	}

	int fetched = 0;
	const int fetchQueueLimit = configuration->NI;
	while (fetched < configuration->NF && !fetchHalted && static_cast<int>(fetchQueue.size()) < fetchQueueLimit)
	{
		if (fetchPc < 0 || fetchPc / 4 >= static_cast<int>(programInstructions.size()))
		{
			fetchHalted = true;
			break;
		}
		FetchPacket packet;
		packet.traceSeq = fetchTraceSeq++;
		packet.pc = fetchPc;
		packet.predictedNextPc = fetchPc + 4;
		const DecodedInstruction& inst = programInstructions[fetchPc / 4];
		bool btbHit = false;
		bool blockOnBranch = false;
		if (inst.opcode == OP_BNE)
		{
			// Predict branches at fetch; follow predicted next PC.
			const int idx = btbIndexForPc(fetchPc);
			btbHit = btb[idx].valid && btb[idx].tagPc == fetchPc;
			predictBranch(fetchPc, packet.predictedNextPc);
			bool branchTaken = false;
			if (btbHit && tryEvaluateFetchBranch(inst, branchTaken))
			{
				const int actualNextPc = branchTaken ? inst.targetPc : (fetchPc + 4);
				if (actualNextPc != packet.predictedNextPc)
				{
					blockOnBranch = true;
				}
			}
		}
		fetchQueue.push_back(packet);
		fetchPc = packet.predictedNextPc;
		applyFetchSpecUpdate(inst);
		++fetched;
		if (inst.opcode == OP_BNE)
		{
			// Stop fetching after a branch for the rest of this cycle.
			// If prediction cannot supply a safe next PC, wait for BU/EX resolution.
			frontEndBlockedOnBranch = !btbHit || blockOnBranch;
			frontEndBlockingBranchTraceSeq = frontEndBlockedOnBranch ? packet.traceSeq : -1;
			break;
		}
	}
}

// Decode: decode, rename, and enqueue decoded instructions.
void Simulator::Decode()
{
	int decoded = 0;
	while (decoded < configuration->NF && !fetchQueue.empty() && static_cast<int>(decodeQueue.size()) < configuration->NI)
	{
		// Decode queue capacity is NI; if full, fetch backs off naturally.
		FetchPacket packet = fetchQueue.front();
		fetchQueue.pop_front();
		const int index = packet.pc / 4;
		if (index >= 0 && index < static_cast<int>(programInstructions.size()))
		{
			DecodedInstruction inst = programInstructions[index];
			inst.nextPc = packet.predictedNextPc;

			bool ok = false;
			RenamedInstruction renamed = decodeStageRename(inst, ok);
			if (!ok)
			{
				fetchQueue.push_front(packet);
				break;
			}
			renamed.traceSeq = packet.traceSeq;
			decodeQueue.push_back(renamed);
		}
		++decoded;
	}
}

// Allocate a ROB entry when the renamed instruction actually enters the issue queue/RS.
bool Simulator::allocateRobEntry(RenamedInstruction& inst)
{
	if (robCount >= configuration->NR)
	{
		return false;
	}

	RobEntry entry;
	entry.busy = true;
	entry.done = false;
	entry.pc = inst.decoded.pc;
	entry.rsStallCount = inst.rsStallCount;
	entry.robStallCount = inst.robStallCount;
	entry.predictedNextPc = inst.decoded.nextPc;
	entry.actualNextPc = inst.decoded.nextPc;
	entry.branchTargetPc = inst.decoded.targetPc;
	entry.robOpcode = inst.decoded.opcode;
	entry.isStore = (inst.decoded.opcode == OP_FSD);
	entry.isBranch = (inst.decoded.opcode == OP_BNE);

	if (inst.decoded.hasDst && !(inst.decoded.dst.type == ArchitecturalRegister::X && inst.decoded.dst.num == 0))
	{
		entry.hasDest = true;
		entry.archDest = inst.decoded.dst;
		entry.oldPhys = inst.oldDstPhys;
	}

	if (entry.isBranch && inst.hasBranchSnapshot)
	{
		// Keep the branch's checkpoint with the ROB entry that will resolve it.
		entry.ratSnapshot = inst.branchRatSnapshot;
		entry.freeListSnapshot = inst.branchFreeListSnapshot;
		entry.freeListHistorySize = inst.branchFreeListHistorySize;
		entry.regValueSnapshot = inst.branchRegValueSnapshot;
	}

	inst.robIndex = robTail;
	rob[robTail] = entry;
	robTail = (robTail + 1) % configuration->NR;
	++robCount;
	return true;
}

// Place the renamed instruction into the target functional unit's reservation station.
bool Simulator::tryDispatchToRS(const RenamedInstruction& inst)
{
	std::vector<RsEntry>* targetRs = NULL;
	switch (inst.fuType)
	{
	case FU_INT:
		targetRs = &rsInt;
		break;
	case FU_LS_LOAD:
		targetRs = &rsLoad;
		break;
	case FU_LS_STORE:
		targetRs = &rsStore;
		break;
	case FU_FP_ADD:
		targetRs = &rsFpAdd;
		break;
	case FU_FP_MUL:
		targetRs = &rsFpMul;
		break;
	case FU_FP_DIV:
		targetRs = &rsFpDiv;
		break;
	case FU_BU:
		targetRs = &rsBu;
		break;
	}

	for (size_t i = 0; i < targetRs->size(); ++i)
	{
		if (!(*targetRs)[i].busy)
		{
			(*targetRs)[i].busy = true;
			(*targetRs)[i].inst = inst;
			(*targetRs)[i].remainingCycles = -1;
			(*targetRs)[i].started = false;
			(*targetRs)[i].dispatchCycle = cycle;
			(*targetRs)[i].seq = instructionSeq++;
			return true;
		}
	}
	return false;
}

// Issue: under NW and per-FU limits, pick ready instructions to start execution.
void Simulator::Issue()
{
	auto hasFreeRsSlot = [this](FuType fu) -> bool
	{
		const std::vector<RsEntry>* targetRs = NULL;
		switch (fu)
		{
		case FU_INT:
			targetRs = &rsInt;
			break;
		case FU_LS_LOAD:
			targetRs = &rsLoad;
			break;
		case FU_LS_STORE:
			targetRs = &rsStore;
			break;
		case FU_FP_ADD:
			targetRs = &rsFpAdd;
			break;
		case FU_FP_MUL:
			targetRs = &rsFpMul;
			break;
		case FU_FP_DIV:
			targetRs = &rsFpDiv;
			break;
		case FU_BU:
			targetRs = &rsBu;
			break;
		}
		for (size_t i = 0; i < targetRs->size(); ++i)
		{
			if (!(*targetRs)[i].busy)
			{
				return true;
			}
		}
		return false;
	};

	int dispatched = 0;
	while (dispatched < configuration->NW && !decodeQueue.empty())
	{
		if (!hasFreeRsSlot(decodeQueue.front().fuType))
		{
			// Keep the instruction queued and count the cycle as an RS back-pressure stall.
			++decodeQueue.front().rsStallCount;
			++rsStalls;
			break;
		}
		RenamedInstruction renamed = decodeQueue.front();
		if (!allocateRobEntry(renamed))
		{
			++decodeQueue.front().robStallCount;
			++robStalls;
			break;
		}
		(void)tryDispatchToRS(renamed);
		decodeQueue.pop_front();
		++dispatched;
	}

	bool intUsed = false;
	bool fpAddUsed = false;
	bool fpMulUsed = false;
	bool fpDivUsed = false;
	bool buUsed = false;
	bool lsUsed = false;

	auto unitBusy = [](const std::vector<RsEntry>& rs) -> bool
	{
		for (size_t i = 0; i < rs.size(); ++i)
		{
			if (rs[i].busy && rs[i].started)
			{
				return true;
			}
		}
		return false;
	};

	const bool fpMulPipelineBusy = unitBusy(rsFpMul);
	const bool fpDivPipelineBusy = unitBusy(rsFpDiv);

	while (true)
	{
		RsEntry* best = NULL;
		FuType bestFu = FU_INT;

		auto firstReady = [&](std::vector<RsEntry>& rs, bool includeNewlyDispatched) -> RsEntry*
		{
			for (size_t i = 0; i < rs.size(); ++i)
			{
				if (!rs[i].busy || rs[i].started)
				{
					continue;
				}
				if (!includeNewlyDispatched && rs[i].dispatchCycle == cycle)
				{
					continue;
				}
				if (rs[i].inst.decoded.hasSrc1 && !rs[i].inst.src1Ready && isPhysReady(rs[i].inst.src1Phys))
				{
					rs[i].inst.src1Ready = true;
				}
				if (rs[i].inst.decoded.hasSrc2 && !rs[i].inst.src2Ready && isPhysReady(rs[i].inst.src2Phys))
				{
					rs[i].inst.src2Ready = true;
				}
				if ((rs[i].inst.decoded.hasSrc1 && !rs[i].inst.src1Ready) ||
					(rs[i].inst.decoded.hasSrc2 && !rs[i].inst.src2Ready))
				{
					continue;
				}
				return &rs[i];
			}
			return static_cast<RsEntry*>(NULL);
		};

		for (int pass = 0; pass < 2 && best == NULL; ++pass)
		{
			// Prefer older RS entries, then allow same-cycle dispatch if capacity remains.
			const bool includeNewlyDispatched = (pass != 0);
			if (!intUsed)
			{
				best = firstReady(rsInt, includeNewlyDispatched);
				bestFu = FU_INT;
			}
			if (best == NULL && !fpAddUsed)
			{
				best = firstReady(rsFpAdd, includeNewlyDispatched);
				bestFu = FU_FP_ADD;
			}
			if (best == NULL && !fpMulUsed && !fpMulPipelineBusy)
			{
				best = firstReady(rsFpMul, includeNewlyDispatched);
				bestFu = FU_FP_MUL;
			}
			if (best == NULL && !fpDivUsed && !fpDivPipelineBusy)
			{
				best = firstReady(rsFpDiv, includeNewlyDispatched);
				bestFu = FU_FP_DIV;
			}
			if (best == NULL && !buUsed)
			{
				best = firstReady(rsBu, includeNewlyDispatched);
				bestFu = FU_BU;
			}
			if (best == NULL && !lsUsed)
			{
				best = firstReady(rsLoad, includeNewlyDispatched);
				bestFu = FU_LS_LOAD;
			}
			if (best == NULL && !lsUsed)
			{
				best = firstReady(rsStore, includeNewlyDispatched);
				bestFu = FU_LS_STORE;
			}
		}

		if (best == NULL)
		{
			// No issuable candidate this cycle; stop issuing.
			break;
		}

		best->started = true;
		best->remainingCycles = latencyFor(best->inst.decoded);
		switch (bestFu)
		{
		case FU_INT:
			intUsed = true;
			break;
		case FU_FP_ADD:
			fpAddUsed = true;
			break;
		case FU_FP_MUL:
			fpMulUsed = true;
			break;
		case FU_FP_DIV:
			fpDivUsed = true;
			break;
		case FU_BU:
			buUsed = true;
			break;
		case FU_LS_LOAD:
		case FU_LS_STORE:
			lsUsed = true;
			break;
		}
	}
}

double Simulator::loadFromMemory(int address) const
{
	if (address < 0 || address >= MAX_MEM_SIZE)
	{
		return 0.0;
	}
	return dataMemory[address];
}

void Simulator::storeToMemory(int address, double value)
{
	if (address < 0 || address >= MAX_MEM_SIZE)
	{
		return;
	}
	dataMemory[address] = value;
}

// Execute one instruction's semantics; return result and branch/memory side effects.
double Simulator::executeInstruction(const RenamedInstruction& inst, bool& hasResult, bool& causesBranch, int& branchNextPc, bool& isStore, int& storeAddress, double& storeValue)
{
	hasResult = false;
	causesBranch = false;
	branchNextPc = inst.decoded.pc + 4;
	isStore = false;
	storeAddress = -1;
	storeValue = 0.0;

	const double src1 = inst.decoded.hasSrc1 ? readPhysValue(inst.src1Phys) : 0.0;
	const double src2 = inst.decoded.hasSrc2 ? readPhysValue(inst.src2Phys) : 0.0;

	switch (inst.decoded.opcode)
	{
	case OP_ADD:
		hasResult = true;
		return static_cast<double>(static_cast<int64_t>(src1) + static_cast<int64_t>(src2));
	case OP_ADDI:
		hasResult = true;
		return static_cast<double>(static_cast<int64_t>(src1) + static_cast<int64_t>(inst.decoded.imm));
	case OP_SLT:
		hasResult = true;
		return static_cast<int64_t>(src1) < static_cast<int64_t>(src2) ? 1.0 : 0.0;
	case OP_FADD:
		hasResult = true;
		return src1 + src2;
	case OP_FSUB:
		hasResult = true;
		return src1 - src2;
	case OP_FMUL:
		hasResult = true;
		return src1 * src2;
	case OP_FDIV:
		hasResult = true;
		return std::fabs(src2) < 1e-12 ? 0.0 : (src1 / src2);
	case OP_FLD:
		hasResult = true;
		storeAddress = static_cast<int>(static_cast<int64_t>(src1) + inst.decoded.imm);
		return loadFromMemory(storeAddress);
	case OP_FSD:
		isStore = true;
		storeAddress = static_cast<int>(static_cast<int64_t>(src2) + inst.decoded.imm);
		storeValue = src1;
		return 0.0;
	case OP_BNE:
		causesBranch = true;
		branchNextPc = (static_cast<int64_t>(src1) != static_cast<int64_t>(src2)) ? inst.decoded.targetPc : (inst.decoded.pc + 4);
		return 0.0;
	default:
		return 0.0;
	}
}

// Branch recovery: squash wrong-path work and restore state from the branch snapshot.
int Simulator::flushAfterBranch(int branchRobIndex)
{
	auto subtractRenamedStalls = [this](const RenamedInstruction& inst) -> void
	{
		rsStalls -= inst.rsStallCount;
		robStalls -= inst.robStallCount;
	};

	std::vector<bool> flushed(configuration->NR, false);
	int flushedCount = 0;
	int idx = (branchRobIndex + 1) % configuration->NR;
	// Everything younger than the resolved branch is wrong-path work.
	while (idx != robTail)
	{
		flushed[idx] = true;
		rsStalls -= rob[idx].rsStallCount;
		robStalls -= rob[idx].robStallCount;
		rob[idx] = RobEntry();
		++flushedCount;
		idx = (idx + 1) % configuration->NR;
	}
	robTail = (branchRobIndex + 1) % configuration->NR;
	robCount = (robTail - robHead + configuration->NR) % configuration->NR;
	if (robHead == robTail && rob[robHead].busy)
	{
		robCount = configuration->NR;
	}

	RobEntry& branch = rob[branchRobIndex];
	// Restore RAT and free-list from the snapshot taken when the branch was decoded.
	registerMapTable = branch.ratSnapshot;

	freeList = branch.freeListSnapshot;
	std::vector<bool> freeRegSeen(NUM_PHYS_REG, false);
	for (size_t i = 0; i < freeList.size(); ++i)
	{
		freeRegSeen[freeList[i]] = true;
		setPhysReady(freeList[i], true);
		if (!branch.regValueSnapshot.empty())
		{
			registerFile[freeList[i]].value = branch.regValueSnapshot[freeList[i]];
		}
	}

	// Replay commit-time register releases that happened after the branch checkpoint.
	for (size_t i = branch.freeListHistorySize; i < committedFreeListHistory.size(); ++i)
	{
		const ReleasedPhysReg& released = committedFreeListHistory[i];
		const int phys = released.phys;
		if (phys < 0 || phys >= NUM_PHYS_REG || freeRegSeen[phys])
		{
			continue;
		}
		freeList.push_back(phys);
		freeRegSeen[phys] = true;
		setPhysReady(phys, true);
		registerFile[phys].value = released.value;
	}

	auto clearFlushedRs = [&flushed](std::vector<RsEntry>& rs) -> void
	{
		for (size_t i = 0; i < rs.size(); ++i)
		{
			if (rs[i].busy && flushed[rs[i].inst.robIndex])
			{
				rs[i] = RsEntry();
			}
		}
	};
	clearFlushedRs(rsInt);
	clearFlushedRs(rsLoad);
	clearFlushedRs(rsStore);
	clearFlushedRs(rsFpAdd);
	clearFlushedRs(rsFpMul);
	clearFlushedRs(rsFpDiv);
	clearFlushedRs(rsBu);

	std::vector<CdbEvent> kept;
	for (size_t i = 0; i < pendingCdbEvents.size(); ++i)
	{
		// Drop broadcasts from squashed instructions before they can wake dependents.
		if (!flushed[pendingCdbEvents[i].robIndex])
		{
			kept.push_back(pendingCdbEvents[i]);
		}
	}
	pendingCdbEvents.swap(kept);

	std::vector<BranchResolutionEvent> keptBranches;
	for (size_t i = 0; i < pendingBranchEvents.size(); ++i)
	{
		if (!flushed[pendingBranchEvents[i].robIndex])
		{
			keptBranches.push_back(pendingBranchEvents[i]);
		}
	}
	pendingBranchEvents.swap(keptBranches);

	for (size_t i = 0; i < decodeQueue.size(); ++i)
	{
		subtractRenamedStalls(decodeQueue[i]);
	}
	decodeQueue.clear();
	fetchQueue.clear();
	// Resume fetch from the branch's resolved target PC.
	fetchPc = branch.actualNextPc;
	fetchHalted = false;
	frontEndBlockedOnBranch = false;
	frontEndBlockingBranchTraceSeq = -1;
	rebuildFetchSpecFromMap();
	return flushedCount;
}

// Execute: count down latency; on completion update ROB and enqueue CDB events.
void Simulator::Execute()
{
	auto advanceRs = [this](std::vector<RsEntry>& rs) -> void
	{
		for (size_t i = 0; i < rs.size(); ++i)
		{
			if (!rs[i].busy || !rs[i].started || rs[i].remainingCycles <= 0)
			{
				continue;
			}
			--rs[i].remainingCycles;
			if (rs[i].remainingCycles != 0)
			{
				continue;
			}

			bool hasResult = false;
			bool causesBranch = false;
			int branchNextPc = rs[i].inst.decoded.pc + 4;
			bool isStore = false;
			int storeAddress = -1;
			double storeValue = 0.0;
			double result = executeInstruction(rs[i].inst, hasResult, causesBranch, branchNextPc, isStore, storeAddress, storeValue);

			RobEntry& entry = rob[rs[i].inst.robIndex];
			if (isStore)
			{
				// Store writes address and data into ROB; CDB/writeback marks it ready to commit.
				entry.storeAddress = storeAddress;
				entry.storeValue = storeValue;
				entry.storeAddressReady = true;
				entry.storeValueReady = true;
			}
			if (causesBranch)
			{
				entry.actualNextPc = branchNextPc;
				entry.mispredicted = (entry.predictedNextPc != branchNextPc);
			}
			if (hasResult || isStore || causesBranch)
			{
				if (hasResult)
				{
					entry.value = result;
				}
				if (causesBranch)
				{
					BranchResolutionEvent event;
					event.robIndex = rs[i].inst.robIndex;
					event.traceSeq = rs[i].inst.traceSeq;
					event.seq = rs[i].seq;
					pendingBranchEvents.push_back(event);
				}
				else
				{
					CdbEvent event;
					event.robIndex = rs[i].inst.robIndex;
					event.destPhys = hasResult ? rs[i].inst.dstPhys : -1;
					event.fuType = rs[i].inst.fuType;
					event.rsSlot = static_cast<int>(i);
					event.seq = rs[i].seq;
					pendingCdbEvents.push_back(event);
				}
			}
		}
	};

	advanceRs(rsInt);
	advanceRs(rsLoad);
	advanceRs(rsStore);
	advanceRs(rsFpAdd);
	advanceRs(rsFpMul);
	advanceRs(rsFpDiv);
	advanceRs(rsBu);
}

// Writeback: broadcast up to NB CDB results per cycle and wake dependents.
void Simulator::Writeback()
{
	if (!pendingBranchEvents.empty())
	{
		// Resolve branches before CDB broadcasts so wrong-path events do not write back.
		std::sort(pendingBranchEvents.begin(), pendingBranchEvents.end(), [](const BranchResolutionEvent& a, const BranchResolutionEvent& b) -> bool
		{
			return a.seq < b.seq;
		});

		while (!pendingBranchEvents.empty())
		{
			const BranchResolutionEvent event = pendingBranchEvents.front();
			pendingBranchEvents.erase(pendingBranchEvents.begin());
			RobEntry& entry = rob[event.robIndex];
			if (frontEndBlockedOnBranch && frontEndBlockingBranchTraceSeq == event.traceSeq)
			{
				frontEndBlockedOnBranch = false;
				frontEndBlockingBranchTraceSeq = -1;
			}
			const bool branchTaken = (entry.actualNextPc != entry.pc + 4);
			updateBranchPredictor(entry.pc, branchTaken, entry.branchTargetPc);
			if (entry.mispredicted)
			{
				(void)flushAfterBranch(event.robIndex);
			}
			entry.done = true;
			for (size_t i = 0; i < rsBu.size(); ++i)
			{
				if (rsBu[i].busy && rsBu[i].inst.robIndex == event.robIndex && rsBu[i].remainingCycles == 0)
				{
					rsBu[i] = RsEntry();
				}
			}
		}
	}

	if (pendingCdbEvents.empty())
	{
		return;
	}

	auto cdbPriority = [](FuType fu) -> int
	{
		switch (fu)
		{
		case FU_LS_LOAD:
			return 0;
		case FU_INT:
			return 1;
		case FU_FP_ADD:
			return 2;
		case FU_FP_MUL:
			return 3;
		case FU_FP_DIV:
			return 4;
		case FU_LS_STORE:
			return 5;
		case FU_BU:
			return 6;
		}
		return 7;
	};

	std::sort(pendingCdbEvents.begin(), pendingCdbEvents.end(), [cdbPriority](const CdbEvent& a, const CdbEvent& b) -> bool
	{
		const int pa = cdbPriority(a.fuType);
		const int pb = cdbPriority(b.fuType);
		if (pa != pb)
		{
			return pa < pb;
		}
		if (a.rsSlot != b.rsSlot)
		{
			return a.rsSlot < b.rsSlot;
		}
		return a.seq < b.seq;
	});

	const int limit = std::min(configuration->NB, static_cast<int>(pendingCdbEvents.size()));
	auto wake = [](std::vector<RsEntry>& rs, int phys) -> void
	{
		if (phys < 0)
		{
			return;
		}
		for (size_t i = 0; i < rs.size(); ++i)
		{
			if (!rs[i].busy)
			{
				continue;
			}
			if (rs[i].inst.decoded.hasSrc1 && !rs[i].inst.src1Ready && rs[i].inst.src1Phys == phys)
			{
				rs[i].inst.src1Ready = true;
			}
			if (rs[i].inst.decoded.hasSrc2 && !rs[i].inst.src2Ready && rs[i].inst.src2Phys == phys)
			{
				rs[i].inst.src2Ready = true;
			}
		}
	};

	auto clearCompleted = [](std::vector<RsEntry>& rs, int robIndex) -> void
	{
		for (size_t i = 0; i < rs.size(); ++i)
		{
			if (rs[i].busy && rs[i].inst.robIndex == robIndex && rs[i].remainingCycles == 0)
			{
				rs[i] = RsEntry();
			}
		}
	};

	int processed = 0;
	while (processed < limit && !pendingCdbEvents.empty())
	{
		const CdbEvent event = pendingCdbEvents.front();
		pendingCdbEvents.erase(pendingCdbEvents.begin());
		// Broadcast values and wake RS entries waiting on this destination phys reg.
		RobEntry& entry = rob[event.robIndex];
		if (event.destPhys >= 0)
		{
			writePhysValue(event.destPhys, entry.value);
			setPhysReady(event.destPhys, true);
		}
		entry.done = true;

		wake(rsInt, event.destPhys);
		wake(rsLoad, event.destPhys);
		wake(rsStore, event.destPhys);
		wake(rsFpAdd, event.destPhys);
		wake(rsFpMul, event.destPhys);
		wake(rsFpDiv, event.destPhys);
		wake(rsBu, event.destPhys);

		clearCompleted(rsInt, event.robIndex);
		clearCompleted(rsLoad, event.robIndex);
		clearCompleted(rsStore, event.robIndex);
		clearCompleted(rsFpAdd, event.robIndex);
		clearCompleted(rsFpMul, event.robIndex);
		clearCompleted(rsFpDiv, event.robIndex);
		clearCompleted(rsBu, event.robIndex);
		++processed;
	}
	// Track CDB utilization for bottleneck analysis.
	cdbWrites += processed;
	cdbCapacityUsed += configuration->NB;
}

// Commit: retire in order, perform stores, recycle old physical registers.
void Simulator::Commit()
{
	int committed = 0;
	while (committed < configuration->NC && robCount > 0)
	{
		RobEntry& head = rob[robHead];
		if (!head.busy || !head.done)
		{
			break;
		}
		if (head.isStore && head.storeAddressReady && head.storeValueReady)
		{
			// Stores write memory only at commit for precise, recoverable state.
			storeToMemory(head.storeAddress, head.storeValue);
		}
		if (head.hasDest)
		{
			if (head.oldPhys >= 0 && head.oldPhys < NUM_PHYS_REG)
			{
				// The previous physical destination becomes reusable only after in-order commit.
				freeList.push_back(head.oldPhys);
				ReleasedPhysReg released;
				released.phys = head.oldPhys;
				released.value = registerFile[head.oldPhys].value;
				committedFreeListHistory.push_back(released);
				setPhysReady(head.oldPhys, true);
			}
		}
		rob[robHead] = RobEntry();
		robHead = (robHead + 1) % configuration->NR;
		--robCount;
		++committed;
	}
}

// Print statistics to stdout.
void Simulator::printStats()
{
	std::cout << "Total CCs: " << cycle << std::endl;
	std::cout << "RS Stalls: " << rsStalls << std::endl;
	std::cout << "ROB Stalls: " << robStalls << std::endl;
}

// Serialize final memory, registers, and stats to JSON.
void Simulator::serializeJSON(std::ofstream* output)
{
	nlohmann::json j;
	j["total_cycles"] = cycle;
	j["rs_stalls"] = rsStalls;
	j["rob_stalls"] = robStalls;
	j["cdb_writes"] = cdbWrites;
	j["cdb_cycles_capacity"] = cdbCapacityUsed;
	for (int i = 0; i < MAX_MEM_SIZE; i++)
	{
		j["data_memory"][i] = dataMemory[i];
	}
	for (int i = 0; i < NUM_PHYS_REG; i++)
	{
		j["register_file"][i] = registerFile[i].value;
	}
	*output << j.dump(4) << std::endl;
}
