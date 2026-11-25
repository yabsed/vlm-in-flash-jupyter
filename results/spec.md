# Measurement machine

Every result in this directory was measured on one laptop in October 2026.
The timing runs were 01 on 2026-10-20 and 03 on 2026-10-29.

## Hardware

| Item | Value |
|---|---|
| Model | Lenovo IdeaPad Pro 5 16AHP9 (83D5) |
| CPU | AMD Ryzen 7 8845HS, 8 cores / 16 threads, x86_64 |
| Memory | 13.4 GiB usable |
| GPU | NVIDIA GeForce RTX 3050 Laptop, 6 GB |
| SSD | WD_BLACK SN850X 1 TB, NVMe, PCIe 4.0 x4, firmware 620361WD |

## Storage

| Item | Value |
|---|---|
| File system | btrfs, mounted with `compress=zstd:1,ssd,discard=async` |
| I/O scheduler | `none` |
| Block size | 512 B, logical and physical |
| Largest single request | 128 KiB (`max_sectors_kb`) |
| Reads | `O_DIRECT`, 6 reader threads |

## Software

| Item | Value |
|---|---|
| OS | Fedora Linux 44, kernel 7.2.10-200.fc44, glibc 2.43 |
| PyTorch | 2.11.0+cu130 (CUDA 13.0) |
| Python | 3.13.11 |
| Libraries | NumPy 2.4.0, pandas 2.3.3, SciPy 1.17.0, Matplotlib 3.10.8, Transformers 5.2.0 |

The OS and PyTorch versions come from [03/environment.json](03/environment.json).
Python and the libraries were read on 2026-11-25, after the runs, and may have changed since then.

## Notes

- `environment.json` names the device `nvme1n1p3`. The kernel numbers NVMe drives differently between boots; `/home` is on the SN850X.
- The power profile and CPU governor during the runs were not recorded.
- The laptop now runs Fedora 45 with kernel 7.3.3, so a rerun would use a different kernel and compiler.
