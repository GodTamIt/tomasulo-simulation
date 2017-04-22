#pragma once
#include <stdint.h>
#include <vector>
#include <memory>

using namespace std;

namespace pipeline {
	class Gselect {
		public:
			Gselect(unsigned int ghr_bits, uintmax_t ghr_init_val, unsigned int pred_bits, uintmax_t pred_init_val, size_t num_preds);
			bool predict(uint_least64_t hash);
			void update(uint_least64_t hash, bool taken);

		private:
			unsigned int ghr_bits_;
			uintmax_t ghr_val_;
			unsigned int pred_bits_;
			vector<vector<uintmax_t>> pred_vals_;
	};
}