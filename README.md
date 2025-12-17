# 🔴 Gravity 4: High-Performance Bitboard Engine

![Language](https://img.shields.io/badge/language-C11-00599C?style=for-the-badge\&logo=c)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey?style=for-the-badge)
![License](https://img.shields.io/badge/license-MIT-green?style=for-the-badge)
![Optimization](https://img.shields.io/badge/optimization-O3%20%7C%20AVX-red?style=for-the-badge)


Welcome to **Gravity 4**, a pure C implementation of the classic *Connect 4*. This project avoids slow arrays, instead leveraging **128-bit Bitboards**. This architectural choice makes Gravity 4 one of the few open-source engines capable of playing at a **superhuman level on large boards (8x8, 10x10)**, where standard 64-bit bots simply fail.
It also uses state-of-the-art adversarial search algorithms and a smooth, animated terminal interface.


Oh, and the board **SPINS**. 🌀

---

## ✨ Key Features

### 🧠 "Superhuman" Artificial Intelligence

* **Bitboard Engine:** Bitwise operations for extreme performance.
* **Move Ordering:** Ordered according to created threats.
* **Negamax + Alpha-Beta Pruning (with PVS):** Deep search with aggressive branch pruning.
* **Transposition Tables (TT):** Massive cache (1GB+) with **Zobrist Hashing** to instantly remember past positions.
* **Iterative Deepening:** The engine constantly improves its response until the allocated time runs out.

### 🏆 Why 128-bit Matters? (The 64-bit Barrier)

Most Connect 4 engines are limited to `uint64_t` bitboards, capping their physical board size at roughly 7x6 cells.
*   **The Competitors:** On larger boards (like 8x8 or 10x10), standard bots forcedly switch to slow array-based logic, dropping their performance to near zero.
*   **Gravity 4:** Thanks to the **128-bit architecture**, this engine maintains its O(1) win detection speed even on massive grids (up to 10x10), keeping the AI "invincible".

### 🎮 Gameplay Experience

* **"Spin" Mode (Chaos):** A unique mechanic where the board can rotate 90º or -90º randomly, reapplying gravity to all pieces.
* **Terminal Interface:**
  * Flicker-free piece drop animations.
  * High-quality Unicode symbols (✖ / ●).
  * Full ANSI color support.
* **Full Customization:** Play on boards from 4x4 up to 10x10, change the win condition (K-in-a-row), and adjust AI thinking time.

---

## ⚡ Engine Capability and Benchmarks

This engine has been obsessively designed for speed. By eliminating arrays from the core logic and leveraging CPU registers, we achieve **competitive-level performance figures**.

### 📊 Real-Time Statistics

The engine displays detailed metrics while it "thinks". Here's what they mean:

1. **NPS (Nodes per Second):**

   * The engine processes approximately **5,500,000 nodes per second** on standard modern hardware.
   * Evaluates 6 million possible future positions every second.
   * **Smart Node Evaluation Mode:** The engine can **anticipate losing positions** and skip their full evaluation, drastically reducing unnecessary depth while maintaining optimal decision quality.

2. **EBF (Effective Branching Factor):**

   * In Connect 4, there are up to 7 possible moves. A raw brute-force search would have an EBF of 7.0.
   * Thanks to our **Alpha-Beta pruning** and move ordering, our EBF is **below 1.6**.
   * **What does this mean?** The engine is so efficient at discarding bad moves that, mathematically, it’s as if the game only had 1 or 2 real options per turn instead of 7.

3. **Game Solving:**

   * The engine is capable of **mathematically solving** the standard 6x7 board (proving forced victory from the start) in roughly **420 seconds (7 minutes)**.
   * You can choose the engine's thinking time to adjust the challenge. With 5 seconds per move, the engine typically reaches a search depth of 28 in a 6x7 board, practically guaranteeing a win in just a few moves.

---

## 🧠 AI Engineering

* **128-bit Bitboards:** Supports boards up to 10x10 cells using `unsigned __int128` types.
* **Negamax + Alpha-Beta:** The core search algorithm.
* **Transposition Tables (TT):**

  * 64-bit Zobrist hashing.
  * Collision management via "Buckets" (stores the deepest and most recent positions).
  * **Fail-Soft** logic to maximize data reuse between iterations.
* **Iterative Deepening:** The engine always has a "best move" ready, refining it cycle by cycle until the allocated time runs out.

## 🛠️ Engineering Decisions & Trade-offs

During development, several standard chess engine heuristics were implemented, profiled, and ultimately discarded based on empirical data:

*   **Killer Moves & History Heuristic:** Implemented but removed.
    *   *Reason:* Connect 4 has a very low branching factor (max 7 moves) compared to Chess. The overhead of maintaining history tables slowed down the search (lower NPS) without significantly improving the Alpha-Beta cut-off rate.
        - While I know that implementations like **Fhourstones** benefit from these techniques, I prefer this approach: ordering moves according to **threats created** and selective evaluation.
    *   *Solution:* A strict **"Threat-First" move ordering** (winning moves > blocking/threating moves > center columns) proved to be the most efficient strategy for this specific game complexity.
*   **Transposition Table Sizing:**
    *   Used a Two-Tier bucket system (Deepest + Newest) to handle collisions, preventing valuable deep-search data from being overwritten by shallow searches in the same hash index.

---

## 🚀 Installation and Usage

This project includes a smart build system that detects your OS automatically.

### Compilation

```bash
make
```

It will automatically detect if you are on Linux, macOS, or Windows and apply optimization flags -O3 -march=native -flto.

### Running the Game

```bash
# Linux / macOS
./connect4

# Windows
connect4.exe
```

### Configuration Menu

At startup, you can configure every aspect of the engine:

```
=== CONFIGURATION ===
Board: 6x7 (Connect 4)
You: O (AI: X)
Start: X
Spin: 0%
AI Time: 5.0s
-------------------------
1. Play vs Bitboard AI (Maximum depth, no heuristic)
2. Play vs Bitboard AI (with heuristic)
3. Play with someone else
4. Change board settings (Rows, Cols, K-in-a-row)
...
```

---

## 🔮 Future Improvements

* **Parallelization:** Exploring multi-threaded search to leverage modern CPU cores and further reduce evaluation time.
* **GUI Integration:** Could extend the terminal interface to a graphical frontend for a more interactive experience.

---

## ✍️ Notes from the Developer

*"This code represents MANY hours and MUCH learning, nights without sleep and days without going out. If anyone finds a bug or malfunction, I appreciate your feedback."*
