#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <getopt.h>
#include <inttypes.h>
#include <queue>
#include <assert.h>

#include "types.hpp"
#include "Tomasulo.hpp"

using namespace std;


void exitUsage()
{
	cout << "Usage:" << endl;
	cout << "\tprocsim -r result_bus_count -f fetch_rate -j k0_func_units -k k1_func_units -l k2_func_units" << endl;
	exit(EXIT_SUCCESS);
}

ProcessorSettings parseArguments(int argc, char* argv[]) {
	ProcessorSettings opts = {};
	
	// Set default values from PDF
	opts.register_count = UINTMAX_C(128);
	
	opts.function_units_count.resize(3, 0);
	opts.function_units_latency.resize(3, 1);

	opts.ghr_bits = 3u;
	opts.ghr_init_val = UINTMAX_C(0b000);
	opts.predictor_bits = 2u;
	opts.predictor_init_val = UINTMAX_C(0b01);
	opts.predictor_table_size = 128;

	int c;
	while ((c = getopt(argc, argv,"r:f:j:k:l:h")) != -1) {
		switch (c) {
			case 'r':
				opts.result_bus_count = stoull(optarg);
				break;
			case 'f':
				opts.fetch_rate = stoull(optarg);
				break;
			case 'j':
				opts.function_units_count[0] = stoull(optarg);
				break;
			case 'k':
				opts.function_units_count[1] = stoull(optarg);
				break;
			case 'l':
				opts.function_units_count[2] = stoull(optarg);
				break;
			default:
				exitUsage();
		}
	}

	if (opts.result_bus_count == 0 || opts.fetch_rate == 0 || opts.function_units_count.size() != opts.function_units_latency.size())
		exitUsage();

	for (auto count : opts.function_units_count) {
		if (count == 0)
			exitUsage();
	}
	

	return opts;
}

void printSettings(const ProcessorSettings& opts) {
	cout << "Processor Settings" << endl;
	cout << "R: " << opts.result_bus_count << endl;
	cout << "k0: " << static_cast<unsigned>(opts.function_units_count[0]) << endl;
	cout << "k1: " << static_cast<unsigned>(opts.function_units_count[1]) << endl;
	cout << "k2: " << static_cast<unsigned>(opts.function_units_count[2]) << endl;
	cout << "F: " << opts.fetch_rate << endl;
	cout << endl;
}

shared_ptr<Instruction> parseInstruction(const string& line) {
	shared_ptr<Instruction> instr = make_shared<Instruction>();

	int_least32_t dst_reg = -1, src1_reg = -1, src2_reg = -1;
	int branch_taken;

	int read = sscanf(line.c_str(), "%" SCNxLEAST64 " %" SCNdLEAST16 " %" SCNdLEAST32 " %" SCNdLEAST32 " %" SCNdLEAST32 " %" SCNxLEAST64 " %d", &instr->address, &instr->func_type, &dst_reg, &src1_reg, &src2_reg, &instr->branch_address, &branch_taken);
	
	if (instr->func_type == -1)
		instr->func_type = 1;

	instr->branch_taken = (bool)branch_taken;

	if (read == 7)
		instr->is_branch = true;
	else if (read != 5) {
		cout << "Error: unreadable format '" << line << "'" << endl;
		exit(EXIT_FAILURE);
	}

	instr->dst_reg = dst_reg;
	instr->src_regs.push_back(src1_reg);
	instr->src_regs.push_back(src2_reg);

	return instr;
}

void printInstructionLives(deque<shared_ptr<Instruction>>& instrs) {
	cout << "INST\tFETCH\tDISP\tSCHED\tEXEC\tSTATE" << endl;
	for (size_t i = 0; i < instrs.size(); i++) {
		auto life = instrs[i]->life;
		
		cout << (i + 1)                  << "\t";
		cout << life->fetch_cycle        << "\t";
		cout << life->dispatch_cycle     << "\t";
		cout << life->schedule_cycle     << "\t";
		cout << life->execute_cycle      << "\t";
		cout << life->state_update_cycle << "\t";
		cout << endl;
	}
}

void printStatistics(Statistics const& stats) {
	// Additional calculations
	double avg_dispatch_size = (double)stats.dispatch_size_sum / (double)stats.clock_cycles;
	double avg_fired_instr = (double)stats.instr_fired / (double)stats.clock_cycles;
	double avg_retired_instr = (double)stats.instr_retired / (double)stats.clock_cycles;

	double pred_accuracy = (double)stats.correct_branches / (double)stats.branches;

	cout << endl;
	cout << "Processor stats:" << endl;
	cout << "Total branch instructions: " << stats.branches << endl;
	cout << "Total correct predicted branch instructions: " << stats.correct_branches << endl;
	cout << "prediction accuracy: " << pred_accuracy << endl;
	cout << "Avg Dispatch queue size: " << avg_dispatch_size << endl;
	cout << "Maximum Dispatch queue size: " << stats.peak_dispatch_size << endl;
	cout << "Avg inst Issue per cycle: " << avg_fired_instr << endl;
	cout << "Avg inst retired per cycle: " << avg_retired_instr << endl;
	cout << "Total run time (cycles): " << stats.clock_cycles << endl;
}

int main(int argc, char** argv) {
	// Parse arguments and print out the settings
	ProcessorSettings settings = parseArguments(argc, argv);
	printSettings(settings);

	// Read the entire stdin into memory
	deque<shared_ptr<Instruction>> instrs;
	string line;
	for (uintmax_t i = 1; getline(cin, line); ++i) {
		auto instr = parseInstruction(line);
		instr->number = i;
		instrs.push_back(instr);
	}

	// Create the pipeline
	pipeline::Tomasulo ooe_pipeline(settings);

	ooe_pipeline.run(instrs);

	// Print out the per-instruction details (per-stage)
	printInstructionLives(instrs);


	// Print out statistics
	printStatistics(ooe_pipeline.getStatistics());
}