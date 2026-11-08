# Tile-Based Selective Weight Loading Accounting for SSD Throughput Saturation

According to “VLM in a Flash,” a study by Professor Youngki Lee’s group at Seoul National University, mobile devices cannot load all LLM weights into DRAM and must therefore select the most important rows. Even when reading the same amount of data from an SSD, reading contiguous data in a single operation takes less time. The researchers modeled reads by grouping consecutive rows into chunks and defining total read time as follows:

```
(Total read time) = (Sum of the read times for individual chunks)
```

The read time for each chunk depends on its length. To maximize importance per unit of read time while reading at most R rows, they assigned an importance score to each row, generated candidate chunks, and ranked them by this ratio.

However, a close mathematical examination of their cost function reveals that the ratio of total importance to total read time is a weighted average of the individual chunks’ importance-to-time ratios, with read times as the weights. Therefore, selecting only the chunk with the highest ratio can yield an optimal solution. This does not reflect the problem the paper intended to solve.

What if we reformulate the problem to require reading exactly R rows, rather than at most R rows?

SSD throughput saturates once the read size exceeds a threshold:

```
(Read time for a large chunk) = (Constant) x (Chunk length)
```

If every chunk exceeds this threshold (approximately 200 MB), total read time is proportional only to R, the total number of selected rows. Because the row count is fixed at exactly R, maximizing total importance per unit of read time becomes equivalent to maximizing total importance.

Building on this property, I am developing a greedy algorithm to minimize the number of candidate chunks as part of my undergraduate thesis research.
