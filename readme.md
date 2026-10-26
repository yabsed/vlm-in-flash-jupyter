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
| [03_01_tune_paper](results/03/03_01_tune_paper.ipynb) | Tune Paper candidate windows for each projection shape on the target device | `paper_tuning_trials.csv`, `paper_tuning.csv` |
| [03_02_measure_latency](results/03/03_02_measure_latency.ipynb) | Measure selection and direct-I/O time for Paper and tile widths down to one row | `cases.csv`, `timings.csv`, `environment.json` |
| [03_03_compare_methods](results/03/03_03_compare_methods.ipynb) | Compare measured importance–latency distributions and paired differences across methods | Tables and figures |
| [03_04_chunk_lengths](results/03/03_04_chunk_lengths.ipynb) | Compare selected chunk-length distributions across traces at one requested budget | Figures |

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

Experiment 02's saved results use captured importance directly. After changing the inputs, regenerate 02_02 and 02_03, then run 02_04 and 02_05 in any order. Experiment 03 reads the activation traces and measured profile independently of the 02 selector results. The oracle uses the Dinkelbach DP with `rtol=1e-6`.

Experiment 03 separates device tuning, measurement, method comparison and chunk-length analysis into four notebooks. It uses the same geometric and coarse width grid. Its saturation boundary minimizes measured cost per KiB in the profile from 01. That boundary is converted to rows before halving; it differs from the fitted two-line boundary in 02. Realized row widths provide the common axis for comparing modeled and measured behavior.

Notebook 03_01 tunes Paper's window start and stride cap for each projection shape, recording candidate times in `paper_tuning_trials.csv` and selected settings in `paper_tuning.csv`. Rerun it when the device, measured profile or projection shapes change. The checked-in selected settings were recovered from the original measurement environment; the original candidate times were not saved. A new tuning run writes the full candidate table. It does not change the tuning snapshot attached to existing measurements.

Notebook 03_02 reads the selected settings without tuning again. It compiles the unchanged native tile implementation in `results/03/selectors.cpp` before measurement. Experiment settings, selector preparation and the measurement loop remain in the notebook. Selection time includes Paper's GPU sorting and score transfers. Wall time runs from prepared CPU importance through completion of the CPU read. The reader uses a random file sized for weight rows, six threads and `O_DIRECT`. At a full budget, all methods bypass selection. GPU weight transfer and model computation are outside timing.

The measurement writes `cases.csv` with one row per trace, requested budget and method, and `timings.csv` with one row per recorded repetition. The integer `case_id` joins the files. Case rows contain the mask intervals, retained importance, actual row count and tile geometry; timing rows contain `case_id`, `repeat`, `selection_ms`, `io_ms` and `wall_ms`. `environment.json` stores device and acquisition settings, including the tuning used for that measurement run. The saved 14,080 cases and 422,400 samples preserve every field of the former 11 CSV parts while reducing their combined size from about 330 MiB to 26 MiB. The conversion did not rerun measurements.

The current 01 profile does not exactly reproduce the recorded modeled costs of some saved 03 masks. Running the original selector code with the current inputs also produces some different masks. The saved measurements retain their historical masks and costs; the two analyses use those recorded cases without applying the current profile again. Rerun tuning and measurement on the target device to obtain results for the current inputs.

Notebook 03_03 follows the structure of 02_04. Its Data section reduces timing repetitions to case medians, joins the saved case metadata and normalizes column names. Choose `METRIC` as `io_ms`, `selection_ms` or `wall_ms`; the chosen value becomes `latency`. The remaining analysis uses `ShapeResults`, `MethodResults` and `TracePoints`, indexed by shape, method and requested budget. Each method has a panel showing trace observations and marginal medians, with Paper's median curve as a reference. Baseline/coarse panels and fine panels share separate logarithmic latency ranges. A table reports paired differences from Paper for the selected metric and retained importance, including the full-budget control.

Notebook 03_04 follows 02_05 and reads only `cases.csv` and `environment.json`. It prepares `ShapeResults`, `MethodResults` and `ChunkLengths` from the saved intervals, then plots the mean chunk count per trace in each length bin. Empty masks remain in the denominator, and Paper uses its actual selected rows. Bins and axes are shared across methods and remain fixed across budgets within each shape. Change `FRACTION` and rerun the final two cells to inspect another recorded budget. Both analyses display every recorded method, ordered by decreasing tile width after Paper, and run independently without native compilation or CUDA.

Run each notebook with its own directory as the working directory. Reading the saved analyses requires NumPy, pandas, SciPy, Matplotlib, and Jupyter. Trace capture also requires PyTorch and Transformers. Native selection and measurement need a C++ build environment and the upstream dependencies; a new run of 03 requires CUDA. See the upstream [installation guide](vlm-flash/docs/installation.md) for its setup requirements.

1. From this repository's root, run `git submodule update --init --recursive vlm-flash` to prepare the upstream code.
2. To inspect the saved data, run `01_02_chunk_latency`, `02_04_analyze_oracle`, `02_05_chunk_lengths`, `03_03_compare_methods` and `03_04_chunk_lengths`. The 03 analyses need no CUDA or native compilation. The last cell of 01 rewrites the cost model file.
3. To change only the 02 selector grid, run `02_03_selector_sweep` followed by 02_04 and 02_05.
4. To regenerate all data, run the notebooks in the order shown in the first table. Run the measurement notebooks in 01 and 03 on the target SSD. Trace capture downloads the checkpoint if it is not cached. In 03, tune with 03_01, measure with 03_02, then run 03_03 and 03_04 in either order. Changes to tile widths, budgets or trace sampling require a new 03_02 run; they do not require retuning unchanged shapes on the same device and profile. The save cells in the generating notebooks replace their existing output files.

The comparison is limited to activations from three prompts, four projection shapes, and the recorded measurement environment. Layers and budgets drawn from the same prompt are related observations. Results after input or setting changes must be regenerated manually. Equal requested budgets can select different numbers of rows and retain different amounts of importance, so these results alone cannot establish a speedup at equal accuracy.
