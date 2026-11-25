# Measurement machine

Results 01–03 in this directory were measured on one NVIDIA Jetson Orin Nano developer kit on 2026-11-25.
The timing runs, 01 and 03, finished at 09:03 and 10:42 on the board's clock (EST, UTC−5).
The laptop that produced `results/` is described in [../results/spec.md](../results/spec.md).

## Hardware

| Item | Value |
|---|---|
| Model | NVIDIA Jetson Orin Nano Engineering Reference Developer Kit Super (module P3767-0005 on carrier P3768-0000) |
| CPU | Arm Cortex-A78AE, 6 cores / 6 threads, aarch64, 1.728 GHz maximum |
| Memory | 7.4 GiB usable, shared by CPU and GPU; 3.7 GiB of zram swap |
| GPU | Integrated NVIDIA Orin GPU (Tegra234), no memory of its own |
| SSD | SK hynix Gold P31 500 GB (SHGP31-500GM), NVMe, PCIe 3.0 x4, firmware 41062C20 |
| System disk | 477.5 GiB SD card, holding `/` and `/home` |

The repository and every measured read are on the SSD. The SD card holds the OS and the caches under `~/.cache`.

## Storage

| Item | Value |
|---|---|
| File system | ext4 directly on `/dev/nvme0n1` (no partition table), mounted at `/mnt/nvme` with `rw,relatime` |
| I/O scheduler | `none` |
| Block size | 512 B, logical and physical |
| Largest single request | 256 KiB (`max_sectors_kb`, equal to the hardware limit) |
| Queues | 6 I/O queues, MSI-X interrupts |
| Reads | `O_DIRECT`, 6 reader threads |

## Run conditions

| Item | Value during the runs | After a reboot |
|---|---|---|
| Power mode | `MAXN_SUPER` (`nvpmodel` mode 2) | kept |
| CPU clocks | all six cores pinned at 1.728 GHz by `jetson_clocks` | unpinned: governor `schedutil`, 729.6 MHz to 1.728 GHz |
| NVMe I/O timeout | 1000 ms | 30000 ms |

`01_01` and `03_02` check all three before they measure, and their saved output shows `('MAXN_SUPER', [(1728000, 1728000, 1728000)])`.
`sudo` asks for a password on this board, so these are set by hand after every boot:

```bash
sudo nvpmodel -m 2      # only if the mode is not MAXN_SUPER
sudo jetson_clocks
echo 1000 | sudo tee /sys/block/nvme0n1/queue/io_timeout
```

## Software

| Item | Value |
|---|---|
| OS | Ubuntu 22.04.5 LTS with Jetson Linux (L4T) R36.4.4, the JetPack 6.2.1 release; kernel 5.15.148-tegra, glibc 2.35 |
| CUDA | 12.6 (toolkit 12.6.11), cuDNN 9.3.0, TensorRT 10.3.0 |
| PyTorch | 2.11.0 built for CUDA 12.6, torchvision 0.26.0 |
| Python | 3.10.12, in the virtual environment `jetson/.venv` |
| Libraries | NumPy 2.2.6, pandas 2.3.3, SciPy 1.15.3, Matplotlib 3.10.8, Transformers 5.2.0 |
| Compiler | GCC 11.4.0 with ninja 1.13.2, for the C++ extensions built by `torch.utils.cpp_extension` |

The OS, PyTorch version, power mode, CPU clocks and I/O timeout come from [03/environment.json](03/environment.json).
Everything else was read on 2026-11-25, within an hour of the last run.

## Python environment

`jetson/.venv` was built on 2026-11-25. A rebuild has to repeat these steps:

- PyTorch comes from the Jetson AI Lab index, alone and without dependencies:
  `pip install --no-deps --index-url https://pypi.jetson-ai-lab.io/jp6/cu126 torch==2.11.0`.
  Everything else comes from PyPI. This PyTorch needs `setuptools<82`; 81.0.0 is installed.
- That PyTorch needs `libcudss.so.0`, which is not installed on the board. `libcudss.so.0.8.0` from NVIDIA's cuDSS 0.8.0.10 archive for linux-aarch64 and CUDA 12 was copied to `site-packages/torch/lib/libcudss.so.0`.
- `site-packages/_venv_bin_on_path.pth` puts the environment's `bin` on `PATH`, so a kernel started without activating the environment still finds ninja.
- VS Code does not discover an environment nested in `jetson/`. The machine setting `"python.venvPath": "/mnt/nvme/handong/vlm-in-flash-jupyter/jetson"` in `~/.vscode-server/data/Machine/settings.json` fixes that.
- Models and datasets download to `~/.cache/huggingface`, and compiled extensions go to `~/.cache/torch_extensions`. Both are on the SD card.

## Notes

- **Stalled reads.** A direct read on this board sometimes hangs until the kernel's NVMe timeout fires. The kernel log then says `nvme nvme0: I/O … timeout, completion polled`: the read had finished, but its interrupt never arrived. With the timeout at 1000 ms a stall costs about 1 s instead of 30 s. `01_01` and `03_02` discard any read that takes 500 ms or longer and measure it again; `01_01` discarded 229 reads, and `03_02` discarded 92 and listed them in [03/stalls.csv](03/stalls.csv). The cause is not known. Turning off the drive's autonomous power-state transitions did not remove the stalls.
- **Watchdog resets.** The board froze and was reset by its watchdog twice on 2026-11-25, at 09:35 and at about 11:03 (`reset_reason` = `BCCPLEXWDT`). Both times other work was running beside the notebooks and little memory was free. The first reset cut off `02_02`, which was run again. Neither reset fell inside a timing run. The cause is not established.
- **Memory.** With VS Code Remote attached and no notebook kernel running, 2.6–3.9 GiB was available when this file was written.
- Temperatures, fan speed and the power supply during the runs were not recorded.
- The board's clock is set to America/New_York. Korean time is 14 hours ahead, so the 03 run finished at 00:42 on 2026-11-26 in Korea.
