#include <algorithm>
#include <Gselect.hpp>

namespace pipeline {

	Gselect::Gselect(unsigned int ghr_bits, uintmax_t ghr_init_val, unsigned int pred_bits, uintmax_t pred_init_val, size_t pred_table_size) {
		ghr_bits_ = ghr_bits;
		ghr_val_ = ghr_init_val & ((1 << ghr_bits_) - 1);
		pred_bits_ = pred_bits;

		// Initialize all of the prediction values
		pred_vals_.resize(pred_table_size);
		for (size_t i = 0; i < pred_table_size; ++i) {
			size_t num_preds = 1u << ghr_bits_;

			pred_vals_[i].resize(num_preds);

			for (size_t j = 0; j < num_preds; ++j)
				pred_vals_[i][j] = pred_init_val & ((1 << pred_bits) - 1);
		}
	}

	bool Gselect::predict(uint_least64_t hash) {
		return pred_vals_[hash % pred_vals_.size()][ghr_val_] >= (UINTMAX_C(1) << (pred_bits_ - 1));
	}

	void Gselect::update(uint_least64_t hash, bool taken) {
		size_t index = hash % pred_vals_.size();
		if (taken && pred_vals_[index][ghr_val_] < ((UINTMAX_C(1) << pred_bits_) - 1))
			++pred_vals_[index][ghr_val_];
		else if (!taken && pred_vals_[index][ghr_val_] > 0)
			--pred_vals_[index][ghr_val_];

		ghr_val_ = ((ghr_val_ << 1) | taken) & ((1 << ghr_bits_) - 1);
	}
}