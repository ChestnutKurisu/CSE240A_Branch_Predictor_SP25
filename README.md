# CSE240A Branch Predictor Project

This repository contains an implementation of three branch prediction techniques:

1. **G-Share**  
2. **Tournament**  
3. **Custom TAGE Predictor**  

The code is designed to simulate branch prediction across multiple address traces and generate statistics (such as the misprediction rate) for each of the predictors. The project follows typical course requirements for implementing and analyzing branch predictors in a Computer Architecture class.

---

## Table of Contents

- [Overview](#overview)
- [Project Structure](#project-structure)
- [Requirements](#requirements)
- [Installation and Setup](#installation-and-setup)
- [Docker Setup and Usage](#docker-setup-and-usage)
- [Usage](#usage)
  - [Running Built-in Experiments](#running-built-in-experiments)
  - [Extended Experiments](#extended-experiments)
  - [Visualizations](#visualizations)
- [Implementation Details](#implementation-details)
  - [1. G-Share Predictor](#1-g-share-predictor)
  - [2. Tournament Predictor](#2-tournament-predictor)
  - [3. Custom TAGE Predictor](#3-custom-tage-predictor)
- [Results](#results)
- [Memory Usage](#memory-usage)
- [Contributing](#contributing)

---

## Overview

Branch prediction is a vital technique in modern CPUs to mitigate control hazards. Predicting branch outcomes (taken or not-taken) correctly most of the time reduces pipeline stalls and improves instruction throughput.

This project implements and compares:
- **G-Share**: A global history-based predictor that XORs the global history register with bits from the PC to index into a table of 2-bit saturating counters.
- **Tournament**: Combines both global and local history predictors, plus a meta-predictor to decide which one to trust.
- **Custom (TAGE)**: A more sophisticated predictor that uses multiple predictor banks (tagged components) to handle various history lengths, providing high accuracy.

We evaluated these predictors on six trace files (`int_1`, `int_2`, `fp_1`, `fp_2`, `mm_1`, `mm_2`).

---

## Project Structure

```
CSE240A_Branch_Predictor_SP25/
├─ latex-report/
│  ├─ report.tex
│  └─ UCSD_CSE240A_Branch_Project_Report.pdf
├─ src/
│  ├─ main.c            # Main driver for reading branch traces and running predictions
│  ├─ predictor.h       # Header file with predictor definitions and APIs
│  ├─ predictor.c       # Implementation of the 3 predictors (G-Share, Tournament, TAGE)
│  ├─ results.txt       # Output of runall.sh (example final results)
│  ├─ runall.sh         # Script to run static, gshare, tournament, custom for 6 traces
│  └─ run_extended_experiments.sh  # Extended experiments script (provided in this README)
├─ traces/
│  ├─ int_1.bz2         # Trace file (integer workload)
│  ├─ int_2.bz2         # ...
│  ├─ fp_1.bz2          # Trace file (floating-point workload)
│  ├─ fp_2.bz2          # ...
│  ├─ mm_1.bz2          # Trace file (memory-intensive workload)
│  └─ mm_2.bz2          # ...
├─ utility/
│  └─ visualize_results.py       # Python code snippet for plotting/visualizing experimental results
├─ visualizations/
│  ├─ gshare_sweep.png
│  └─ tournament_heatmap.png
└─ README.md            # This readme
```

---

## Requirements

- **C Compiler** (e.g., `gcc`)
- **Make** 4.0 or higher
- **bunzip2** (to decompress `.bz2` trace files)
- **Python** 3.7+ (only needed for extended visualization—if you plan to use `visualize_results.py`)

---

## Installation and Setup

1. **Clone the repository**:

   ```bash
   git clone https://github.com/ChestnutKurisu/CSE240A_Branch_Predictor_SP25.git
   cd CSE240A_Branch_Predictor_SP25
   ```
2. **Build the predictor**:

   ```bash
   cd src
   make clean
   make
   ```

   This produces an executable named `predictor`.

3. **(Optional) Install Python Dependencies** for extended result visualization:

   ```bash
   pip install matplotlib numpy
   ```
   or manually install them with your favorite Python environment manager (conda, venv, etc.).

---

## Docker Setup and Usage

If you prefer to work within a **Docker environment** (which closely matches the autograder’s Ubuntu + gcc toolchain), follow these steps:

1. **Install Docker Desktop** on Windows 11:
   - Download [Docker Desktop for Windows](https://www.docker.com/products/docker-desktop).
   - Follow the on-screen instructions to enable WSL2 integration (default on Windows 11).

2. **Pull the course Docker image**:
   ```bash
   docker pull prodromou87/ucsd_cse240a
   ```

3. **Run a Docker container** and mount your local project directory. For example, if your project is in  
   `C:\Users\param\OneDrive\Documents\PyCharmProjects\UCSD-CSE\CSE 240A\project`,  
   you can run:
   ```bash
   docker run --rm -it ^
     -v "//c/Users/param/OneDrive/Documents/PyCharmProjects/UCSD-CSE/CSE 240A/project:/CSE240A" ^
     prodromou87/ucsd_cse240a
   ```
   - **Note** the use of `//c/Users/...` instead of `C:\Users\...`.
   - **Note** the quotes around the `-v` argument if there is a space in the path.

4. **Build your predictor** inside Docker:
   ```bash
   cd /CSE240A/src
   make
   ```
   This creates `predictor` inside the container.

5. **Run experiments**:
   - **`runall.sh`**:
     ```bash
     chmod +x runall.sh
     ./runall.sh
     ```
     This will generate or update `results.txt` in `/CSE240A/src`.
   - **`run_extended_experiments.sh`**:
     ```bash
     chmod +x run_extended_experiments.sh
     ./run_extended_experiments.sh
     ```
     This typically saves an `extended_results.txt` (or similar) in `/CSE240A/src`.

6. **Retrieve results**: 
   - Because you used `-v` (a volume mount), the `.txt` files created inside Docker under `/CSE240A/src` will **automatically appear** in your Windows folder:
     ```
     C:\Users\param\OneDrive\Documents\PyCharmProjects\UCSD-CSE\CSE 240A\project\src
     ```
     You can open them directly in an editor on Windows.

That’s it! This Docker-based flow lets you compile and run your code in a known environment identical (or very close) to the grading environment.

---

## Usage

### Running Built-in Experiments

To run the default set of branch predictions (Static, G-Share, Tournament, Custom) on the six provided traces, run:

```bash
cd src
./runall.sh
```

This script:
- Decompresses each of the six `.bz2` files.
- Invokes `./predictor` with four modes: `--static`, `--gshare:13`, `--tournament:9:10:10`, and `--custom`.
- Appends the results into `results.txt`.

**Sample `results.txt`** output snippet:

```
Branch Predictor Results
========================
int_1:
  [STATIC]
Approx memory usage: 0 bits (0.00 KB)
Branches:           3771697
Incorrect:          1664686
Misprediction Rate:  44.136
  [GSHARE:13]
Approx memory usage: 16384 bits (2.00 KB)
Branches:           3771697
Incorrect:           521958
Misprediction Rate:  13.839
  [TOURNAMENT:9:10:10]
Approx memory usage: 14336 bits (1.75 KB)
Branches:           3771697
Incorrect:           476073
Misprediction Rate:  12.622
  [CUSTOM]
Approx memory usage: 63250 bits (7.72 KB)
Branches:           3771697
Incorrect:           248627
Misprediction Rate:   6.592
======
```

### Extended Experiments

If you want to sweep over different G-Share history sizes, different Tournament parameters, or additional TAGE configurations, you can run the **extended experiment script**:

```bash
./run_extended_experiments.sh
```

*(See the [run_extended_experiments.sh](#extended-experiments-script) section below for details on how to configure and use it.)*

### Visualizations

After extended experiments, you can use the Python script [`visualize_results.py`](#visualize_resultspy) to parse the new extended results file and generate charts (accuracy vs. history size, or any other relevant parameter). See that section for usage instructions.

---

## Implementation Details

### 1. G-Share Predictor

- **Global History Register (GHR)** of length `ghistoryBits`.
- **Index** into a 2-bit saturating counter array by XORing the GHR with bits from the PC.
- 2-bit counters track the states (Strongly Not Taken, Weakly Not Taken, Weakly Taken, Strongly Taken).

**Key operations**:
1. **Prediction**: Use `(PC ^ GHR) & ((1 << ghistoryBits)-1)` to index the counter table.
2. **Update**: Shift GHR left by 1, insert the actual outcome bit, and update the saturating counter.

### 2. Tournament Predictor

- **Global Predictor**: 2-bit counters indexed by global history.
- **Local Predictor**: (a) Per-PC local history table, (b) 2-bit counters indexed by that local history.
- **Choice (meta) Predictor**: 2-bit counters to choose between global or local predictions.

**Steps**:
1. Compute local and global predictions separately.
2. The choice predictor decides which to trust.
3. Update local, global, and choice predictors.

### 3. Custom TAGE Predictor

- **TAGE** uses multiple tagged predictor tables at different history lengths.
- **Bimodal** fallback: if none of the TAGE tables match, use a simple bimodal table.
- **Allocates** new entries on mispredictions if the provider predictor is also incorrect.
- **Saturating counters** track usage (confidence) and usefulness bits.

This predictor aims to outperform simpler schemes by capturing correlations over varying history lengths.

---

## Results

From the example `results.txt` (produced by `runall.sh`), the **final misprediction rates** for the six traces are:

| **Trace** | **Static** | **G-Share:13** | **Tournament:9:10:10** | **Custom (TAGE)** |
|-----------|-----------:|---------------:|-----------------------:|------------------:|
| **int_1** | 44.136%    | 13.839%        | 12.622%                | 6.592%            |
| **int_2** | 5.508%     | 0.420%         | 0.426%                 | 0.254%            |
| **fp_1**  | 12.128%    | 0.825%         | 0.991%                 | 0.767%            |
| **fp_2**  | 42.350%    | 1.678%         | 3.246%                 | 0.236%            |
| **mm_1**  | 50.353%    | 6.696%         | 2.581%                 | 0.343%            |
| **mm_2**  | 37.045%    | 10.138%        | 8.483%                 | 5.292%            |

- **Static** is generally much worse except in certain highly biased traces.
- **G-Share** improves misprediction significantly in most traces.
- **Tournament** is often better than G-Share (but not always).
- **Custom TAGE** obtains the best accuracy for all traces.

---

## Memory Usage

A summary of approximate memory usage (in bits) for each predictor:

- **Static**: ~0 bits.
- **G-Share**: `(1 << ghistoryBits) * 2` bits. For `ghistoryBits = 13`, usage = `2^13 * 2 = 16384 bits`.
- **Tournament**: 
  - Global BHT: `(1 << ghistoryBits) * 2` bits  
  - Choice predictor: `(1 << ghistoryBits) * 2` bits  
  - Local BHT: `(1 << lhistoryBits) * 2` bits  
  - Local PHT: `(1 << pcIndexBits) * lhistoryBits` bits  
- **Custom TAGE** (TAGE + Bimodal + overhead): Typically in the tens of thousands of bits. In the provided code, it’s around ~63K bits for the base config.

The course limit is **64K + 256 bits**; the custom TAGE predictor stays within this limit.

---

## Contributing

This is a course assignment repository. Contributions are usually not expected. If you wish to suggest improvements or discuss design, feel free to open an issue or contact the repository owner.
