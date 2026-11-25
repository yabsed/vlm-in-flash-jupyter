#include <torch/extension.h>
#include <algorithm>
#include <numeric>
#include <tuple>
#include <vector>

// (mask, importance, latency)
using Result = std::tuple<torch::Tensor, double, double>;

namespace {

void check(const torch::Tensor& v, int64_t R, const torch::Tensor& T, int64_t B) {

    // Importance
    TORCH_CHECK(
        v.device().is_cpu()
        && v.scalar_type() == torch::kFloat32
        && v.dim() == 1
        && v.is_contiguous(),
        "v must be contiguous CPU float32");

    // Budget
    TORCH_CHECK(0 <= R && R <= v.numel(), "R must be between 0 and N");

    // Latency of one run, for run lengths 0, 1, ..., N
    TORCH_CHECK(
        T.device().is_cpu()
        && T.scalar_type() == torch::kFloat64
        && T.dim() == 1
        && T.is_contiguous()
        && T.numel() > v.numel(),
        "T must be contiguous CPU float64 with N+1 entries");

    // Tile width
    TORCH_CHECK(B >= 1, "tile width B must be positive");
}

// Return the indices of the k largest scores, in no particular order.
// The order is irrelevant because the chosen tiles are disjoint.
std::vector<int64_t> largest(const std::vector<double>& scores, int64_t k) {

    // 0, 1, ..., size - 1
    std::vector<int64_t> order(scores.size());
    std::iota(order.begin(), order.end(), 0);

    // Ties prefer earlier rows.
    auto better = [&](int64_t a, int64_t b) {
        if (scores[a] != scores[b]) {
            return scores[a] > scores[b];
        }
        return a < b;
    };

    // Move the best k to the front without sorting them.
    const auto size = static_cast<int64_t>(order.size());

    if (0 < k && k < size) {
        std::nth_element(order.begin(), order.begin() + k, order.end(), better);
    }

    order.resize(k);
    return order;
}

// One importance vector and its latency table.
// Same structure as SelectionProblem in ../02/02_03_selector_sweep.ipynb.
struct SelectionProblem {

    int64_t             n;  // number of rows
    const float*        v;  // v[i]: importance of row i
    std::vector<double> P;  // P[i]: total importance before row i
    const double*       T;  // T[length]: latency of one selected run

    SelectionProblem(const torch::Tensor& values, const torch::Tensor& costs) {

        // Importance
        n = values.numel();
        v = values.data_ptr<float>();

        P.assign(n + 1, 0);
        for (int64_t i = 0; i < n; ++i) {
            P[i + 1] = P[i] + v[i];
        }

        // Latency
        T = costs.data_ptr<double>();
    }

    // Select exactly R rows using tiles and a residual interval.
    Result tiles(int64_t R, int64_t width) const {

        if (width > R) {
            return summarize(best_run(R));
        }

        // Budget
        const int64_t count     = R / width;
        const int64_t remaining = R % width;

        // Two offsets
        Result best;
        double best_ratio = -1;

        for (int64_t offset : {int64_t{0}, width / 2}) {
            auto mask = torch::zeros({n}, torch::kUInt8);

            // Full tiles
            if (!select_tiles(mask, offset, width, count)) {
                continue;
            }

            // Residual
            if (!fill_residual(mask, remaining)) {
                continue;
            }

            // Mask ratio
            Result       result     = summarize(mask);
            const double importance = std::get<1>(result);
            const double latency    = std::get<2>(result);
            const double ratio      = importance / latency;

            // Equal ratios keep the earlier offset.
            if (ratio > best_ratio) {
                best_ratio = ratio;
                best       = result;
            }
        }

        TORCH_CHECK(best_ratio >= 0, "no exact-R tiling was feasible");
        return best;
    }

    // Fill the `count` most important tiles on the grid starting at `offset`.
    // Return false if the grid has fewer than `count` tiles.
    bool select_tiles(torch::Tensor& mask, int64_t offset, int64_t width, int64_t count) const {

        // Candidate tiles
        std::vector<int64_t> starts;
        std::vector<double>  importance;

        for (int64_t start = offset; start + width <= n; start += width) {
            starts.push_back(start);

            // A prefix difference may round differently from v[start];
            // one-row tiles must rank exactly like Top-R.
            if (width == 1) {
                importance.push_back(v[start]);
            } else {
                importance.push_back(P[start + width] - P[start]);
            }
        }

        if (static_cast<int64_t>(starts.size()) < count) {
            return false;
        }

        // Fill the best candidates
        auto* selected = mask.data_ptr<uint8_t>();

        for (int64_t j : largest(importance, count)) {
            std::fill_n(selected + starts[j], width, uint8_t{1});
        }

        return true;
    }

    // Fill the most important free interval of `remaining` rows.
    // Return false if no interval is free.
    bool fill_residual(torch::Tensor& mask, int64_t remaining) const {

        if (remaining == 0) {
            return true;
        }

        auto* selected = mask.data_ptr<uint8_t>();

        // occupied[i]: number of selected rows before row i.
        std::vector<int64_t> occupied(n + 1, 0);

        for (int64_t i = 0; i < n; ++i) {
            occupied[i + 1] = occupied[i] + selected[i];
        }

        // Best free interval
        int64_t best_start      = -1;
        double  best_importance = -1;

        for (int64_t start = 0; start + remaining <= n; ++start) {
            const int64_t end        = start + remaining;
            const double  importance = P[end] - P[start];
            const bool    free       = occupied[end] == occupied[start];

            // Ties prefer earlier rows.
            if (free && importance > best_importance) {
                best_start      = start;
                best_importance = importance;
            }
        }

        if (best_start < 0) {
            return false;
        }

        // Fill
        std::fill_n(selected + best_start, remaining, uint8_t{1});
        return true;
    }

    // Select R consecutive rows with maximum importance.
    torch::Tensor best_run(int64_t R) const {
        int64_t best_start = 0;

        for (int64_t start = 1; start + R <= n; ++start) {
            const double importance      = P[start + R]      - P[start];
            const double best_importance = P[best_start + R] - P[best_start];

            // Ties prefer earlier rows.
            if (importance > best_importance) {
                best_start = start;
            }
        }

        auto mask = torch::zeros({n}, torch::kUInt8);
        std::fill_n(mask.data_ptr<uint8_t>() + best_start, R, uint8_t{1});
        return mask;
    }

    // Return the mask with its total importance and total latency.
    // Adjacent selected rows form one run and are charged once.
    Result summarize(torch::Tensor mask) const {
        const auto* selected = mask.data_ptr<uint8_t>();

        double  importance = 0;
        double  latency    = 0;
        int64_t length     = 0;  // rows in the current run

        for (int64_t i = 0; i < n; ++i) {
            if (selected[i]) {
                importance += v[i];
                ++length;
            } else {
                // A run ends here. With no open run, length is 0 and T[0] = 0.
                latency += T[length];
                length   = 0;
            }
        }

        // Last run, if it reaches the final row.
        latency += T[length];

        return {mask, importance, latency};
    }
};

}  // namespace

Result tiles(torch::Tensor v, int64_t R, torch::Tensor T, int64_t B) {
    check(v, R, T, B);

    SelectionProblem problem(v, T);
    return problem.tiles(R, B);
}

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m) {
    m.def("tiles", &tiles);
}
