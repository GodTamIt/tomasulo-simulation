#include <algorithm>
#include <assert.h>
#include <queue>

#include "Tomasulo.hpp"


namespace pipeline {
	Tomasulo::Tomasulo(ProcessorSettings settings) :
		branch_pred_(settings.ghr_bits, settings.ghr_init_val, settings.predictor_bits, settings.predictor_init_val, settings.predictor_table_size) 
	{
		settings_ = settings;

		// Calculate the size of schedule queue
		schedule_q_limit_ = 0;
		for (auto count : settings_.function_units_count)
			schedule_q_limit_ += count;
		schedule_q_limit_ *= 2;

		register_file.resize(settings_.register_count);
		function_units_.resize(settings_.function_units_count.size());

		reset();
	}

	ProcessorSettings Tomasulo::getSettings() const {
		return settings_;
	}

	Statistics Tomasulo::getStatistics() const {
		return stats_;
	}

	void Tomasulo::reset() {
		stats_ = {};
		current_clock_ = 0;
		current_tag_ = 1;

		for (auto& reg : register_file)
			reg.tag = -1;
		

		for (auto& units : function_units_)
			units.clear();
	}


	uint_least64_t Tomasulo::hashAddress(uint_least64_t addr) {
		return addr >> 2;
	}


	void Tomasulo::run(deque<shared_ptr<Instruction>>& instruction_q) {
		deque<shared_ptr<Instruction>> result_q;

		while (!instruction_q.empty() || !isPipelineEmpty()) {
			++current_clock_;

			// ~~ 1ST HALF ~~
			updateState();

			busy_result_buses_.swap(retire_buffer_);

			retireInstructions();

			fireInstructions();

			scheduleInstructions();

			dispatchInstructions();

			fetchInstructions(instruction_q, result_q);


			sweepRetirementBuffer();

			// STATS: update total clock cycles
			++stats_.clock_cycles;
		}

		// Fix off-by-one for last instruction in the pipeline
		--current_clock_;
		--stats_.clock_cycles;

		// Swap the results into the parameter passed in
		instruction_q.swap(result_q);

		// Clean up some memory usage
		schedule_q_.shrink_to_fit();
		busy_result_buses_.shrink_to_fit();
		result_q.shrink_to_fit();
	}

	bool Tomasulo::isPipelineEmpty() const {
		return fetch_q_.empty() && dispatch_q_.empty() && schedule_q_.empty() && busy_result_buses_.empty();
	}

	void Tomasulo::fetchInstructions(deque<shared_ptr<Instruction>>& instruction_q, deque<shared_ptr<Instruction>>& result_q) {
		for (size_t i = 0; i < settings_.fetch_rate && !instruction_q.empty(); ++i) {
			// Grab instruction from the front of the queue
			auto instr = instruction_q.front();
			instruction_q.pop_front();

			// Make a new life for the instruction
			instr->life = make_shared<InstructionLife>();

			// Record when the instruction gets to fetch
			instr->life->fetch_cycle = current_clock_;

			fetch_q_.push_back(instr);
			result_q.push_back(instr);

			// STATS: instruction total
			++stats_.instructions;
		}
	}

	void Tomasulo::dispatchInstructions() {
		for (size_t i = 0; i < settings_.fetch_rate && bad_branch_instr_ == nullptr && !fetch_q_.empty(); ++i) {
			shared_ptr<Instruction> instr = fetch_q_.front();
			fetch_q_.pop_front();

			instr->life->dispatch_cycle = current_clock_;

			// Stall dispatch stage
			if (instr->is_branch) {
				// STATS: update branch prediction
				++stats_.branches;

				if (instr->branch_taken != branch_pred_.predict(hashAddress(instr->address))) {
					bad_branch_instr_ = instr;
				}
				else {
					// STATS: update number of correct branches
					++stats_.correct_branches;
				}
			}


			dispatch_q_.push_back(instr);

			// STATS: dispatch total
			++stats_.instr_dispatched;
		}

		// STATS: dispatch size
		uintmax_t dispatch_size = (uintmax_t)dispatch_q_.size();
		stats_.peak_dispatch_size = max(stats_.peak_dispatch_size, dispatch_size);
		stats_.dispatch_size_sum += dispatch_size;
	}

	void Tomasulo::scheduleInstructions() {
		while (schedule_q_.size() < schedule_q_limit_ && !dispatch_q_.empty()) {
			shared_ptr<Instruction> instr = dispatch_q_.front();
			dispatch_q_.pop_front();

			// Mark down the schedule time
			instr->life->schedule_cycle = current_clock_;

			auto station = make_shared<ReservationStation>();
			station->instr = instr;
			station->fired = false;

			for (auto const& src_reg : instr->src_regs) {
				if (src_reg < 0 || register_file[src_reg].tag < 0) {
					// Register file is updated and can be read directly
					station->source_tags.push_back(-1);
					station->sources_ready.push_back(true);
				}
				else {
					// Need to await instruction to publish a result
					station->source_tags.push_back(register_file[src_reg].tag);
					station->sources_ready.push_back(false);
				}
			}

			// Set destination tag
			tag_t unique_tag = getNewTag();
			station->target_tag = unique_tag;
			if (instr->dst_reg >= 0) {
				register_file[instr->dst_reg].tag = unique_tag;
			}

			// Add to the scheduling queue
			schedule_q_.push_back(station);


			// STATS: scheduled total
			++stats_.instr_scheduled;
		}
	}

	void Tomasulo::fireInstructions() {
		for (auto station : schedule_q_) {
			shared_ptr<Instruction> instr = station->instr;

			// Don't fire if it's already fired
			if (station->fired)
				continue;

			if (function_units_[instr->func_type].size() >= settings_.function_units_count[instr->func_type])
				continue;

			bool all_ready = true;
			for (auto const& ready : station->sources_ready)
				all_ready &= ready;

			// Don't fire if all the tags aren't ready
			if (!all_ready)
				continue;

			// Fire away
			FunctionUnit unit;
			unit.latency = settings_.function_units_latency[instr->func_type];
			unit.enter_cycle = current_clock_;
			unit.station = station;

			function_units_[instr->func_type].push_back(unit);

			station->fired = true;

			// Record when the instruction gets to execute
			instr->life->execute_cycle = current_clock_;


			// STATS: instructions fired
			++stats_.instr_fired;
		}
	}

	void Tomasulo::retireInstructions() {
		// Make a min heap to pick the lowest retirable instruction (greater<FunctionUnit>)
		priority_queue<FunctionUnit, vector<FunctionUnit>, greater<FunctionUnit>> retirable;

		for (auto& unit_type : function_units_) {
			for (auto unit : unit_type) {
				if ((current_clock_ - unit.enter_cycle) >= unit.latency) {
					retirable.push(unit);
				}
			}
		}

		assert(busy_result_buses_.empty());

		// Fill the result buses with the as many retirable instructions as possible
		for (size_t i = 0; i < settings_.result_bus_count && !retirable.empty(); i++) {
			auto unit = retirable.top();
			retirable.pop();

			auto station = unit.station;
			auto instr = unit.station->instr;

			// Put instruction on a result bus
			ResultBus bus;
			bus.reg = instr->dst_reg;
			bus.tag = station->target_tag;
			bus.station = station;
			busy_result_buses_.push_back(bus);

			// Record when the instruction gets to state update
			instr->life->state_update_cycle = current_clock_;


			// Delete instruction from active function units
			auto& func_bucket = function_units_[instr->func_type];
			auto func_i = find(func_bucket.begin(), func_bucket.end(), unit);
			assert(func_i != func_bucket.end());
			func_bucket.erase(func_i);

			// STATS: executed total
			++stats_.instr_executed;
		}

		for (auto const& bus : busy_result_buses_) {
			// Update branch predictor bits
			if (bus.station->instr->is_branch) {
				branch_pred_.update(hashAddress(bus.station->instr->address), bus.station->instr->branch_taken);

				// Unmark branch tag
				if (bus.station->instr == bad_branch_instr_)
					bad_branch_instr_ = nullptr;
			}
		}
	}

	void Tomasulo::updateState() {
		for (auto const& bus : busy_result_buses_) {
			// Write to the register file
			if (bus.reg >= 0 && bus.tag == register_file[bus.reg].tag) {
				// Simulate write to register file
				register_file[bus.reg].tag = -1;
			}

			// Mark any matching tags as ready
			for (auto station : schedule_q_) {
				for (size_t i = 0; i < station->source_tags.size(); i++) {
					if (station->source_tags[i] == bus.tag)
						station->sources_ready[i] = true;
				}
			}
		}
	}

	void Tomasulo::sweepRetirementBuffer() {
		for (auto const& bus : retire_buffer_) {
			auto station_i = find(schedule_q_.begin(), schedule_q_.end(), bus.station);
			assert(station_i != schedule_q_.end());
			schedule_q_.erase(station_i);

			// STATS: executed total
			++stats_.instr_retired;
		}

		retire_buffer_.clear();
	}

	intmax_t Tomasulo::getNewTag() {
		return current_tag_ == INTMAX_MAX ? 1 : current_tag_++;
	}
}