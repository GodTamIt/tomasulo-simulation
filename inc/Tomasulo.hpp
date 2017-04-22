#pragma once
#include <deque>
#include <memory>
#include <functional>
#include <tuple>
#include <stdint.h>

#include "types.hpp"
#include "Gselect.hpp"

using namespace std;

namespace pipeline {
	class Tomasulo {
		public:
			Tomasulo(ProcessorSettings settings);
			ProcessorSettings getSettings() const;
			Statistics getStatistics() const;
			void reset();
			void run(deque<shared_ptr<Instruction>>& instruction_q);

		private:
			typedef intmax_t tag_t;

			typedef struct ReservationStation {
				bool fired;

				tag_t target_tag;
				vector<tag_t> source_tags;
				vector<bool> sources_ready;

				shared_ptr<Instruction> instr;
			} ReservationStation;

			typedef struct FunctionUnit {
				uint_fast32_t latency;
				clock_cycle_t enter_cycle;

				shared_ptr<ReservationStation> station;

				bool operator <(const FunctionUnit& rhs) const {
					return tie(enter_cycle, station->target_tag) < tie(rhs.enter_cycle, rhs.station->target_tag);
				}

				bool operator >(const FunctionUnit& rhs) const {
					return tie(enter_cycle, station->target_tag) > tie(rhs.enter_cycle, rhs.station->target_tag);
				}

				bool operator ==(const FunctionUnit& rhs) const {
					return enter_cycle == rhs.enter_cycle && station == rhs.station;
				}
			} FunctionUnit;

			typedef struct ResultBus {
				regno_t reg;
				tag_t tag;

				shared_ptr<ReservationStation> station;

				bool operator <(const ResultBus& rhs) const {
					return tag < rhs.tag;
				}

				bool operator >(const ResultBus& rhs) const {
					return tag > rhs.tag;
				}

				bool operator ==(const ResultBus& rhs) const {
					return tag == rhs.tag && reg == rhs.reg;
				}
			} ResultBus;

			typedef struct Register {
				tag_t tag;
			} Register;

			ProcessorSettings settings_;
			Statistics stats_;
			Gselect branch_pred_;

			vector<Register> register_file;
			deque<shared_ptr<Instruction>> fetch_q_;
			deque<shared_ptr<Instruction>> dispatch_q_;
			vector<shared_ptr<ReservationStation>> schedule_q_;
			vector<vector<FunctionUnit>> function_units_;
			vector<ResultBus> busy_result_buses_;
			vector<ResultBus> retire_buffer_;

			size_t schedule_q_limit_;
			clock_cycle_t current_clock_;
			tag_t current_tag_;
			shared_ptr<Instruction> bad_branch_instr_;

			uint_least64_t hashAddress(uint_least64_t addr);
			void fetchInstructions(deque<shared_ptr<Instruction>>& instruction_q, deque<shared_ptr<Instruction>>& result_q);
			bool isPipelineEmpty() const;
			void dispatchInstructions();
			void scheduleInstructions();
			void fireInstructions();
			void retireInstructions();
			void updateState();
			void sweepRetirementBuffer();
			tag_t getNewTag();
	};
}