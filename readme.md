# VLM in Flash: Chunk Selection Experiments

The cost of reading a subset of model weights from flash depends on both the number of bytes read and the lengths of the contiguous reads. Selecting individually important channels can produce many short reads. Selecting longer runs can improve read efficiency while retaining less importance at the same row budget. The time spent choosing the rows also matters.

This repository examines these trade-offs in three steps. It measures read cost by chunk size, finds the best fixed-budget mask under an approximate cost model, and measures the selection and read times of two practical methods. The experiments and analyses are in the Jupyter notebooks under `results/`. The `vlm-flash/` submodule contains the upstream implementation used for comparison.

The analysis notebooks calculate comparisons from the saved measurements, including paired differences in time and retained importance. These are projection-level measurements; they do not measure full-model accuracy or inference speed.

| Notebook | Purpose | Saved output |
| --- | --- | --- |
| [01_01_measure_chunk_latency](results/01/01_01_measure_chunk_latency.ipynb) | Measure direct I/O by chunk size | `chunk_latency_measurements.csv` |
| [01_02_chunk_latency](results/01/01_02_chunk_latency.ipynb) | Fit read-cost models and evaluate held-out repeats | `latency_model.json` |
| [02_01_capture_traces](results/02/02_01_capture_traces.ipynb) | Record projection inputs during dense forward passes | `activation_traces.json` |
| [02_02_global_oracle](results/02/02_02_global_oracle.ipynb) | Generate fixed-budget DP reference masks and ratio bounds | `oracle_reference.csv` |
| [02_03_selector_sweep](results/02/02_03_selector_sweep.ipynb) | Evaluate Paper and distinct tile widths on all traces and row budgets | `selector_results.csv` |
| [02_04_analyze_oracle](results/02/02_04_analyze_oracle.ipynb) | Plot importance–latency trajectories, including underfilled Paper masks | Figures |
| [02_05_chunk_lengths](results/02/02_05_chunk_lengths.ipynb) | Compare chunk-length histograms by shape, method, and requested budget | Figures |
| [03_01_measure_selection_latency](results/03/03_01_measure_selection_latency.ipynb) | Measure selection and direct-I/O time for Paper and tile widths down to one row | `measured_results/part-*.csv`, `measured_environment.json` |
| [03_02_analyze_selection_latency](results/03/03_02_analyze_selection_latency.ipynb) | Compare time, retained importance, tile sizes and selected masks | Tables, figures and computed findings |

Notebook 01 measures sizes from 1 to 256 KiB in 1 KiB steps and from 260 to 768 KiB in 4 KiB steps. It randomizes the size order within each of seven measurement repeats and uses six read threads with `O_DIRECT`. It converts batch throughput into an amortized cost per chunk; this is not the response time of an isolated read.

The first four repeats are used to fit the models, and the remaining three are held out for evaluation. The two-line model is

$$
T_{\mathrm{2L}}(z)=bz+\delta\max(z_0,z),\qquad b,\delta\ge0,
$$

where $z>0$ is the chunk length in KiB and $T$ is in milliseconds. The stored coefficients are `short_slope` ($b$) and `slope_change` ($\delta$); the slope beyond the boundary is $b+\delta$. JSON uses `short_slope_ms_per_kib` and `slope_change_ms_per_kib`, while the oracle CSV uses `short_slope_ms_per_row` and `slope_change_ms_per_row`. The fitted free boundary $z_0$ is 207 KiB. Both the measured size with the lowest cost per KiB and the first size to reach 99% of the peak smoothed training throughput are 256 KiB. These sizes come from different criteria.

| Cost model | Boundary (KiB) | Held-out RMSE (µs/chunk) | Held-out MAPE (%) |
| --- | ---: | ---: | ---: |
| Two-line, free boundary | 207 | 1.632 | 2.878 |
| Two-line, 99% throughput boundary | 256 | 1.960 | 4.018 |
| Hinge | 228 | 1.449 | 2.442 |
| Affine | — | 3.538 | 9.747 |

The hinge has the lowest approximation error in this comparison. The later analysis uses the two-line model because its cost separates into a term for the selected row count and a penalty for short runs. If each row is $w$ KiB, $M$ is a selection mask, and $\mathcal C(M)$ is the set of its maximal contiguous runs, then

$$
L(M)=(b+\delta)w|M|+
\delta\sum_{C\in\mathcal C(M)}(z_0-w|C|)_+.
$$

At a fixed row count, the first term is constant. Differences in modeled cost therefore come from the short-run penalty. Extrapolation beyond the measured chunk sizes and the actual read time of a complete mask require separate validation.

Notebook 02 records attention and MLP projection inputs from dense forward passes of `Qwen/Qwen2.5-0.5B-Instruct` on three text prompts. The capture notebook pins the model revision. Channel importance is the mean absolute input activation over batch and token positions, used directly by the selectors. Reports compare retained importance as a fraction of each trace's total, modeled latency, and chunk-length distributions. The traces in this experiment come from text inputs to a language model.

The capture appends one dictionary per trace, containing `trace_id`, `shape`, `row_bytes`, and `values` in their original channel order. It writes these dictionaries directly to `activation_traces.json` with the weight element size. Readers load the JSON and convert the value lists to NumPy arrays. Model revision and prompts remain in the capture notebook's settings. The checked-in JSON preserves all 504 recordings and their numerical values from the former NPZ archive; the readable file occupies about 15 MiB.

For exactly $R$ selected rows, the oracle maximizes the ratio of retained importance to modeled read cost:

$$
\max_{|M|=R}\frac{I(M)}{L(M)},\qquad I(M)=\sum_i v_iM_i.
$$

The notebook solves the additive problem $I-qL$ with dynamic programming and narrows upper and lower bounds on the optimal ratio. With the two-line cost, one additive solve including mask recovery uses $O((N-R+1)(R+1))$ time and memory. The ratio search uses a relative tolerance of `1e-6`, with float64 arithmetic. The oracle serves as a reference for evaluating selection methods.

The oracle notebook generates only the reference. Each dictionary appended to `records` becomes one CSV row, and its keys become the column names. Both CSVs use the same 14 common columns, in the same order: `trace_id`, `n`, `d`, `fraction`, `R_nominal`, `importance_total`, `saturation_rows`, `short_slope_ms_per_row`, `slope_change_ms_per_row`, `method`, `tile_width`, `importance`, `latency`, and `intervals`. Each result carries its own input totals and cost parameters. The oracle uses `method="Global DP"` and an empty `tile_width`, and adds `eta_lower` and `eta_upper` after the common columns. Intervals are JSON lists of `[start, end)` pairs inside one CSV cell.

Notebooks 02_02 and 02_03 independently use all captured traces and fractions from 0.1 through 0.9, with the same row-budget calculation. Notebook 02_03 does not read the oracle CSV. The two analyses, 02_04 and 02_05, independently read both result files. Notebook 02_04 plots each mask's actual importance and modeled latency, including Paper masks that select fewer than the requested rows. Notebook 02_05 groups the saved chunk lengths by shape, method, and requested fraction. It plots the mean number of chunks per trace in each length bin, including underfilled Paper masks and empty masks. All methods within a figure share bins and axis ranges, with a line marking the model's saturation boundary. Selector inputs are never filled from the oracle.

Changing tile widths or offsets requires rerunning 02_03 and the analyses, without solving the DP again. A changed archive, model, or budget grid requires rerunning both 02_02 and 02_03 before the analyses. A changed oracle solver requires rerunning 02_02 and the analyses. Changing only the displayed histogram budget requires rerunning the final two cells of 02_05. Files are not linked by hashes, so matching inputs and results is the experimenter's responsibility.

For saturation boundary $s$ in rows, the tile family includes

$$B_j=\max(1,\lceil s/2^j\rceil),$$

ending at one row, and the coarse multipliers $1,1.25,1.5,1.75,2$. Each shape retains only distinct integer widths. At one row, tiles equal stable Top-R: full-tile ranking becomes row ranking and there is no residual. The endpoint is recorded once. Notebook 02_05 checks matching fractions and trace IDs across methods. Histograms use actual maximal contiguous runs, whose lengths can differ from tile widths when selected tiles join. Bin edges stay fixed across budgets for each shape. The x axis is logarithmic, and the shared y axis is linear up to one chunk per trace and logarithmic above. Histogram heights sum to the mean number of chunks per trace; they are not normalized to percentages or divided by bin width.

Experiment 02's saved results use captured importance directly. After changing the inputs, regenerate 02_02 and 02_03, then run 02_04 and 02_05 in any order, followed by 03_01 and 03_02 on the measurement setup. The oracle uses the Dinkelbach DP with `rtol=1e-6`.

Notebook 03 separates measurement from analysis and uses the same geometric and coarse width grid. Its saturation boundary minimizes measured cost per KiB in the profile from 01. That boundary is converted to rows before halving; it differs from the fitted two-line boundary in 02. Realized row widths provide the common axis for comparing modeled and measured behavior.

Each CSV part is at most 32 MiB and repeats the header; analysis concatenates all parts. The measurement records each repetition and selected intervals in `measured_results/part-*.csv`, with device and tuning settings in `measured_environment.json`. Selection time includes Paper's GPU sorting and score transfers. Wall time runs from prepared CPU importance through completion of the CPU read. At a full budget, all methods bypass selection. GPU weight transfer and model computation are outside timing.

The analysis notebook reads the saved CSV parts and environment JSON. It reduces repetitions to case medians, compares Paper with the reference tile size, then compares the recorded tile widths using paired differences, logarithmic time axes and recorded masks. Each time metric keeps its own millisecond range, shared between comparisons. The width ranking excludes full-budget cases. Missing geometric widths are reported as unmeasured. Tables and findings are computed from the loaded data, so rerunning the analysis reflects the saved session without editing conclusions.

Run each notebook with its own directory as the working directory. Reading the saved analyses requires NumPy, pandas, SciPy, Matplotlib, and Jupyter. Trace capture also requires PyTorch and Transformers. Native selection and measurement need a C++ build environment and the upstream dependencies; a new run of 03 requires CUDA. See the upstream [installation guide](vlm-flash/docs/installation.md) for its setup requirements.

1. From this repository's root, run `git submodule update --init --recursive vlm-flash` to prepare the upstream code.
2. To inspect the saved data, run `01_02_chunk_latency`, `02_04_analyze_oracle`, `02_05_chunk_lengths`, and `03_02_analyze_selection_latency`. The 03 analysis needs no CUDA or native compilation. The last cell of 01 rewrites the cost model file.
3. To change only the 02 selector grid, run `02_03_selector_sweep` followed by 02_04 and 02_05.
4. To regenerate all data, run the notebooks in the order shown in the first table. Run the measurement notebooks in 01 and 03 on the target SSD. Trace capture downloads the checkpoint if it is not cached. The save cells in the generating notebooks replace their existing output files.

The comparison is limited to activations from three prompts, four projection shapes, and the recorded measurement environment. Layers and budgets drawn from the same prompt are related observations. Results after input or setting changes must be regenerated manually. Equal requested budgets can select different numbers of rows and retain different amounts of importance, so these results alone cannot establish a speedup at equal accuracy.
