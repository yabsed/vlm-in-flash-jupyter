# Tile-Based Selective Weight Loading Accounting for SSD Throughput Saturation

## 1. Research Background and Objectives

### 1.1 A Study of SSD Architecture by Jeong et al. (2013)

According to Jeong et al. (2013), flash memory, unlike HDDs, can access data quickly regardless of its physical location, offering consistently fast random access. However, the sustained read/write speed of a single flash memory device is lower than that of an HDD. To compensate, most SSDs adopt multichannel and multiway architectures.

The researchers used IOMeter to measure how HDD and SSD throughput varies with request size. For both SSD reads and writes, throughput—the amount of data processed per unit time—increased with request size until it stopped increasing beyond a certain size. This occurs because larger requests allow the SSD to use its multiple channels in parallel, eventually reaching maximum channel utilization.

The researchers analyzed how this throughput saturation differs between SSD writes and reads. For writes, sequential access achieved higher throughput than random access. For reads, however, sequential and random access reached saturation at similar request sizes and exhibited similar throughput levels at saturation.

### 1.2 A Study of I/O-Efficient Sparsification by Lee et al. (2025)

A challenge in running models such as LLMs on portable hardware is that the device often lacks enough DRAM to hold the entire model. Research on activation sparsification has explored storing weights in flash memory and loading only rows predicted to be important into DRAM.

Lee et al. (2025) questioned the assumption underlying existing methods that I/O latency increases in proportion to the amount of data requested. Even when reading the same amount of weights, throughput can decrease if the selected rows are sparsely distributed over a wide range. The paper argues that this effect may be more pronounced for models such as VLMs, whose channel importance is uniformly distributed.

The researchers therefore defined a chunk-based latency model that predicts read latency by accounting for the physical contiguity of weight rows, and proposed Neuron Chunking, a selection method based on importance and latency.

Let <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-001-dark.svg"><img src="report/images/readme-math-001.svg" alt="N" align="middle"></picture> be the number of weight rows and <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-002-dark.svg"><img src="report/images/readme-math-002.svg" alt="M\in\{0,1\}^N" align="middle"></picture> be a selection mask. If <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-003-dark.svg"><img src="report/images/readme-math-003.svg" alt="M_i=1" align="middle"></picture>, row <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-004-dark.svg"><img src="report/images/readme-math-004.svg" alt="i" align="middle"></picture> is read. For an input containing <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-005-dark.svg"><img src="report/images/readme-math-005.svg" alt="H" align="middle"></picture> tokens, the importance of channel <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-004-dark.svg"><img src="report/images/readme-math-004.svg" alt="i" align="middle"></picture> and the importance retained by the mask are defined as follows.

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-006-dark.svg"><img src="report/images/readme-math-006.svg" alt="v_i=\frac1H\sum_{t=1}^{H}|a_{t,i}|, \qquad I(M)=\sum_{i=0}^{N-1}v_iM_i"></picture>
</p>

Here, <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-007-dark.svg"><img src="report/images/readme-math-007.svg" alt="a_{t, i}" align="middle"></picture> is the activation of channel <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-004-dark.svg"><img src="report/images/readme-math-004.svg" alt="i" align="middle"></picture> for token <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-008-dark.svg"><img src="report/images/readme-math-008.svg" alt="t" align="middle"></picture>.

Decompose the rows selected by mask <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-009-dark.svg"><img src="report/images/readme-math-009.svg" alt="M" align="middle"></picture> into maximal contiguous intervals, and denote their set by <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-010-dark.svg"><img src="report/images/readme-math-010.svg" alt="\mathcal C(M)" align="middle"></picture>. Let <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-011-dark.svg"><img src="report/images/readme-math-011.svg" alt="T[l]" align="middle"></picture> be the latency cost of reading an interval of <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-012-dark.svg"><img src="report/images/readme-math-012.svg" alt="l" align="middle"></picture> rows. The original paper's chunk-based latency model is

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-013-dark.svg"><img src="report/images/readme-math-013.svg" alt="\widehat L_{\mathrm{table}}(M) =\sum_{C\in\mathcal C(M)}T[|C|]"></picture>
</p>

For each interval length <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-012-dark.svg"><img src="report/images/readme-math-012.svg" alt="l" align="middle"></picture>, the researchers measured <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-011-dark.svg"><img src="report/images/readme-math-011.svg" alt="T[l]" align="middle"></picture> by reading enough chunks of the same size for throughput to stabilize. The definition of <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-014-dark.svg"><img src="report/images/readme-math-014.svg" alt="\widehat L_{\mathrm{table}}" align="middle"></picture> provides an approximation of the time required to read the weights selected by <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-009-dark.svg"><img src="report/images/readme-math-009.svg" alt="M" align="middle"></picture> from flash memory.

The researchers formulated the problem of finding a mask <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-009-dark.svg"><img src="report/images/readme-math-009.svg" alt="M" align="middle"></picture> that maximizes retained importance per unit of estimated latency under a row budget <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-015-dark.svg"><img src="report/images/readme-math-015.svg" alt="R" align="middle"></picture>.

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-016-dark.svg"><img src="report/images/readme-math-016.svg" alt="\max_{\substack{M\in\{0,1\}^N\\1\le\|M\|_1\le R}} \frac{I(M)}{\widehat L_{\mathrm{table}}(M)}."></picture>
</p>

To construct a mask <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-009-dark.svg"><img src="report/images/readme-math-009.svg" alt="M" align="middle"></picture> with a high importance-to-latency ratio, the paper's greedy algorithm generates a set <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-017-dark.svg"><img src="report/images/readme-math-017.svg" alt="\mathcal G" align="middle"></picture> of candidate contiguous intervals. For each candidate <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-018-dark.svg"><img src="report/images/readme-math-018.svg" alt="C\in\mathcal G" align="middle"></picture>, it computes

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-019-dark.svg"><img src="report/images/readme-math-019.svg" alt="U(C)=\frac{\sum_{i\in C}v_i}{T[|C|]}"></picture>
</p>

Candidates are considered in descending order of score. The mask is updated by selecting intervals that neither overlap previously selected rows nor exceed the remaining budget.

Define a candidate interval starting at row <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-020-dark.svg"><img src="report/images/readme-math-020.svg" alt="j" align="middle"></picture> with length <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-021-dark.svg"><img src="report/images/readme-math-021.svg" alt="\ell" align="middle"></picture> as follows.

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-022-dark.svg"><img src="report/images/readme-math-022.svg" alt="C_{j,\ell}=\{j,j+1,\ldots,j+\ell-1\}, \qquad 0\le j\le N-\ell"></picture>
</p>

Let <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-023-dark.svg"><img src="report/images/readme-math-023.svg" alt="\ell_{\min},\ell_{\max}" align="middle"></picture> denote the minimum and maximum candidate lengths in rows, respectively, <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-024-dark.svg"><img src="report/images/readme-math-024.svg" alt="\Delta\ell" align="middle"></picture> the length increment, and <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-025-dark.svg"><img src="report/images/readme-math-025.svg" alt="J" align="middle"></picture> the upper bound on the step between starting positions. The paper considers the following <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-026-dark.svg"><img src="report/images/readme-math-026.svg" alt="(j,\ell)" align="middle"></picture> pairs when constructing the candidate set <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-017-dark.svg"><img src="report/images/readme-math-017.svg" alt="\mathcal G" align="middle"></picture>.


<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-027-dark.svg"><img src="report/images/readme-math-027.svg" alt="\begin{aligned} \ell&amp;=\ell_{\min}+k\Delta\ell\le\min(\ell_{\max},N),\\ j&amp;=m\min(\ell,J)\le N-\ell, \end{aligned} \qquad k,m=0,1,2,\ldots"></picture>
</p>

This rule allows candidates of the same length to overlap when <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-028-dark.svg"><img src="report/images/readme-math-028.svg" alt="\ell&gt;J" align="middle"></picture>. The maximum length <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-029-dark.svg"><img src="report/images/readme-math-029.svg" alt="\ell_{\max}" align="middle"></picture> is determined by the device's throughput saturation point, while <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-030-dark.svg"><img src="report/images/readme-math-030.svg" alt="\ell_{\min}" align="middle"></picture>, <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-024-dark.svg"><img src="report/images/readme-math-024.svg" alt="\Delta\ell" align="middle"></picture>, and <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-025-dark.svg"><img src="report/images/readme-math-025.svg" alt="J" align="middle"></picture> control the granularity and computational cost of the search.

## 2. Problem Formulation and Proposed Method

### 2.1 The Actual Optimal Solution to the Paper's Formulation

Consider the Neuron Chunking problem formulated in the paper.

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-016-dark.svg"><img src="report/images/readme-math-016.svg" alt="\max_{\substack{M\in\{0,1\}^N\\1\le\|M\|_1\le R}} \frac{I(M)}{\widehat L_{\mathrm{table}}(M)}."></picture>
</p>

Assume <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-031-dark.svg"><img src="report/images/readme-math-031.svg" alt="1\le R\le N" align="middle"></picture> and <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-032-dark.svg"><img src="report/images/readme-math-032.svg" alt="T[\ell]&gt;0" align="middle"></picture> for every length <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-033-dark.svg"><img src="report/images/readme-math-033.svg" alt="1\le\ell\le R" align="middle"></picture>.

Let <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-034-dark.svg"><img src="report/images/readme-math-034.svg" alt="M^\star" align="middle"></picture> be any optimal mask under the row budget <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-035-dark.svg"><img src="report/images/readme-math-035.svg" alt="1\le\|M\|_1\le R" align="middle"></picture>. Since the intervals in <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-036-dark.svg"><img src="report/images/readme-math-036.svg" alt="\mathcal C(M^\star)" align="middle"></picture> partition the selected rows, we have

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-037-dark.svg"><img src="report/images/readme-math-037.svg" alt="\begin{aligned} \frac{I(M^\star)}{\widehat L_{\mathrm{table}}(M^\star)} &amp;= \sum_{C\in\mathcal C(M^\star)} \underbrace{ \frac{T[|C|]}{\widehat L_{\mathrm{table}}(M^\star)} }_{w_C} \underbrace{ \frac{\sum_{i\in C}v_i}{T[|C|]} }_{U(C)}\\ &amp;= \sum_{C\in\mathcal C(M^\star)}w_CU(C) \le \max_{C\in\mathcal C(M^\star)}U(C). \end{aligned}"></picture>
</p>

Since <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-038-dark.svg"><img src="report/images/readme-math-038.svg" alt="w_C&gt;0" align="middle"></picture> and <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-039-dark.svg"><img src="report/images/readme-math-039.svg" alt="\sum_Cw_C=1" align="middle"></picture>, the overall ratio is a latency-weighted average of the individual interval ratios. It therefore cannot exceed the largest interval ratio.

Let <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-040-dark.svg"><img src="report/images/readme-math-040.svg" alt="\widetilde M" align="middle"></picture> be the mask that retains only the interval <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-041-dark.svg"><img src="report/images/readme-math-041.svg" alt="C^\star\in\mathcal C(M^\star)" align="middle"></picture> with the largest <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-042-dark.svg"><img src="report/images/readme-math-042.svg" alt="U(C)" align="middle"></picture>. Since

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-043-dark.svg"><img src="report/images/readme-math-043.svg" alt="1\le\|\widetilde M\|_1=|C^\star|\le\|M^\star\|_1\le R"></picture>
</p>

the mask <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-040-dark.svg"><img src="report/images/readme-math-040.svg" alt="\widetilde M" align="middle"></picture> also satisfies the row budget. By the preceding inequality, the ratio of <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-040-dark.svg"><img src="report/images/readme-math-040.svg" alt="\widetilde M" align="middle"></picture> is no smaller than that of <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-034-dark.svg"><img src="report/images/readme-math-034.svg" alt="M^\star" align="middle"></picture>, and by the optimality of <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-034-dark.svg"><img src="report/images/readme-math-034.svg" alt="M^\star" align="middle"></picture>, it cannot be larger either. Thus, <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-040-dark.svg"><img src="report/images/readme-math-040.svg" alt="\widetilde M" align="middle"></picture> is also optimal.

An optimal solution consisting of a single contiguous interval therefore exists. The original problem reduces to finding the interval that maximizes <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-042-dark.svg"><img src="report/images/readme-math-042.svg" alt="U(C)" align="middle"></picture> among all contiguous intervals of length at most <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-015-dark.svg"><img src="report/images/readme-math-015.svg" alt="R" align="middle"></picture>.

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-044-dark.svg"><img src="report/images/readme-math-044.svg" alt="\max_{\substack{M\in\{0,1\}^N\\1\le\|M\|_1\le R}} \frac{I(M)}{\widehat L_{\mathrm{table}}(M)} = \max_{\substack{1\le\ell\le R\\0\le j\le N-\ell}} U(C_{j,\ell})."></picture>
</p>

The maximization on the right ranges over all contiguous intervals, not just those in the candidate set <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-017-dark.svg"><img src="report/images/readme-math-017.svg" alt="\mathcal G" align="middle"></picture>.

This occurs because the formulation only imposes an upper bound on the number of selected rows, rather than requiring exactly <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-015-dark.svg"><img src="report/images/readme-math-015.svg" alt="R" align="middle"></picture> rows. This study therefore considers the more restrictive problem of selecting exactly <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-015-dark.svg"><img src="report/images/readme-math-015.svg" alt="R" align="middle"></picture> rows.

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-045-dark.svg"><img src="report/images/readme-math-045.svg" alt="\boxed{ \max_{\substack{M\in\{0,1\}^N\\\|M\|_1=R}} \frac{I(M)}{\widehat L(M)}. }"></picture>
</p>

### 2.2 Two-Line Latency Model

#### 2.2.1 Read Cost of a Contiguous Interval

SSD throughput tends to increase with request size until it saturates at a certain level. Let <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-046-dark.svg"><img src="report/images/readme-math-046.svg" alt="\alpha&gt;0" align="middle"></picture> be the per-row read cost in the saturated regime. The latency of an I/O request spanning <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-021-dark.svg"><img src="report/images/readme-math-021.svg" alt="\ell" align="middle"></picture> rows can then be approximated as follows.

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-047-dark.svg"><img src="report/images/readme-math-047.svg" alt="T[\ell]=\alpha\ell, \qquad \ell\gg1."></picture>
</p>

To also capture the reduced throughput of small requests, this study approximates the measured table <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-048-dark.svg"><img src="report/images/readme-math-048.svg" alt="T[\ell]" align="middle"></picture> using the following two-line model.

Let <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-049-dark.svg"><img src="report/images/readme-math-049.svg" alt="T_{\mathrm{2L}}(\ell)" align="middle"></picture> be the cost of an interval of <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-021-dark.svg"><img src="report/images/readme-math-021.svg" alt="\ell" align="middle"></picture> rows. With a transition point <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-050-dark.svg"><img src="report/images/readme-math-050.svg" alt="\ell_0&gt;0" align="middle"></picture> expressed in rows and an additional cost coefficient <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-051-dark.svg"><img src="report/images/readme-math-051.svg" alt="0\le\lambda\le\alpha" align="middle"></picture>, the model is

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-052-dark.svg"><img src="report/images/readme-math-052.svg" alt="\boxed{ \begin{aligned} T_{\mathrm{2L}}(\ell) &amp;=\alpha\ell+\lambda(\ell_0-\ell)_+\\ &amp;= \begin{cases} (\alpha-\lambda)\ell+\lambda\ell_0, &amp;0&lt;\ell&lt;\ell_0,\\ \alpha\ell, &amp;\ell\ge\ell_0. \end{cases} \end{aligned} }"></picture>
</p>

Here, <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-053-dark.svg"><img src="report/images/readme-math-053.svg" alt="(x)_+=\max(x,0)" align="middle"></picture>. When there are no rows to read, we separately define <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-054-dark.svg"><img src="report/images/readme-math-054.svg" alt="T_{\mathrm{2L}}(0)=0" align="middle"></picture>.

The two lines meet continuously at <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-055-dark.svg"><img src="report/images/readme-math-055.svg" alt="\ell=\ell_0" align="middle"></picture>, and the throughput in rows per unit time is given by

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-056-dark.svg"><img src="report/images/readme-math-056.svg" alt="\frac{\ell}{T_{\mathrm{2L}}(\ell)} = \frac{1}{\alpha+\lambda(\ell_0/\ell-1)_+} \le\frac1\alpha"></picture>
</p>

If <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-057-dark.svg"><img src="report/images/readme-math-057.svg" alt="\lambda&gt;0" align="middle"></picture>, throughput increases with interval length before the transition point and remains constant at <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-058-dark.svg"><img src="report/images/readme-math-058.svg" alt="1/\alpha" align="middle"></picture> thereafter.

The coefficients <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-059-dark.svg"><img src="report/images/readme-math-059.svg" alt="\alpha,\lambda" align="middle"></picture> and the transition point <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-060-dark.svg"><img src="report/images/readme-math-060.svg" alt="\ell_0" align="middle"></picture> are determined by fitting the model to measured SSD read costs.

#### 2.2.2 Total Read Cost under a Fixed Row Budget

We now use this model to examine the total read cost of a mask <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-009-dark.svg"><img src="report/images/readme-math-009.svg" alt="M" align="middle"></picture> that selects exactly <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-015-dark.svg"><img src="report/images/readme-math-015.svg" alt="R" align="middle"></picture> rows. Even when the number of selected rows is fixed, the cost depends on how they form contiguous intervals. The two-line model expresses this difference as the additional cost of short intervals.

Apply the two-line model to the prior work's additive interval-cost structure. Since <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-061-dark.svg"><img src="report/images/readme-math-061.svg" alt="\sum_{C\in\mathcal C(M)}|C|=R" align="middle"></picture>, the total cost decomposes as follows.

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-062-dark.svg"><img src="report/images/readme-math-062.svg" alt="\begin{aligned} \widehat L_{\mathrm{2L}}(M) &amp;=\sum_{C\in\mathcal C(M)}T_{\mathrm{2L}}(|C|)\\ &amp;=\alpha R +\lambda\sum_{C\in\mathcal C(M)}(\ell_0-|C|)_+. \end{aligned}"></picture>
</p>

For a fixed <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-015-dark.svg"><img src="report/images/readme-math-015.svg" alt="R" align="middle"></picture>, differences in model cost are therefore determined by the length shortfalls of intervals shorter than the transition point. Each interval's additional cost decreases as its length approaches <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-060-dark.svg"><img src="report/images/readme-math-060.svg" alt="\ell_0" align="middle"></picture>, and becomes zero once that length is reached.

This decomposition also yields a lower bound on cost under a fixed row budget. Since every interval has length at most <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-015-dark.svg"><img src="report/images/readme-math-015.svg" alt="R" align="middle"></picture>, we have <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-063-dark.svg"><img src="report/images/readme-math-063.svg" alt="\sum_C(\ell_0-|C|)_+\ge(\ell_0-R)_+" align="middle"></picture>, and hence

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-064-dark.svg"><img src="report/images/readme-math-064.svg" alt="\widehat L_{\mathrm{2L}}(M) \ge \alpha R+\lambda(\ell_0-R)_+ =T_{\mathrm{2L}}(R)."></picture>
</p>

A single interval of length <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-015-dark.svg"><img src="report/images/readme-math-015.svg" alt="R" align="middle"></picture> attains this lower bound. When <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-057-dark.svg"><img src="report/images/readme-math-057.svg" alt="\lambda&gt;0" align="middle"></picture> and <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-065-dark.svg"><img src="report/images/readme-math-065.svg" alt="R&lt;\ell_0" align="middle"></picture>, only a single interval attains it. When <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-066-dark.svg"><img src="report/images/readme-math-066.svg" alt="R\ge\ell_0" align="middle"></picture>, any mask whose intervals all have length at least <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-060-dark.svg"><img src="report/images/readme-math-060.svg" alt="\ell_0" align="middle"></picture> attains the bound. Thus, even if the selected rows form multiple intervals, the model cost remains <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-067-dark.svg"><img src="report/images/readme-math-067.svg" alt="\alpha R" align="middle"></picture> as long as every interval is sufficiently long.

Changing which rows are selected also changes the retained importance <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-068-dark.svg"><img src="report/images/readme-math-068.svg" alt="I(M)" align="middle"></picture>. The next section therefore solves the following optimization problem, which accounts for both importance and the additional cost of short intervals.

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-069-dark.svg"><img src="report/images/readme-math-069.svg" alt="\max_{\substack{M\in\{0,1\}^N\\\|M\|_1=R}} \frac{I(M)} {\displaystyle \alpha R+ \lambda\sum_{C\in\mathcal C(M)}(\ell_0-|C|)_+ }."></picture>
</p>

### 2.3 Finding the Optimal Mask Using Dynamic Programming

Consider the problem of selecting exactly <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-015-dark.svg"><img src="report/images/readme-math-015.svg" alt="R" align="middle"></picture> rows under the two-line cost model defined in the previous section. For brevity, write <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-070-dark.svg"><img src="report/images/readme-math-070.svg" alt="T(\ell)=T_{\mathrm{2L}}(\ell)" align="middle"></picture> and <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-071-dark.svg"><img src="report/images/readme-math-071.svg" alt="L(M)=\widehat L_{\mathrm{2L}}(M)" align="middle"></picture>, and define the optimal ratio as

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-072-dark.svg"><img src="report/images/readme-math-072.svg" alt="q^\star= \max_{\substack{M\in\{0,1\}^N\\\|M\|_1=R}} \frac{I(M)}{L(M)}"></picture>
</p>

Here, <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-031-dark.svg"><img src="report/images/readme-math-031.svg" alt="1\le R\le N" align="middle"></picture> and <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-073-dark.svg"><img src="report/images/readme-math-073.svg" alt="v_i\ge0" align="middle"></picture>. Under the coefficient conditions in the previous section, every feasible mask has cost <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-074-dark.svg"><img src="report/images/readme-math-074.svg" alt="L(M)\ge T(R)&gt;0" align="middle"></picture>.

For a fixed ratio estimate <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-075-dark.svg"><img src="report/images/readme-math-075.svg" alt="q" align="middle"></picture>, the following additive objective can be maximized using dynamic programming (DP).

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-076-dark.svg"><img src="report/images/readme-math-076.svg" alt="F_R(q)= \max_{\substack{M\in\{0,1\}^N\\\|M\|_1=R}} \{I(M)-qL(M)\}."></picture>
</p>

We first describe how to solve this problem for a fixed <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-075-dark.svg"><img src="report/images/readme-math-075.svg" alt="q" align="middle"></picture>, then explain how to use the DP results to find <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-077-dark.svg"><img src="report/images/readme-math-077.svg" alt="q^\star" align="middle"></picture>.

#### 2.3.1 DP for a Fixed <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-075-dark.svg"><img src="report/images/readme-math-075.svg" alt="q" align="middle"></picture>

Index the rows by <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-078-dark.svg"><img src="report/images/readme-math-078.svg" alt="0,\ldots,N-1" align="middle"></picture> and define the importance prefix sums as <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-079-dark.svg"><img src="report/images/readme-math-079.svg" alt="P_j=\sum_{i=0}^{j-1}v_i" align="middle"></picture>, with <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-080-dark.svg"><img src="report/images/readme-math-080.svg" alt="P_0=0" align="middle"></picture>. Let <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-081-dark.svg"><img src="report/images/readme-math-081.svg" alt="D_{z,r}" align="middle"></picture> be the maximum value of <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-082-dark.svg"><img src="report/images/readme-math-082.svg" alt="I-qL" align="middle"></picture> when <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-083-dark.svg"><img src="report/images/readme-math-083.svg" alt="z" align="middle"></picture> rows are excluded and <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-084-dark.svg"><img src="report/images/readme-math-084.svg" alt="r" align="middle"></picture> rows are selected from the first <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-085-dark.svg"><img src="report/images/readme-math-085.svg" alt="z+r" align="middle"></picture> rows. The required state ranges are <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-086-dark.svg"><img src="report/images/readme-math-086.svg" alt="0\le z\le N-R" align="middle"></picture> and <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-087-dark.svg"><img src="report/images/readme-math-087.svg" alt="0\le r\le R" align="middle"></picture>.

For a mask with <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-088-dark.svg"><img src="report/images/readme-math-088.svg" alt="z\ge1" align="middle"></picture>, split it at the last unselected row. If <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-008-dark.svg"><img src="report/images/readme-math-008.svg" alt="t" align="middle"></picture> rows are selected before that row, the prefix contains <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-089-dark.svg"><img src="report/images/readme-math-089.svg" alt="z-1" align="middle"></picture> excluded rows and <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-008-dark.svg"><img src="report/images/readme-math-008.svg" alt="t" align="middle"></picture> selected rows. The last excluded row has index <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-090-dark.svg"><img src="report/images/readme-math-090.svg" alt="z+t-1" align="middle"></picture>, and the suffix <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-091-dark.svg"><img src="report/images/readme-math-091.svg" alt="[z+t,z+r)" align="middle"></picture> is a contiguous selected interval of length <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-092-dark.svg"><img src="report/images/readme-math-092.svg" alt="r-t" align="middle"></picture>.

Because the excluded row separates the prefix from the final interval, their costs can be added. The importance of the final interval is <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-093-dark.svg"><img src="report/images/readme-math-093.svg" alt="P_{z+r}-P_{z+t}" align="middle"></picture>, giving the recurrence

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-094-dark.svg"><img src="report/images/readme-math-094.svg" alt="\boxed{ D_{z,r} =\max_{0\le t\le r} \left\{ D_{z-1,t} +P_{z+r}-P_{z+t} -qT(r-t) \right\}. }"></picture>
</p>

When <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-095-dark.svg"><img src="report/images/readme-math-095.svg" alt="t=r" align="middle"></picture>, the mask ends with an unselected row and the final interval is empty. In this case, we use <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-096-dark.svg"><img src="report/images/readme-math-096.svg" alt="T(0)=0" align="middle"></picture>. Every mask admits this decomposition, so the recurrence obtains the optimal value by considering all <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-008-dark.svg"><img src="report/images/readme-math-008.svg" alt="t" align="middle"></picture>.

If no rows are excluded, all of the first <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-084-dark.svg"><img src="report/images/readme-math-084.svg" alt="r" align="middle"></picture> rows must be selected. The initial and final values are therefore

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-097-dark.svg"><img src="report/images/readme-math-097.svg" alt="D_{0,r}=P_r-qT(r), \qquad F_R(q)=D_{N-R,R}"></picture>
</p>

The optimal mask is reconstructed by backtracking from <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-098-dark.svg"><img src="report/images/readme-math-098.svg" alt="(z,r)=(N-R,R)" align="middle"></picture>. At each state, find a maximizing <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-008-dark.svg"><img src="report/images/readme-math-008.svg" alt="t" align="middle"></picture>, select <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-091-dark.svg"><img src="report/images/readme-math-091.svg" alt="[z+t,z+r)" align="middle"></picture>, and move to <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-099-dark.svg"><img src="report/images/readme-math-099.svg" alt="(z,r)\leftarrow(z-1,t)" align="middle"></picture>. Upon reaching <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-100-dark.svg"><img src="report/images/readme-math-100.svg" alt="z=0" align="middle"></picture>, select all of the remaining first <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-084-dark.svg"><img src="report/images/readme-math-084.svg" alt="r" align="middle"></picture> rows.

#### 2.3.2 Computing the DP Using the Two-Line Structure

Comparing all <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-008-dark.svg"><img src="report/images/readme-math-008.svg" alt="t" align="middle"></picture> independently at every state is computationally expensive. Under the two-line model, maximum computations can be shared by separating the final interval into three cases: empty, short, and long.

Define the boundary for classifying integer lengths as follows.

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-101-dark.svg"><img src="report/images/readme-math-101.svg" alt="h=\min(\lfloor\ell_0\rfloor,R)"></picture>
</p>

Use the short-interval cost formula when <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-102-dark.svg"><img src="report/images/readme-math-102.svg" alt="1\le r-t\le h" align="middle"></picture> and the long-interval cost formula when <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-103-dark.svg"><img src="report/images/readme-math-103.svg" alt="r-t&gt;h" align="middle"></picture>. If <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-060-dark.svg"><img src="report/images/readme-math-060.svg" alt="\ell_0" align="middle"></picture> is an integer, the two cost formulas agree at the boundary, so that length may be included in the short-interval case. The transition point <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-060-dark.svg"><img src="report/images/readme-math-060.svg" alt="\ell_0" align="middle"></picture> used in cost calculations is not rounded.

Collect the terms in the recurrence that depend on <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-008-dark.svg"><img src="report/images/readme-math-008.svg" alt="t" align="middle"></picture> as follows.

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-104-dark.svg"><img src="report/images/readme-math-104.svg" alt="\begin{aligned} A_{z,t}&amp;=D_{z-1,t}-P_{z+t}+q(\alpha-\lambda)t,\\ B_{z,t}&amp;=D_{z-1,t}-P_{z+t}+q\alpha t \end{aligned}"></picture>
</p>

The recurrence then becomes

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-105-dark.svg"><img src="report/images/readme-math-105.svg" alt="D_{z,r}= \max\left\{ \begin{aligned} &amp;D_{z-1,r},\\ &amp;P_{z+r}-q\bigl((\alpha-\lambda)r+\lambda\ell_0\bigr) +\max_{\max(0,r-h)\le t&lt;r}A_{z,t},\\ &amp;P_{z+r}-q\alpha r +\max_{0\le t&lt;r-h}B_{z,t} \end{aligned} \right\}."></picture>
</p>

The three terms correspond to an empty, short, and long final interval, respectively. The maximum over a range with no candidates is defined as <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-106-dark.svg"><img src="report/images/readme-math-106.svg" alt="-\infty" align="middle"></picture>.

As <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-084-dark.svg"><img src="report/images/readme-math-084.svg" alt="r" align="middle"></picture> increases, the query range for <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-107-dark.svg"><img src="report/images/readme-math-107.svg" alt="A_{z,t}" align="middle"></picture> is a sliding window of width <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-108-dark.svg"><img src="report/images/readme-math-108.svg" alt="h" align="middle"></picture>, while the query range for <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-109-dark.svg"><img src="report/images/readme-math-109.svg" alt="B_{z,t}" align="middle"></picture> is a prefix whose right endpoint advances. Using sliding-window maxima and prefix maxima, all values of <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-084-dark.svg"><img src="report/images/readme-math-084.svg" alt="r" align="middle"></picture> for a fixed <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-083-dark.svg"><img src="report/images/readme-math-083.svg" alt="z" align="middle"></picture> can therefore be computed in <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-110-dark.svg"><img src="report/images/readme-math-110.svg" alt="O(R+1)" align="middle"></picture> time.

The DP table has size <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-111-dark.svg"><img src="report/images/readme-math-111.svg" alt="(N-R+1)(R+1)" align="middle"></picture>. Backtracking also compares at most <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-112-dark.svg"><img src="report/images/readme-math-112.svg" alt="R+1" align="middle"></picture> candidates for each <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-083-dark.svg"><img src="report/images/readme-math-083.svg" alt="z" align="middle"></picture>, so both the time and memory required for DP computation and mask reconstruction at a fixed <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-075-dark.svg"><img src="report/images/readme-math-075.svg" alt="q" align="middle"></picture> are

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-113-dark.svg"><img src="report/images/readme-math-113.svg" alt="O\bigl((N-R+1)(R+1)\bigr)"></picture>
</p>

#### 2.3.3 Searching for the Optimal Ratio <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-075-dark.svg"><img src="report/images/readme-math-075.svg" alt="q" align="middle"></picture>

For any feasible mask <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-009-dark.svg"><img src="report/images/readme-math-009.svg" alt="M" align="middle"></picture>, we have

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-114-dark.svg"><img src="report/images/readme-math-114.svg" alt="I(M)-qL(M) =L(M)\left(\frac{I(M)}{L(M)}-q\right)"></picture>
</p>

Since <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-115-dark.svg"><img src="report/images/readme-math-115.svg" alt="L(M)&gt;0" align="middle"></picture>, the following holds.

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-116-dark.svg"><img src="report/images/readme-math-116.svg" alt="F_R(q) \begin{cases} &gt;0,&amp;q&lt;q^\star,\\ =0,&amp;q=q^\star,\\ &lt;0,&amp;q&gt;q^\star. \end{cases}"></picture>
</p>

The optimal ratio is the root of <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-117-dark.svg"><img src="report/images/readme-math-117.svg" alt="F_R(q)=0" align="middle"></picture>. We use Dinkelbach's method to find this root.

Let <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-118-dark.svg"><img src="report/images/readme-math-118.svg" alt="M_C" align="middle"></picture> be the mask selecting the contiguous interval of length <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-015-dark.svg"><img src="report/images/readme-math-015.svg" alt="R" align="middle"></picture> with the greatest importance, and let <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-119-dark.svg"><img src="report/images/readme-math-119.svg" alt="M_T" align="middle"></picture> be the mask selecting the <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-015-dark.svg"><img src="report/images/readme-math-015.svg" alt="R" align="middle"></picture> rows with the highest importance.

The cost of <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-118-dark.svg"><img src="report/images/readme-math-118.svg" alt="M_C" align="middle"></picture> is <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-120-dark.svg"><img src="report/images/readme-math-120.svg" alt="T(R)" align="middle"></picture>, and every feasible mask satisfies <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-121-dark.svg"><img src="report/images/readme-math-121.svg" alt="I(M)\le I(M_T)" align="middle"></picture> and <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-122-dark.svg"><img src="report/images/readme-math-122.svg" alt="T(R)\le L(M)" align="middle"></picture>. We therefore initialize the lower and upper bounds as follows.

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-123-dark.svg"><img src="report/images/readme-math-123.svg" alt="q_0=\frac{I(M_C)}{T(R)} \le q^\star\le u_0=\frac{I(M_T)}{T(R)}"></picture>
</p>

At iteration <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-124-dark.svg"><img src="report/images/readme-math-124.svg" alt="k" align="middle"></picture>, solve the DP with <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-125-dark.svg"><img src="report/images/readme-math-125.svg" alt="q=q_k" align="middle"></picture> and denote the reconstructed mask by <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-126-dark.svg"><img src="report/images/readme-math-126.svg" alt="M_k" align="middle"></picture>. Using <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-127-dark.svg"><img src="report/images/readme-math-127.svg" alt="F_R(q_k)=I(M_k)-q_kL(M_k)" align="middle"></picture>, update the ratio as follows.

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-128-dark.svg"><img src="report/images/readme-math-128.svg" alt="\boxed{ \begin{aligned} q_{k+1} &amp;=\frac{I(M_k)}{L(M_k)}\\ &amp;=\frac{q_kL(M_k)+F_R(q_k)}{L(M_k)}\\ &amp;=q_k+\frac{F_R(q_k)}{L(M_k)} \end{aligned} }"></picture>
</p>

If <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-129-dark.svg"><img src="report/images/readme-math-129.svg" alt="q_k\le q^\star" align="middle"></picture>, then <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-130-dark.svg"><img src="report/images/readme-math-130.svg" alt="F_R(q_k)\ge0" align="middle"></picture>. Since <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-126-dark.svg"><img src="report/images/readme-math-126.svg" alt="M_k" align="middle"></picture> also selects exactly <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-015-dark.svg"><img src="report/images/readme-math-015.svg" alt="R" align="middle"></picture> rows, we have <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-131-dark.svg"><img src="report/images/readme-math-131.svg" alt="q_k\le q_{k+1}\le q^\star" align="middle"></picture>. The ratio increases while <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-132-dark.svg"><img src="report/images/readme-math-132.svg" alt="F_R(q_k)&gt;0" align="middle"></picture>, and when <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-133-dark.svg"><img src="report/images/readme-math-133.svg" alt="F_R(q_k)=0" align="middle"></picture>, <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-126-dark.svg"><img src="report/images/readme-math-126.svg" alt="M_k" align="middle"></picture> is an optimal solution to the original ratio problem. Because the number of feasible masks is finite, the method reaches the optimum in finitely many iterations with exact DP solutions and exact arithmetic.

#### 2.3.4 Updating the Upper Bound Using the Residual

For any optimal mask <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-034-dark.svg"><img src="report/images/readme-math-034.svg" alt="M^\star" align="middle"></picture>, we have <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-134-dark.svg"><img src="report/images/readme-math-134.svg" alt="I(M^\star)=q^\star L(M^\star)" align="middle"></picture>.

We also have <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-135-dark.svg"><img src="report/images/readme-math-135.svg" alt="q^\star-q_k\ge0" align="middle"></picture> and <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-136-dark.svg"><img src="report/images/readme-math-136.svg" alt="T(R)\le L(M^\star)" align="middle"></picture>.

By the definition of <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-137-dark.svg"><img src="report/images/readme-math-137.svg" alt="F_R(q_k)" align="middle"></picture>, we have <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-138-dark.svg"><img src="report/images/readme-math-138.svg" alt="I(M^\star)-q_kL(M^\star)\le F_R(q_k)" align="middle"></picture>, and therefore

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-139-dark.svg"><img src="report/images/readme-math-139.svg" alt="\begin{aligned} (q^\star-q_k)T(R) &amp;\le(q^\star-q_k)L(M^\star)\\ &amp;=I(M^\star)-q_kL(M^\star)\\ &amp;\le F_R(q_k). \end{aligned}"></picture>
</p>

Dividing by <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-140-dark.svg"><img src="report/images/readme-math-140.svg" alt="T(R)&gt;0" align="middle"></picture> and adding <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-141-dark.svg"><img src="report/images/readme-math-141.svg" alt="q_k" align="middle"></picture> to both sides yields

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-142-dark.svg"><img src="report/images/readme-math-142.svg" alt="q^\star \le q_k+\frac{F_R(q_k)}{T(R)}."></picture>
</p>

Both the existing upper bound <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-143-dark.svg"><img src="report/images/readme-math-143.svg" alt="u_k" align="middle"></picture> and the right-hand side above are at least <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-077-dark.svg"><img src="report/images/readme-math-077.svg" alt="q^\star" align="middle"></picture>, so we update the upper bound as follows.

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-144-dark.svg"><img src="report/images/readme-math-144.svg" alt="u_{k+1} =\min\left\{ u_k,\; q_k+\frac{F_R(q_k)}{T(R)} \right\}"></picture>
</p>

At each iteration, we maintain <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-145-dark.svg"><img src="report/images/readme-math-145.svg" alt="q_k\le q^\star\le u_k" align="middle"></picture>. When <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-146-dark.svg"><img src="report/images/readme-math-146.svg" alt="q_0&gt;0" align="middle"></picture>, we terminate once

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-147-dark.svg"><img src="report/images/readme-math-147.svg" alt="u_k-q_k\le\varepsilon q_0"></picture>
</p>

The implementation uses float64 arithmetic and <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-148-dark.svg"><img src="report/images/readme-math-148.svg" alt="\varepsilon=10^{-6}" align="middle"></picture>. With <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-149-dark.svg"><img src="report/images/readme-math-149.svg" alt="K" align="middle"></picture> DP calls, the total time for DP computation and mask reconstruction is <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-150-dark.svg"><img src="report/images/readme-math-150.svg" alt="O(K(N-R+1)(R+1))" align="middle"></picture>.

### 2.4 Tile-Based Row Selection

Tiles selects rows in tiles of a fixed length and fills the remaining budget with a single contiguous interval.

Let the tile length be a positive integer <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-151-dark.svg"><img src="report/images/readme-math-151.svg" alt="B" align="middle"></picture>. Starting at rows <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-152-dark.svg"><img src="report/images/readme-math-152.svg" alt="0" align="middle"></picture> and <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-153-dark.svg"><img src="report/images/readme-math-153.svg" alt="d=\lfloor B/2\rfloor" align="middle"></picture> gives the following two tile arrangements.

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-154-dark.svg"><img src="report/images/readme-math-154.svg" alt="\boxed{ \begin{aligned} \mathcal T_1&amp;=\{C_{0,B},C_{B,B},C_{2B,B},\ldots\},\\ \mathcal T_2&amp;=\{C_{d,B},C_{d+B,B},C_{d+2B,B},\ldots\} \end{aligned} }"></picture>
</p>

Tiles whose final row exceeds <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-155-dark.svg"><img src="report/images/readme-math-155.svg" alt="N-1" align="middle"></picture> are discarded, and a mask is constructed independently for each arrangement.

Decompose the row budget as follows.

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-156-dark.svg"><img src="report/images/readme-math-156.svg" alt="R=KB+r, \qquad K=\left\lfloor\frac RB\right\rfloor, \qquad 0\le r&lt;B"></picture>
</p>

For each arrangement, select the <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-149-dark.svg"><img src="report/images/readme-math-149.svg" alt="K" align="middle"></picture> tiles with the largest importance <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-157-dark.svg"><img src="report/images/readme-math-157.svg" alt="\sum_{i\in C}v_i" align="middle"></picture>.

If <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-158-dark.svg"><img src="report/images/readme-math-158.svg" alt="r&gt;0" align="middle"></picture>, add the contiguous interval of length <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-084-dark.svg"><img src="report/images/readme-math-084.svg" alt="r" align="middle"></picture> with the greatest importance among those that do not overlap the selected tiles. If <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-159-dark.svg"><img src="report/images/readme-math-159.svg" alt="R&lt;B" align="middle"></picture>, select the contiguous interval of length <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-015-dark.svg"><img src="report/images/readme-math-015.svg" alt="R" align="middle"></picture> with the greatest importance.

Among the masks that fill the budget, choose the one with the larger <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-160-dark.svg"><img src="report/images/readme-math-160.svg" alt="I(M)/L(M)" align="middle"></picture> as <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-161-dark.svg"><img src="report/images/readme-math-161.svg" alt="M_{\mathrm{Tiles}}" align="middle"></picture>.

If <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-162-dark.svg"><img src="report/images/readme-math-162.svg" alt="B\ge\ell_0" align="middle"></picture> and <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-163-dark.svg"><img src="report/images/readme-math-163.svg" alt="R\ge B" align="middle"></picture>, additional cost under the two-line model can arise only from a residual interval that does not merge with a full tile. Therefore,

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-164-dark.svg"><img src="report/images/readme-math-164.svg" alt="\alpha R \le L(M_{\mathrm{Tiles}}) \le\alpha(R-r)+T(r)."></picture>
</p>

If <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-165-dark.svg"><img src="report/images/readme-math-165.svg" alt="r=0" align="middle"></picture> or the residual interval merges with a full tile, then <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-166-dark.svg"><img src="report/images/readme-math-166.svg" alt="L(M_{\mathrm{Tiles}})=\alpha R" align="middle"></picture>. When <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-167-dark.svg"><img src="report/images/readme-math-167.svg" alt="B=1" align="middle"></picture>, Tiles is equivalent to Top-<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-015-dark.svg"><img src="report/images/readme-math-015.svg" alt="R" align="middle"></picture>.

Using importance prefix sums and partial selection of the top <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-149-dark.svg"><img src="report/images/readme-math-149.svg" alt="K" align="middle"></picture> tiles, the method has average time complexity <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-168-dark.svg"><img src="report/images/readme-math-168.svg" alt="O(N)" align="middle"></picture> and memory complexity <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-168-dark.svg"><img src="report/images/readme-math-168.svg" alt="O(N)" align="middle"></picture> for a fixed <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-151-dark.svg"><img src="report/images/readme-math-151.svg" alt="B" align="middle"></picture>.


### 2.5 Minimum Interval Length Constraints and Guarantees Relative to the Optimum

Under the two-line model, masks that select the same number of rows have the same read cost if all their intervals are sufficiently long. We use this property to formulate an optimization problem with a length constraint and assess how close its solution is to the original optimum in terms of the importance retention ratio.

Let <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-169-dark.svg"><img src="report/images/readme-math-169.svg" alt="q(M)=I(M)/L(M)" align="middle"></picture> be the importance-to-latency ratio of mask <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-009-dark.svg"><img src="report/images/readme-math-009.svg" alt="M" align="middle"></picture>.

#### 2.5.1 Optimal Solution under the Length Constraint

Add the requirement that every maximal contiguous interval <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-170-dark.svg"><img src="report/images/readme-math-170.svg" alt="C\in\mathcal C(M)" align="middle"></picture> have length at least <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-060-dark.svg"><img src="report/images/readme-math-060.svg" alt="\ell_0" align="middle"></picture> to the problem of selecting exactly <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-015-dark.svg"><img src="report/images/readme-math-015.svg" alt="R" align="middle"></picture> rows. To ensure feasibility, assume <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-171-dark.svg"><img src="report/images/readme-math-171.svg" alt="\lceil\ell_0\rceil\le R\le N" align="middle"></picture>. A single interval of length <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-015-dark.svg"><img src="report/images/readme-math-015.svg" alt="R" align="middle"></picture> then always satisfies the constraint.

Each interval satisfies <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-172-dark.svg"><img src="report/images/readme-math-172.svg" alt="T(|C|)=\alpha|C|" align="middle"></picture>, and the interval lengths sum to <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-015-dark.svg"><img src="report/images/readme-math-015.svg" alt="R" align="middle"></picture>, so the total read cost is

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-173-dark.svg"><img src="report/images/readme-math-173.svg" alt="\begin{aligned} L(M) &amp;=\sum_{C\in\mathcal C(M)}T(|C|)\\ &amp;=\alpha\sum_{C\in\mathcal C(M)}|C| =\alpha R. \end{aligned}"></picture>
</p>

The read cost is therefore the same regardless of the positions of the selected rows or the number of intervals. Because the denominator of the importance-to-latency ratio is fixed, the mask that maximizes importance under this constraint also maximizes the ratio.

Denote this mask by <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-174-dark.svg"><img src="report/images/readme-math-174.svg" alt="M_{\mathrm{sat}}^\star" align="middle"></picture> and its retained importance by <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-175-dark.svg"><img src="report/images/readme-math-175.svg" alt="I_{\mathrm{sat}}=I(M_{\mathrm{sat}}^\star)" align="middle"></picture>. The optimal ratio under the length constraint is

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-176-dark.svg"><img src="report/images/readme-math-176.svg" alt="q_{\mathrm{sat}} =q(M_{\mathrm{sat}}^\star) =\frac{I_{\mathrm{sat}}}{\alpha R}"></picture>
</p>

#### 2.5.2 A Guarantee for General Masks

We now compare this with the optimal ratio <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-077-dark.svg"><img src="report/images/readme-math-077.svg" alt="q^\star" align="middle"></picture> for the same row budget without the length constraint.

Let <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-119-dark.svg"><img src="report/images/readme-math-119.svg" alt="M_T" align="middle"></picture> be the Top-<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-015-dark.svg"><img src="report/images/readme-math-015.svg" alt="R" align="middle"></picture> mask that selects the <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-015-dark.svg"><img src="report/images/readme-math-015.svg" alt="R" align="middle"></picture> rows with the highest importance, and let <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-177-dark.svg"><img src="report/images/readme-math-177.svg" alt="I_T=I(M_T)" align="middle"></picture> be its importance. Every mask selecting exactly <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-015-dark.svg"><img src="report/images/readme-math-015.svg" alt="R" align="middle"></picture> rows satisfies <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-178-dark.svg"><img src="report/images/readme-math-178.svg" alt="I(M)\le I_T" align="middle"></picture>. Since the two-line model also gives <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-179-dark.svg"><img src="report/images/readme-math-179.svg" alt="L(M)\ge\alpha R" align="middle"></picture>, we have

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-180-dark.svg"><img src="report/images/readme-math-180.svg" alt="q^\star\le\frac{I_T}{\alpha R}."></picture>
</p>

This upper bound combines the importance upper bound <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-181-dark.svg"><img src="report/images/readme-math-181.svg" alt="I_T" align="middle"></picture> with the cost lower bound <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-067-dark.svg"><img src="report/images/readme-math-067.svg" alt="\alpha R" align="middle"></picture>. It holds even if no mask attains both values simultaneously.

If <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-182-dark.svg"><img src="report/images/readme-math-182.svg" alt="I_T&gt;0" align="middle"></picture>, then <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-183-dark.svg"><img src="report/images/readme-math-183.svg" alt="q^\star&gt;0" align="middle"></picture>. Since <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-184-dark.svg"><img src="report/images/readme-math-184.svg" alt="q(M)\ge0" align="middle"></picture> for any mask <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-009-dark.svg"><img src="report/images/readme-math-009.svg" alt="M" align="middle"></picture> selecting exactly <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-015-dark.svg"><img src="report/images/readme-math-015.svg" alt="R" align="middle"></picture> rows, we obtain

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-185-dark.svg"><img src="report/images/readme-math-185.svg" alt="\boxed{ \begin{aligned} \frac{q(M)}{q^\star} &amp;\ge\frac{q(M)}{I_T/(\alpha R)}\\ &amp;=\frac{I(M)}{I_T}\, \frac{\alpha R}{L(M)}. \end{aligned} }"></picture>
</p>

The two factors in the final expression on the right are the importance retention ratio relative to Top-<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-015-dark.svg"><img src="report/images/readme-math-015.svg" alt="R" align="middle"></picture> and the fraction of the maximum read efficiency achieved, respectively. The latter is the throughput in rows per unit time, <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-186-dark.svg"><img src="report/images/readme-math-186.svg" alt="R/L(M)" align="middle"></picture>, divided by its upper bound <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-058-dark.svg"><img src="report/images/readme-math-058.svg" alt="1/\alpha" align="middle"></picture>. Both factors lie between <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-152-dark.svg"><img src="report/images/readme-math-152.svg" alt="0" align="middle"></picture> and <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-187-dark.svg"><img src="report/images/readme-math-187.svg" alt="1" align="middle"></picture>, and their product gives a lower bound on the fraction of the optimal ratio achieved.

#### 2.5.3 A Guarantee Based on Importance Retention

A mask whose maximal contiguous intervals all have length at least <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-060-dark.svg"><img src="report/images/readme-math-060.svg" alt="\ell_0" align="middle"></picture> attains the cost lower bound <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-188-dark.svg"><img src="report/images/readme-math-188.svg" alt="L(M)=\alpha R" align="middle"></picture>. The read-efficiency factor in the preceding guarantee therefore equals <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-187-dark.svg"><img src="report/images/readme-math-187.svg" alt="1" align="middle"></picture>, giving

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-189-dark.svg"><img src="report/images/readme-math-189.svg" alt="\frac{q(M)}{q^\star} \ge \frac{I(M)}{I_T} \underbrace{\frac{\alpha R}{L(M)}}_{=1} =\frac{I(M)}{I_T}."></picture>
</p>

Under this condition, the read cost cannot be reduced further, so the importance retention ratio alone guarantees the fraction of the optimal ratio achieved. Even if the importance upper bound <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-181-dark.svg"><img src="report/images/readme-math-181.svg" alt="I_T" align="middle"></picture> is not reached, the cost lower bound has already been attained.

In particular, applying this result to the optimal mask <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-174-dark.svg"><img src="report/images/readme-math-174.svg" alt="M_{\mathrm{sat}}^\star" align="middle"></picture> under the length constraint yields

<p align="center">
<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-190-dark.svg"><img src="report/images/readme-math-190.svg" alt="\boxed{ \frac{q_{\mathrm{sat}}}{q^\star} \ge \frac{I_{\mathrm{sat}}}{I_T}. }"></picture>
</p>

Since <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-174-dark.svg"><img src="report/images/readme-math-174.svg" alt="M_{\mathrm{sat}}^\star" align="middle"></picture> is also feasible for the original problem, <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-191-dark.svg"><img src="report/images/readme-math-191.svg" alt="q_{\mathrm{sat}}\le q^\star" align="middle"></picture>. Thus, preserving at least <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-192-dark.svg"><img src="report/images/readme-math-192.svg" alt="95\%" align="middle"></picture> of the Top-<picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-015-dark.svg"><img src="report/images/readme-math-015.svg" alt="R" align="middle"></picture> importance under the length constraint guarantees an importance-to-latency ratio of at least <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-192-dark.svg"><img src="report/images/readme-math-192.svg" alt="95\%" align="middle"></picture> of the original optimum.

This guarantee can be checked using only <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-193-dark.svg"><img src="report/images/readme-math-193.svg" alt="I_{\mathrm{sat}}" align="middle"></picture> and <picture><source media="(prefers-color-scheme: dark)" srcset="report/images/readme-math-181-dark.svg"><img src="report/images/readme-math-181.svg" alt="I_T" align="middle"></picture>, without explicitly finding the original optimal mask. The attainment of the cost lower bound and the ratio guarantee are both based on the estimated costs of the two-line model.
