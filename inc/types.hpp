#pragma once
#include <stdint.h>
#include <vector>
#include <memory>

using namespace std;

typedef int_least32_t regno_t;
typedef uintmax_t clock_cycle_t;

typedef struct ProcessorSettings {
	uintmax_t result_bus_count;
	uintmax_t fetch_rate;

	vector<uint_least16_t> function_units_count;
	vector<uint_least16_t> function_units_latency;

	uintmax_t register_count;

	unsigned int ghr_bits;
	uintmax_t ghr_init_val;
	unsigned int predictor_bits;
	uintmax_t predictor_init_val;
	size_t predictor_table_size;
} ProcessorSettings;

typedef struct InstructionLife {
	clock_cycle_t fetch_cycle;
	clock_cycle_t dispatch_cycle;
	clock_cycle_t schedule_cycle;
	clock_cycle_t execute_cycle;
	clock_cycle_t state_update_cycle;
} InstructionLife;

typedef struct Instruction {
	uintmax_t number;

	uint_least64_t address;
	int_least16_t func_type;

	regno_t dst_reg;
	vector<regno_t> src_regs;

	uint_least64_t branch_address;
	bool branch_taken;
	bool is_branch;

	shared_ptr<InstructionLife> life;
} Instruction;

typedef struct Statistics {
	uintmax_t instructions;
	clock_cycle_t clock_cycles;

	uintmax_t instr_dispatched;
	uintmax_t instr_scheduled;
	uintmax_t instr_fired;
	uintmax_t instr_executed;
	uintmax_t instr_retired;

	uintmax_t peak_dispatch_size;
	uintmax_t dispatch_size_sum;

	uintmax_t branches;
	uintmax_t correct_branches;

} Statistics;