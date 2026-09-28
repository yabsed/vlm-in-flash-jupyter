# VLM in Flash: Chunk Selection Experiments

The cost of reading a subset of model weights from flash depends on both the number of bytes read and the lengths of the contiguous reads. Selecting individually important channels can produce many short reads. Selecting longer runs can improve read efficiency while retaining less importance at the same row budget. The time spent choosing the rows also matters.

This repository examines these trade-offs in three steps. It measures read cost by chunk size, finds the best fixed-budget mask under an approximate cost model, and measures the selection and read times of two practical methods. The experiments and analyses are in the Jupyter notebooks under `results/`. The `vlm-flash/` submodule contains the upstream implementation used for comparison.

The analysis notebooks calculate comparisons from the saved measurements, including paired differences in time and retained importance. These are projection-level measurements; they do not measure full-model accuracy or inference speed.

| Notebook | Purpose | Saved output |
| --- | --- | --- |
| [01_01_measure_chunk_latency](results/01/01_01_measure_chunk_latency.ipynb) | Measure direct I/O by chunk size | `block_estimates.csv` |
| [01_02_chunk_latency](results/01/01_02_chunk_latency.ipynb) | Fit read-cost models and evaluate held-out blocks | `latency_model.json` |
| [02_01_capture_traces](results/02/02_01_capture_traces.ipynb) | Record projection inputs during dense forward passes | `activation_traces.npz` |
| [02_02_global_oracle](results/02/02_02_global_oracle.ipynb) | Generate fixed-budget DP reference masks and ratio bounds | `oracle_reference.csv` |
| [02_03_selector_sweep](results/02/02_03_selector_sweep.ipynb) | Evaluate Paper and distinct tile widths against the saved reference | `selector_results.csv` |
| [02_04_analyze_oracle](results/02/02_04_analyze_oracle.ipynb) | Examine ratio gaps and selected runs | Tables and figures |
| [03_01_measure_selection_latency](results/03/03_01_measure_selection_latency.ipynb) | Measure selection and direct-I/O time for Paper and tile widths down to one row | `measured_results.csv`, `measured_environment.json` |
| [03_02_analyze_selection_latency](results/03/03_02_analyze_selection_latency.ipynb) | Compare time, retained importance, tile sizes and selected masks | Tables, figures and computed findings |

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

The oracle notebook generates only the reference. It saves the feasible ratio `eta_lower`, the upper bound `eta_upper`, selected intervals and input hashes for each trace and budget. The selector notebook checks the archive and model hashes, then evaluates Paper and tiles using the saved bounds. Changing tile widths or offsets requires rerunning the sweep and report, without solving the DP again. A changed archive, model, case selection or solver configuration requires a new reference.

For saturation boundary $s$ in rows, the tile family includes

$$B_j=\max(1,\lceil s/2^j\rceil),$$

ending at one row, and the coarse multipliers $1,1.25,1.5,1.75,2$. Each shape retains only distinct integer widths. At one row, tiles equal stable Top-R: full-tile ranking becomes row ranking and there is no residual. The endpoint is recorded once. The report computes gaps, run counts, short-run deficit, trajectories and masks from `oracle_reference.csv` and `selector_results.csv`. Paper's gap is defined only when it fills the requested budget and importance is nonzero. Paired comparisons are made within each shape.

The `reference_origin` column records how a reference was produced. Migrated recordings reconstruct the upper ratio bound from the saved oracle ratio and gap; their masks were checked against the current traces and model. The legacy `oracle_results_fixed_r.csv` is no longer an input to the notebooks.

Notebook 03 separates measurement from analysis and uses the same geometric and coarse width grid. Its saturation boundary minimizes measured cost per KiB in the profile from 01. That boundary is converted to rows before halving; it differs from the fitted two-line boundary in 02. Realized row widths provide the common axis for comparing modeled and measured behavior.

The measurement records each repetition and selected intervals in `measured_results.csv`, with device and tuning settings in `measured_environment.json`. Selection time includes Paper's GPU sorting and score transfers. Wall time runs from prepared CPU importance through completion of the CPU read. At a full budget, all methods bypass selection. GPU weight transfer and model computation are outside timing.

The analysis notebook reads only those two saved files. It reduces repetitions to case medians, compares Paper with the reference tile size, then compares the recorded tile widths using paired differences, shared plot axes and recorded masks. The width ranking excludes full-budget cases. Missing geometric widths are reported as unmeasured. Tables and findings are computed from the loaded data, so rerunning the analysis reflects the saved session without editing conclusions.

Run each notebook with its own directory as the working directory. Reading the saved analyses requires NumPy, pandas, SciPy, Matplotlib, and Jupyter. Trace capture also requires PyTorch and Transformers. Native selection and measurement need a C++ build environment and the upstream dependencies; a new run of 03 requires CUDA. See the upstream [installation guide](vlm-flash/docs/installation.md) for its setup requirements.

1. From this repository's root, run `git submodule update --init --recursive vlm-flash` to prepare the upstream code.
2. To inspect the saved data, run `01_02_chunk_latency`, `02_04_analyze_oracle`, and `03_02_analyze_selection_latency`. The 03 analysis needs no CUDA or native compilation. The last cell of 01 rewrites the cost model file.
3. To change only the 02 selector grid, run `02_03_selector_sweep` followed by `02_04_analyze_oracle`.
4. To regenerate all data, run the notebooks in the order shown in the first table. Run the measurement notebooks in 01 and 03 on the target SSD. Trace capture downloads the checkpoint if it is not cached. The save cells in the generating notebooks replace their existing output files.

The comparison is limited to activations from three prompts, four projection shapes, and the recorded measurement environment. Layers and budgets drawn from the same prompt are related observations. Results after input or setting changes must be regenerated manually. Equal requested budgets can select different numbers of rows and retain different amounts of importance, so these results alone cannot establish a speedup at equal accuracy.
