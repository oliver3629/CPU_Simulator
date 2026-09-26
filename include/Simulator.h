
#include <climits>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <sys/types.h>
#include <vector>

#include "Config.h"
#include "json.hpp"
using namespace nlohmann::literals;

#ifndef SIMULATOR_H
#define SIMULATOR_H

#define MAX_MEM_SIZE 1024
#define NUM_PHYS_REG 32
#define NUM_PHYS_REG_INCLUDING_X0 NUM_PHYS_REG + 1 // We will use the last register as a "dummy" register to represent the value of X0, which is always 0

enum DebugArg
{
	DEBUG_DCACHE,
	DEBUG_REGISTERS
};

struct ArchitecturalRegister
{
	enum
	{
		X,
		F
	} type;
	int32_t num;

	bool operator<(const ArchitecturalRegister& o) const
	{
		return (type < o.type) || (type == o.type && num < o.num);
	}
};

struct RegisterFileEntry
{
	double value = 0.0;
};

class XLSXEventLogger;

class Simulator
{
public:
	/**
	 * @brief Simulator Configuration
	 */
	Config *configuration;

	/**
	 * @brief Data Memory ($D Cache)
	 * On startup, all values are initialized to 0
	 */
	double dataMemory[MAX_MEM_SIZE] = {0};

	/**
	 * @brief Register File
	 * @note This is a map of physical register number -> value
	 * @note For bookkeeping, we also store the ROB number and whether the register is busy
	 */
	RegisterFileEntry registerFile[NUM_PHYS_REG_INCLUDING_X0];
	std::map<ArchitecturalRegister, int> registerMapTable;

	// *PUBLIC --------------------
	/**
	 * @brief Constructor for Simulator that uses custom configuration
	 * @param program Pointer to ifstream object containing the program
	 * @param c Configuration struct
	 */
	Simulator(std::ifstream* program, Config *c);
	void setXlsxLogger(XLSXEventLogger* logger);

	/**
	 * @brief Run the full simulation until the pipeline drains
	 */
	void run();

	/**
	 * @brief Debugging function
	 * @param argc Number of arguments excluding argc
	 * @param ... Variable arguments - see enum DebugArg
	 * 
	 * @example this->dump(1, DEBUG_ALL);
	 */
	void dump(int argc...);
	void printStats();

	/**
	 * @brief Serialize the simulator state to a JSON file
	 * @param output Pointer to ofstream object where the serialized data is written
	 */
	void serializeJSON(std::ofstream* output);

private:
	enum Opcode
	{
		OP_INVALID,
		OP_FLD,
		OP_FSD,
		OP_ADD,
		OP_ADDI,
		OP_SLT,
		OP_FADD,
		OP_FSUB,
		OP_FMUL,
		OP_FDIV,
		OP_BNE
	};

	enum FuType
	{
		FU_INT,
		FU_LS_LOAD,
		FU_LS_STORE,
		FU_FP_ADD,
		FU_FP_MUL,
		FU_FP_DIV,
		FU_BU
	};

	struct DecodedInstruction
	{
		Opcode opcode = OP_INVALID;
		int pc = 0;
		int nextPc = 0;
		std::string rawText;
		bool hasDst = false;
		ArchitecturalRegister dst = {ArchitecturalRegister::X, 0};
		bool hasSrc1 = false;
		ArchitecturalRegister src1 = {ArchitecturalRegister::X, 0};
		bool hasSrc2 = false;
		ArchitecturalRegister src2 = {ArchitecturalRegister::X, 0};
		int imm = 0;
		int targetPc = 0;
	};

	struct RenamedInstruction
	{
		DecodedInstruction decoded;
		FuType fuType = FU_INT;
		int traceSeq = -1;
		int src1Phys = NUM_PHYS_REG;
		bool src1Ready = false;
		int src2Phys = NUM_PHYS_REG;
		bool src2Ready = false;
		int dstPhys = -1;
		int oldDstPhys = -1;
		int robIndex = -1;
		int rsStallCount = 0;
		int robStallCount = 0;
		bool hasBranchSnapshot = false;
		std::map<ArchitecturalRegister, int> branchRatSnapshot;
		std::deque<int> branchFreeListSnapshot;
		size_t branchFreeListHistorySize = 0;
		std::vector<double> branchRegValueSnapshot;
	};

	struct FetchPacket
	{
		int traceSeq = -1;
		int pc = 0;
		int predictedNextPc = 0;
	};

	struct RsEntry
	{
		bool busy = false;
		RenamedInstruction inst;
		int remainingCycles = -1;
		bool started = false;
		int dispatchCycle = -1;
		int seq = -1;
	};

	struct RobEntry
	{
		bool busy = false;
		bool done = false;
		int pc = 0;
		int rsStallCount = 0;
		int robStallCount = 0;
		int predictedNextPc = 0;
		int actualNextPc = 0;
		int branchTargetPc = 0;
		Opcode robOpcode = OP_INVALID;
		bool isStore = false;
		bool isBranch = false;
		bool hasDest = false;
		ArchitecturalRegister archDest = {ArchitecturalRegister::X, 0};
		int oldPhys = -1;
		double value = 0.0;
		int storeAddress = -1;
		double storeValue = 0.0;
		bool storeAddressReady = false;
		bool storeValueReady = false;
		bool mispredicted = false;
		std::map<ArchitecturalRegister, int> ratSnapshot;
		std::deque<int> freeListSnapshot;
		size_t freeListHistorySize = 0;
		std::vector<double> regValueSnapshot;
	};

	struct CdbEvent
	{
		int robIndex = -1;
		int destPhys = -1;
		FuType fuType = FU_INT;
		int rsSlot = -1;
		int seq = -1;
	};

	struct BranchResolutionEvent
	{
		int robIndex = -1;
		int traceSeq = -1;
		int seq = -1;
	};

	struct BranchPredictorEntry
	{
		bool valid = false;
		int tagPc = 0;
		int targetPc = 0;
		int counter = 1;
	};

	struct ReleasedPhysReg
	{
		int phys = -1;
		double value = 0.0;
	};

	struct RobTraceSlot
	{
		bool busy = false;
		Opcode robOpcode = OP_INVALID;
		bool hasDest = false;
		ArchitecturalRegister archDest = {ArchitecturalRegister::X, 0};
		double value = 0.0;
		int actualNextPc = 0;
	};

	void parseProgram(std::ifstream* program);
	DecodedInstruction parseInstructionLine(const std::string& line, int pc);
	void initializeState();
	void snapshotRsForTrace();
	bool hasInFlightWork() const;
	int btbIndexForPc(int pc) const;
	bool predictBranch(int pc, int& predictedNextPc) const;
	void updateBranchPredictor(int pc, bool taken, int targetPc);
	FuType classifyFu(const DecodedInstruction& decoded) const;
	int latencyFor(const DecodedInstruction& decoded) const;
	int getMappedPhys(const ArchitecturalRegister& reg) const;
	double readPhysValue(int phys) const;
	void writePhysValue(int phys, double value);
	bool isPhysReady(int phys) const;
	void setPhysReady(int phys, bool ready);
	void rebuildFetchSpecFromMap();
	RenamedInstruction decodeStageRename(const DecodedInstruction& decoded, bool& ok);
	void applyFetchSpecUpdate(const DecodedInstruction& decoded);
	bool tryEvaluateFetchBranch(const DecodedInstruction& decoded, bool& branchTaken) const;
	void Fetch();
	void Decode();
	bool allocateRobEntry(RenamedInstruction& inst);
	bool tryDispatchToRS(const RenamedInstruction& inst);
	void Issue();
	double loadFromMemory(int address) const;
	void storeToMemory(int address, double value);
	double executeInstruction(const RenamedInstruction& inst, bool& hasResult, bool& causesBranch, int& branchNextPc,
		bool& isStore, int& storeAddress, double& storeValue);
	int flushAfterBranch(int branchRobIndex);
	void Execute();
	void Writeback();
	void Commit();
	void printTraceInitial();
	void printTraceCycle();

	std::vector<DecodedInstruction> programInstructions;
	std::map<std::string, int> labelToPc;
	std::deque<int> freeList;
	std::vector<ReleasedPhysReg> committedFreeListHistory;
	std::vector<bool> physReady;
	std::vector<RobEntry> rob;
	int robHead = 0;
	int robTail = 0;
	int robCount = 0;
	std::vector<RsEntry> rsInt;
	std::vector<RsEntry> rsLoad;
	std::vector<RsEntry> rsStore;
	std::vector<RsEntry> rsFpAdd;
	std::vector<RsEntry> rsFpMul;
	std::vector<RsEntry> rsFpDiv;
	std::vector<RsEntry> rsBu;
	std::vector<CdbEvent> pendingCdbEvents;
	std::vector<BranchResolutionEvent> pendingBranchEvents;
	std::deque<FetchPacket> fetchQueue;
	std::deque<RenamedInstruction> decodeQueue;
	std::vector<double> fetchSpecXValues;
	std::vector<bool> fetchSpecXReady;
	int fetchPc = 0;
	bool fetchHalted = false;
	bool simulationDone = false;
	bool frontEndBlockedOnBranch = false;
	int frontEndBlockingBranchTraceSeq = -1;
	int cycle = 0;
	int rsStalls = 0;
	int robStalls = 0;
	int cdbWrites = 0;
	int cdbCapacityUsed = 0;
	int instructionSeq = 0;
	int fetchTraceSeq = 1;
	BranchPredictorEntry btb[16];
	XLSXEventLogger* xlsxLogger = NULL;
	std::vector<RsEntry> traceRsInt;
	std::vector<RsEntry> traceRsLoad;
	std::vector<RsEntry> traceRsStore;
	std::vector<RsEntry> traceRsFpAdd;
	std::vector<RsEntry> traceRsFpMul;
	std::vector<RsEntry> traceRsFpDiv;
	std::vector<RsEntry> traceRsBu;
	std::vector<RobTraceSlot> traceRob;
	int traceRobHead = 0;
	int traceRobTail = 0;
};

#endif // SIMULATOR_H
