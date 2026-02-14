
# 📦 SwiftEx Logistics Engine

A high-performance, console-based logistics simulation engine built entirely in **C++** without Standard Template Library (STL) containers.

## 🚀 Key Features
*   **Custom Data Structures:** Manually implemented Min-Heaps, Graphs (Adjacency Lists), Hash Tables (O(1) Tracking), and Linked Lists.
*   **Intelligent Routing:** Uses **Dijkstra’s Algorithm** for optimal pathfinding across 8 cities with support for dynamic road blockages.
*   **Real-Time Simulation:** Multi-threaded architecture separating the Simulation Engine from the UI.
*   **Smart Dispatch:** Implements a "Space Filling" algorithm to optimize vehicle loads (Buses vs Trucks) based on parcel priority.
*   **Live Dashboard:** separate Admin Panel with colored UI to monitor traffic, lost parcels, and system logs in real-time.

## 🛠️ Tech Stack
*   **Language:** C++17
*   **Concepts:** Multi-threading, IPC (File-based), Graph Theory, Hashing.
*   **Zero STL:** `std::vector`, `std::map`, etc., were replaced with custom templates.

Instructions:
First run the source.cpp and then run the admin.cpp
