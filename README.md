# Tomasulo Simulation

## Introduction
This project is an educational simulation of the [Tomasulo algorithm](https://en.wikipedia.org/wiki/Tomasulo_algorithm), a hardware algorithm for out-of-order scheduling and execution of computer instructions, written in C++.

## Implementation

Below are some of the key concepts of the simulation's algorithm:

* **Reorder Buffer:** simulates branching resolution using a reorder buffer
* **Register Renaming:** a technique to minimize data hazards
* **Result Buses:** unlike the original Tomasulo algorithm, which contains a single *Common Data Bus (CDB)*, there can be multiple result buses to update the state of the pipeline.
* **Reservation Station Scheduling:** units of scheduling managing an instruction fired to a *Function Unit*
* **Specialized Function Units:** specialized hardware units specific to different types of instructions
* **Gselect Branch Prediction:** a global branch prediction scheme that keeps a pattern history table and concatenates the global branch history