# Microarchitectural Espionage

FPGA-based RISC-V BOOM testbed for microarchitectural side-channel attacks.

This repository provides a complete experimental platform for implementing and evaluating branch-predictor-based side-channel attacks on a RISC-V out-of-order core (BOOM) deployed on FPGA.


## Overview

This project implements two state-of-the-art microarchitectural side-channel attacks:

- **CBPA** (Conditional Branch Predictor Attack)  
- **IBPA** (Indirect Branch Predictor Attack)

The platform is built using **Chipyard** and targets the **Nexys Video (Artix-7)** FPGA board. Two prebuilt BOOM configurations are provided:

- `BoomNLP.bit` – small BOOM core with a simple 1-level predictor (NLP + BIM + BTB)  
- `BoomTAGE.bit` – small BOOM core with TAGE predictor enabled (countermeasure evaluation)

Both attacks are implemented as bare-metal programs and loaded onto the FPGA using **UART-TSI**.


## Repository Structure
```bash
PRIM/
├── Makefile
├── fpga-procs/
│   ├── BoomNLP.bit        
│   └── BoomTAGE.bit       
└── demo-attacks/
    ├── CBPA.c             
    ├── IBPA.c            
    ├── smart-lock.c
    └── rlibsc.h
```

## Requirements

### Hardware
- Nexys Video FPGA board

### Software
- Linux host
- Vivado **2021.x**
- Chipyard (installed next to this repository)
- Java + SBT (for Chipyard)
- RISC-V GCC toolchain


## Setup

### 1. Install Chipyard

Clone Chipyard next to this repository:
workspace/
├── chipyard/
└── Microarchitectural-Espionage/

Set environment variable:

```bash
export CHIPYARD=/path/to/chipyard
```

### 2. Build UART-TSI in Chipyard (once)
UART-TSI is required to load bare-metal binaries.
```bash
cd $CHIPYARD
source env.sh
cd generators/testchipip/uart_tsi
make
```
This produces the uart_tsi binary used by the Makefile.


## Programming the FPGA

#### 1.	Open Vivado
#### 2.	Connect Nexys Video board
#### 3.	Program one of the provided bitstreams:

Vulnerable configuration:
```bash
fpga-procs/BoomNLP.bit
```

Countermesure configuration:
```bash
fpga-procs/BoomTAGE.bit
```


## Running the Attacks

### 1. Compile the attack
From repository root:
```bash
make build-CBPA   
    or
make build-IBPA
```

### 2. Load binary file
```bash
make run-CBPA
    or
make run-IBPA
```
The binary is transmitted over UART and executed automatically.

### 3. View results

Attack output appears directly in the terminal (via serial).


## Attacks Implemented

### CBPA
- Exploits aliasing in branch direction predictor tables
- Recovers secret bits using timing differences between taken / not-taken predictions

### IBPA
- Exploits aliasing in the Branch Target Buffer (BTB)
- Uses indirect branches to leak secret-dependent control flow
