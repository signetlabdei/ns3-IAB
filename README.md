# inmarsat-iab-releases

An open-source, full-stack ns-3 simulation framework for **5G NR Integrated Access and Backhaul (IAB) networks**, aligned with 3GPP Release 16 and extended for **remote maritime connectivity** scenarios.

Built on top of the [ns3-mmwave](https://github.com/nyuwireless-unipd/ns3-mmwave) framework.

> **Associated paper:**
> A. Traspadini, M. Pagin, R. Ihamouine, R. Lucas, A. Noren, M. Zorzi, M. Giordani, *"End-to-End Simulation of 5G NR Integrated Access and Backhaul Networks for Remote Maritime Connectivity"*, submitted to IEEE Transactions on Communications.
---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Prerequisites](#prerequisites)
- [Installation](#installation)
- [Topology Configuration](#topology-configuration)
- [Citation](#citation)

---

## Overview

`inmarsat-iab-releases` implements the complete 5G NR IAB protocol stack as defined in 3GPP Release 16, from the physical layer up to the Backhaul Adaptation Protocol (BAP). It supports flexible configuration of network topology, slot structure, multiplexing schemes, and channel conditions.

The framework models two types of network nodes:

- **IAB-donors** are full-blown base stations with a wired fiber backhaul to the 5G core. They anchor the topology and serve as the gateway for all traffic entering or leaving the IAB network.
- **IAB-nodes** are fiber-free wireless relays, each split into a **Distributed Unit (DU)** — which handles access-link connectivity toward downstream UEs and child IAB-nodes — and a **Mobile Terminal (MT)** — which maintains the wireless backhaul link toward an upstream IAB-node or IAB-donor. This dual-interface design allows each IAB-node to simultaneously provide access and relay backhaul traffic across multi-hop chains.

Spanning tree topologies are supported, with no upper limit on the number of hops.

---

## Features

### Protocol Stack
- **BAP layer** with header encoding, routing table management, and per-hop forwarding logic for multi-hop packet delivery.
- **PDU session traffic** (UE-anchored), using inner PDCP/GTP-U tunneling over the IAB backhaul.
- **Non-PDU LAN traffic** (IAB-node-anchored), routed across the backhaul via BAP-only forwarding, bypassing the SDAP and PDCP layers. A reserved bit in the BAP header distinguishes this traffic from standard PDU flows.

### Radio Resource Management
- Configurable **TDD slot formats** with downlink (DL), uplink (UL), and switching (SW) slot types, and adjustable control symbol overhead.
- **Time Division Multiplexing (TDM)** between MT and DU interfaces, with per-layer OFDM symbol allocation to prevent bottleneck effects in multi-hop chains.
- **Frequency Division Multiplexing (FDM)** via carrier aggregation, enabling simultaneous MT and DU operation over orthogonal frequency bands.

### Channel Modeling
- **Modified 2-ray propagation model** for mmWave maritime and coastal environments, capturing sea-surface reflections and frequency-dependent path loss.
- **Rain attenuation model** following ITU-R P.838-3, with a configurable rain rate to evaluate performance across varying weather conditions.

---

## Prerequisites

Install the baseline ns-3 dependencies (Ubuntu):

```bash
sudo apt install g++ python3 python3-dev pkg-config sqlite3 cmake
```

For batch simulation campaigns, also install the [SEM execution manager](https://github.com/signetlabdei/sem) along with `NumPy` and `Matplotlib`:

```bash
python3 -m pip install sem numpy matplotlib
```

---

## Installation

This module is self-contained. Simply clone the repository and build:

```bash
git clone https://github.com/signetlabdei/inmarsat-iab-releases.git
cd inmarsat-iab-releases
./ns3 configure --build-profile=optimized --enable-tests --enable-examples --enable-modules=mmwave,internet-apps
./ns3
```

---

## Topology Configuration

IAB topologies are defined using the `MmWaveHelper` class, which exposes three methods to attach IAB nodes and establish parent-child backhaul relationships.

### `AttachIabTotDonorWithIndex`

```cpp
mmwaveHelper->AttachIabTotDonorWithIndex(iabNode, donorDevContainer, donorIndex);
```

Attaches an IAB-node directly to an IAB-donor (depth 1). `donorIndex` selects the target donor from `donorDevContainer` when multiple donors are present.

### `AttachIabTotIabWithIndex`

```cpp
mmwaveHelper->AttachIabTotIabWithIndex(iabDevContainer, childIndex, parentIndex, donorDevContainer, donorIndex);
```

Attaches an IAB-node to another IAB-node, creating a multi-hop backhaul link. `childIndex` and `parentIndex` identify the two nodes within `iabDevContainer`; `donorIndex` identifies the IAB-donor that roots the subtree.

### `AttachToClosestIab`

```cpp
mmwaveHelper->AttachToClosestIab(ueDevice, iabDevices);
```

Attaches a UE to the closest IAB-node in `iabDevices`, based on node positions at the time of the call.

---

### Examples

**Single-hop** (UE — IAB-Node — Donor):

```cpp
mmwaveHelper->AttachIabTotDonorWithIndex(iabMmWaveDev.Get(0), enbMmWaveDev, 0);
mmwaveHelper->AttachToClosestIab(ueDev, iabMmWaveDev);
```

`UE` → `IAB-Node 0` → `Donor` → `Core Network`

**Multi-hop** (UE — IAB-Node — IAB-Node — Donor):

```cpp
mmwaveHelper->AttachIabTotDonorWithIndex(iabMmWaveDev.Get(0), enbMmWaveDev, 0); // depth 1
mmwaveHelper->AttachIabTotIabWithIndex(iabMmWaveDev, 1, 0, enbMmWaveDev, 0);    // depth 2
mmwaveHelper->AttachToClosestIab(ueDev, iabMmWaveDev);
```

`UE` → `IAB-Node 1` → `IAB-Node 0` → `Donor` → `Core Network`

---

## Citation

If you use this simulator in your research, please cite:

```bibtex
@article{traspadini2025iab,
  title   = {End-to-End Simulation of {5G} {NR} Integrated Access and Backhaul Networks for Remote Maritime Connectivity},
  author  = {Traspadini, Alessandro and Pagin, Matteo and Ihamouine, Rapha{\"e}l and Lucas, Rupert and Noren, Andrew and Zorzi, Michele and Giordani, Marco},
  journal = {IEEE Transactions on Communications},
  year    = {2025},
  note    = {Submitted}
}
```
