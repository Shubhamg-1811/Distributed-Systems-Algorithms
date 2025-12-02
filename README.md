
# 🚀 Distributed Systems Algorithms in OpenMPI

This repository contains C++ implementations of several fundamental algorithms in distributed systems, built using the **OpenMPI** library. These examples are designed to demonstrate the core logic of each concept in a practical, hands-on way—perfect for students and developers learning about distributed computing.

---

# 🚀 Algorithms Included

Below is a list of all algorithms included, with file names and descriptions.

---

## 🕑 Logical Clocks (Lamport)

**Description:**  
Implements Lamport’s logical clock algorithm for establishing a partial ordering of events in a distributed system.  
Each event gets a simple integer timestamp, enabling reasoning about the *happened-before* relationship.

**File:** `logical_clock.cpp`

---

## 📈 Vector Clocks

**Description:**  
An improvement over Lamport clocks that captures the full causal history.  
Each process maintains a vector of timestamps—one entry per process—to determine whether events are causally related or concurrent.

**File:** `vector_clock.cpp`

---

## 🧮 Matrix Clocks

**Description:**  
A global state tracking technique where each process maintains a **matrix** representing the vector clocks of all processes.  
Useful for detecting stable properties and performing distributed garbage collection.

**File:** `matrix_clock.cpp`

---

## 👑 Leader Election (Chang & Roberts Algorithm)

**Description:**  
Implements the Chang & Roberts leader election algorithm for ring topologies.  
Each process sends its ID clockwise; higher IDs get forwarded, lower IDs get discarded, and when a process receives its own ID back, it becomes the leader.

**File:** `ring.cpp`

---

## 🌳 Simple Rooted Spanning Tree

**Description:**  
A wave-based, asynchronous algorithm for building a spanning tree from an arbitrary connected graph.  
Nodes accept the first proposal they receive and adopt that sender as the parent.

**File:** `rst.cpp`

---

## 🌲 Asynchronous BFS Spanning Tree

**Description:**  
A more structured version of spanning tree creation using **SYNC** (`MS_SYNC_TAG`) and **COMPLETE** (`MC_COMPLETE_TAG`) messages.  
Parents control when children can start processing the next BFS level.  
This prevents race conditions and ensures proper BFS layering.

**File:** `bfs_async.cpp`

---

## 🔒 Maekawa’s Distributed Mutual Exclusion Algorithm (DME)

**Description:**  
Implements Maekawa’s quorum-based DME algorithm.  
Instead of asking all processes for permission, each node requests access only from a small, overlapping **voting set**.  
The design ensures only one process can obtain all permissions required to enter the critical section.

Includes full 5-message protocol:
- `REQUEST`
- `REPLY`
- `FAILED`
- `INQUIRE`
- `RELINQUISH`

**File:** `maekawa.cpp`

---

## 🏛️ Paxos (Conceptual Implementation)

**Description:**  
A simplified implementation of the Paxos consensus algorithm with simultaneous initiations of consensus by multiple nodes.  
Demonstrates the roles of **proposers**, **acceptors**, and **learners**, and how consensus is safely reached even under failures.

**File:** `paxos.cpp`

---

## 🛠️ How to Compile and Run

All examples in this repository follow the same general compilation and execution pattern.

---

## 📌 Prerequisites

- A C++ compiler (like **g++** or **clang++**)  
- An OpenMPI installation (e.g., **libopenmpi-dev**)

---

## 🔧 1. Compilation

To compile any of the `.cpp` files, use the `mpic++` wrapper, which links the necessary MPI libraries.

```bash
# Compiles your C++ file and links the MPI library
mpic++ <your_file_name>.cpp -o <your_executable_name>
```

### Example
```bash
mpic++ maekawa_mpi.cpp -o maekawa
```

---

## ▶️ 2. Execution

To run the compiled program, use `mpirun`.  
The `-np` flag specifies the number of processes to launch.

```bash
# Runs your executable with <number> of processes
mpirun -np <number> ./<your_executable_name>
```

### Example
```bash
mpirun -np 6 ./maekawa
```

---

## ⚠️ Important Note on Process Count

These algorithms are hard-coded with a specific number of processes (e.g.,:

```
NUM_PROCESSES = 6
NUM_PROCESSES = 4
```

👉 **You must check the `.cpp` file first!**  
Then, match the `-np` value in your `mpirun` command with the required number of processes — otherwise the program may **fail**, **hang**, or **deadlock**.

---