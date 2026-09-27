# VLM in Flash: Chunk Selection Experiments

The cost of reading a subset of model weights from flash depends on both the number of bytes read and the lengths of the contiguous reads. Selecting individually important channels can produce many short reads. Selecting longer runs can improve read efficiency while retaining less importance at the same row budget. The time spent choosing the rows also matters.

This repository examines these trade-offs in three steps. It measures read cost by chunk size, finds the best fixed-budget mask under an approximate cost model, and measures the selection and read times of two practical methods. The experiments and analyses are in the Jupyter notebooks under `results/`. The `vlm-flash/` submodule contains the upstream implementation used for comparison.

In the saved measurements, Saturation took less time than Paper greedy to select a mask, and less time from the start of selection to completion of the CPU read. Its I/O time was longer, and the difference in retained importance varied by projection shape. These are projection-level measurements of time and activation-based importance. They do not measure full-model accuracy or inference speed.

| Notebook | Purpose | Saved output |
| --- | --- | --- |
| [01_measure_chunk_latency](results/01/01_measure_chunk_latency.ipynb) | Measure direct I/O by chunk size | `block_estimates.csv` |
| [01_chunk_latency](results/01/01_chunk_latency.ipynb) | Fit read-cost models and evaluate held-out blocks | `latency_model.json` |
| [02_capture_traces](results/02/02_capture_traces.ipynb) | Record projection inputs during dense forward passes | `activation_traces.npz` |
| [02_global_oracle](results/02/02_global_oracle.ipynb) | Compare an oracle and heuristics at fixed row budgets | `oracle_results_fixed_r.csv` |
| [02_analyze_oracle](results/02/02_analyze_oracle.ipynb) | Examine ratio gaps and selected runs | Tables and figures |
| [03_selection_latency](results/03/03_selection_latency.ipynb) | Compare selection time, read time, and retained importance | `measured_results.csv`, `measured_environment.json` |

Notebook 01 measures sizes from 1 to 256 KiB in 1 KiB steps and from 260 to 768 KiB in 4 KiB steps. It randomizes the size order within each of seven measurement blocks and uses six read threads with `O_DIRECT`. It converts batch throughput into an amortized cost per chunk; this is not the response time of an isolated read.

The first four blocks are used to fit the models, and the remaining three are held out for evaluation. The two-line model is

$$
T_{\mathrm{2L}}(z)=c_1z+\delta\max(z_0,z),\qquad c_1,\delta\ge0,
$$

where $z>0$ is the chunk length in KiB and $T$ is in milliseconds. The fitted free boundary $z_0$ is 207 KiB. Both the measured size with the lowest cost per KiB and the first size to reach 99% of the peak smoothed training throughput are 256 KiB. These sizes come from different criteria.

| Cost model | Boundary (KiB) | Held-out RMSE (µs/chunk) | Held-out MAPE (%) |
| --- | ---: | ---: | ---: |
| Two-line, free boundary | 207 | 1.632 | 2.878 |
| Two-line, 99% throughput boundary | 256 | 1.960 | 4.018 |
| Hinge | 228 | 1.449 | 2.442 |
| Affine | — | 3.538 | 9.747 |

The hinge has the lowest approximation error in this comparison. The later analysis uses the two-line model because its cost separates into a term for the selected row count and a penalty for short runs. If each row is $w$ KiB, $M$ is a selection mask, and $\mathcal C(M)$ is the set of its maximal contiguous runs, then

$$
L(M)=(c_1+\delta)w|M|+
\delta\sum_{C\in\mathcal C(M)}(z_0-w|C|)_+.
$$

At a fixed row count, the first term is constant. Differences in modeled cost therefore come from the short-run penalty. Extrapolation beyond the measured chunk sizes and the actual read time of a complete mask require separate validation.

Notebook 02 records attention and MLP projection inputs from dense forward passes of `Qwen/Qwen2.5-0.5B-Instruct` on three text prompts. The capture notebook pins the model revision. Channel importance is the mean absolute input activation over batch and token positions, normalized to have mean one for the comparison. The traces in this experiment come from text inputs to a language model.

For exactly $R$ selected rows, the oracle maximizes the ratio of retained importance to modeled read cost:

$$
\max_{|M|=R}\frac{I(M)}{L(M)},\qquad I(M)=\sum_i v_iM_i.
$$

The notebook solves the additive problem $I-qL$ with dynamic programming and narrows upper and lower bounds on the optimal ratio. With the two-line cost, one additive solve including mask recovery uses $O((N-R+1)(R+1))$ time and memory. The ratio search uses a relative tolerance of `1e-6`, with float64 arithmetic. The oracle serves as a reference for evaluating selection methods.

The comparison includes Paper greedy, which accepts nonoverlapping windows ranked by importance per cost; Saturation tiles, which selects full tiles and a remainder interval; and Top-R, which selects the individually most important rows. Saturation tiles compares two starting offsets. Its tile width is the fitted boundary converted to rows and rounded up. The oracle, tiles, and Top-R select exactly $R$ rows. Paper may leave part of the budget unused because it selects whole windows.

The saved oracle comparison uses 32 traces for each of four projection shapes and nine budgets from 10% to 90%. The table reports the median upper bound on ratio loss relative to the fixed-$R$ optimum. Parentheses give the number of eligible cases.

| Input × output | Paper greedy (%) | Saturation tiles (%) | Top-R (%) |
| --- | ---: | ---: | ---: |
| 4864 × 896 | 3.443 (64) | 3.427 (288) | 93.713 (288) |
| 896 × 128 | 0.227 (32) | 0.000 (288) | 98.310 (288) |
| 896 × 4864 | 2.227 (288) | 3.890 (288) | 72.767 (288) |
| 896 × 896 | 5.580 (96) | 5.034 (288) | 92.002 (288) |

Cases where Paper does not fill $R$ are excluded from its fixed-$R$ gap. The columns can therefore cover different sets of cases. The analysis notebook also reports a comparison restricted to cases where both Paper and tiles are eligible. A displayed `0.000` is rounded to three decimal places. These gaps concern activation-based importance and fitted read cost; they are not accuracy losses.

Notebook 03 compares Paper greedy and Saturation on the same traces and requested budgets using actual reads. This experiment uses the measured cost table from 01 rather than the two-line model used in 02. Saturation takes its tile boundary from the 256 KiB minimum-cost-per-KiB point, while Paper uses a 768 KiB window ceiling at the end of the measured range. Paper's window settings come from the upstream Appendix H tuner. The cost models and selector settings thus differ between 02 and 03.

The measurements use 32 traces per shape, for 128 traces in total, and eight budgets from 30% to 100%. Each case has three warmup rounds and 30 recorded repetitions, with the method order randomized. At a 100% budget, both methods bypass selection. Reads come from a file filled with random data and sized for the weight rows.

For the table below, repetitions are first reduced to a median within each trace, method, and budget. Saturation minus Paper is then computed for paired traces and budgets, followed by a median for each shape. Negative time differences mean Saturation was faster. Importance differences are in percentage points (pp). The 100% budget is included.

| Input × output | Selection time difference (ms) | I/O time difference (ms) | Selection-to-read-completion difference (ms) | Retained importance difference (pp) |
| --- | ---: | ---: | ---: | ---: |
| 4864 × 896 | -1.7595 | +0.1233 | -1.6172 | -0.7254 |
| 896 × 128 | -0.0998 | +0.0064 | -0.0955 | +0.1079 |
| 896 × 4864 | -1.4447 | +0.0943 | -1.3355 | -1.1530 |
| 896 × 896 | -1.0236 | +0.0217 | -0.9977 | -1.4988 |

Selection time starts with a prepared CPU importance vector and ends when a mask is available. It includes Paper's GPU sorting and score transfers. I/O time covers the native reader's read and thread interval; the time from selection to read completion also includes read preparation. The medians of the component differences do not add up to the median total difference. GPU weight transfer and model computation are outside the timed interval.

The measurement environment is recorded in [measured_environment.json](results/03/measured_environment.json). The saved run used Linux x86_64, PyTorch `2.11.0+cu130`, and an NVMe path on Btrfs. It used six read threads, a CPU read destination, and required `O_DIRECT`. A different device or filesystem calls for a new cost profile and new measurements.

Run each notebook with its own directory as the working directory. Reading the saved analyses requires NumPy, pandas, SciPy, Matplotlib, and Jupyter. Trace capture also requires PyTorch and Transformers. Native selection and measurement need a C++ build environment and the upstream dependencies; a new run of 03 requires CUDA. See the upstream [installation guide](vlm-flash/docs/installation.md) for its setup requirements.

1. From this repository's root, run `git submodule update --init --recursive vlm-flash` to prepare the upstream code.
2. To inspect the saved data, run `01_chunk_latency`, `02_analyze_oracle`, and `03_selection_latency`. Set `RUN_BENCHMARK = False` in 03 before running it; the default `True` tunes and measures again. The last cell of 01 rewrites the cost model file.
3. To regenerate all data, run the notebooks in the order shown in the first table. Run the measurement notebooks in 01 and 03 on the target SSD. Trace capture downloads the checkpoint if it is not cached. The save cells in the generating notebooks replace their existing output files.

The comparison is limited to activations from three prompts, four projection shapes, and the recorded measurement environment. Layers and budgets drawn from the same prompt are related observations. Results after input or setting changes must be regenerated manually. Equal requested budgets can select different numbers of rows and retain different amounts of importance, so these results alone cannot establish a speedup at equal accuracy.
