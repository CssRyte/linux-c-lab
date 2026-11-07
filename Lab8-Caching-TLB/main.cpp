
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

constexpr uint32_t PAGE_SIZE = 4096;
constexpr uint32_t DIRECTORY_ENTRIES = 1024;
constexpr uint32_t TABLE_ENTRIES = 1024;
constexpr uint32_t PHYSICAL_FRAMES = 256;

struct PageTableEntry {
    bool valid = false;
    uint32_t frame = 0;
};

struct PageTable {
    array<PageTableEntry, TABLE_ENTRIES> entries{};
};

struct PageDirectoryEntry {
    unique_ptr<PageTable> table;
};

class TwoLevelPageTable {
    array<PageDirectoryEntry, DIRECTORY_ENTRIES> directory{};

public:
    static uint32_t directoryIndex(uint32_t va) {
        return (va >> 22) & 0x3FFu;
    }

    static uint32_t tableIndex(uint32_t va) {
        return (va >> 12) & 0x3FFu;
    }

    static uint32_t offset(uint32_t va) {
        return va & 0xFFFu;
    }

    PageTableEntry& entry(uint32_t vpn) {
        uint32_t pd = (vpn >> 10) & 0x3FFu;
        uint32_t pt = vpn & 0x3FFu;

        if (!directory[pd].table) {
            directory[pd].table = make_unique<PageTable>();
        }

        return directory[pd].table->entries[pt];
    }
};

struct TLBEntry {
    bool valid = false;
    uint32_t vpn = 0;
    uint32_t frame = 0;
    uint64_t lastUsed = 0;
};

class TLB {
    vector<TLBEntry> entries;
    uint64_t clock = 0;
    uint64_t hits = 0;
    uint64_t misses = 0;

public:
    explicit TLB(size_t capacity) : entries(capacity) {}

    bool lookup(uint32_t vpn, uint32_t& frame) {
        ++clock;

        for (auto& item : entries) {
            if (item.valid && item.vpn == vpn) {
                item.lastUsed = clock;
                frame = item.frame;
                ++hits;
                return true;
            }
        }

        ++misses;
        return false;
    }

    void insert(uint32_t vpn, uint32_t frame) {
        ++clock;

        // Update an existing entry.
        for (auto& item : entries) {
            if (item.valid && item.vpn == vpn) {
                item.frame = frame;
                item.lastUsed = clock;
                return;
            }
        }

        // Use an empty entry first.
        for (auto& item : entries) {
            if (!item.valid) {
                item = {true, vpn, frame, clock};
                return;
            }
        }

        // Replace the least recently used entry.
        auto victim = min_element(
            entries.begin(),
            entries.end(),
            [](const TLBEntry& a, const TLBEntry& b) {
                return a.lastUsed < b.lastUsed;
            }
        );

        *victim = {true, vpn, frame, clock};
    }

    void invalidate(uint32_t vpn) {
        for (auto& item : entries) {
            if (item.valid && item.vpn == vpn) {
                item.valid = false;
            }
        }
    }

    uint64_t hitCount() const {
        return hits;
    }

    uint64_t missCount() const {
        return misses;
    }
};

struct Frame {
    bool occupied = false;
    uint32_t vpn = 0;
};

class MemorySimulator {
    TwoLevelPageTable pageTable;
    TLB tlb;

    array<Frame, PHYSICAL_FRAMES> frames{};

    uint32_t nextFree = 0;
    mt19937 rng{42};

    uint64_t faults = 0;
    uint64_t replacements = 0;

public:
    explicit MemorySimulator(size_t tlbSize)
        : tlb(tlbSize) {}

    struct Result {
        uint64_t physicalAddress;
        bool tlbHit;
        bool pageFault;
        bool replaced;
        uint32_t frame;
    };

    Result translate(uint32_t va) {
        const uint32_t vpn = va >> 12;
        const uint32_t offset =
            TwoLevelPageTable::offset(va);

        uint32_t frame = 0;

        const bool hit = tlb.lookup(vpn, frame);

        bool fault = false;
        bool replaced = false;

        if (!hit) {
            PageTableEntry& entry = pageTable.entry(vpn);

            if (!entry.valid) {
                fault = true;
                ++faults;

                if (nextFree < PHYSICAL_FRAMES) {
                    frame = nextFree++;
                } else {
                    uniform_int_distribution<uint32_t> choose(
                        0, PHYSICAL_FRAMES - 1
                    );

                    frame = choose(rng);

                    uint32_t oldVpn = frames[frame].vpn;

                    // Invalidate the old page mapping.
                    pageTable.entry(oldVpn).valid = false;

                    // Remove the old TLB translation.
                    tlb.invalidate(oldVpn);

                    replaced = true;
                    ++replacements;
                }

                frames[frame] = {true, vpn};

                entry.frame = frame;
                entry.valid = true;
            } else {
                frame = entry.frame;
            }

            tlb.insert(vpn, frame);
        }

        return {
            (static_cast<uint64_t>(frame) << 12) | offset,
            hit,
            fault,
            replaced,
            frame
        };
    }

    uint64_t faultCount() const {
        return faults;
    }

    uint64_t replacementCount() const {
        return replacements;
    }

    uint64_t hitCount() const {
        return tlb.hitCount();
    }

    uint64_t missCount() const {
        return tlb.missCount();
    }
};

bool parseAddress(const string& line, uint32_t& result) {
    string text = line.substr(0, line.find('#'));

    auto first = text.find_first_not_of(" \t\r\n");

    if (first == string::npos) {
        return false;
    }

    auto last = text.find_last_not_of(" \t\r\n");

    text = text.substr(first, last - first + 1);

    // Addresses are hexadecimal, with or without 0x.
    size_t parsed = 0;

    unsigned long long value =
        stoull(text, &parsed, 16);

    if (parsed != text.size() ||
        value > numeric_limits<uint32_t>::max()) {

        throw invalid_argument(
            "Invalid 32-bit hexadecimal address: " + text
        );
    }

    result = static_cast<uint32_t>(value);

    return true;
}

int main(int argc, char* argv[]) {
    size_t tlbSize = 16;
    string filename = "addresses.txt";
    bool verbose = true;

    try {
        for (int i = 1; i < argc; ++i) {
            string option = argv[i];

            if (option == "--tlb-size" && i + 1 < argc) {
                string sizeText = argv[++i];
                size_t parsed = 0;

                unsigned long value =
                    stoul(sizeText, &parsed);

                if (parsed != sizeText.size() ||
                    value == 0 || value > 1024) {

                    throw invalid_argument(
                        "TLB size must be 1 through 1024"
                    );
                }

                tlbSize = static_cast<size_t>(value);

            } else if (option == "--input" && i + 1 < argc) {
                filename = argv[++i];

            } else if (option == "--quiet") {
                verbose = false;

            } else {
                cerr << "Usage: " << argv[0]
                     << " [--input addresses.txt]"
                     << " [--tlb-size N] [--quiet]\n";

                return 1;
            }
        }

        ifstream input(filename);

        if (!input) {
            cerr << "Cannot open " << filename << '\n';
            return 1;
        }

        MemorySimulator simulator(tlbSize);

        uint64_t accesses = 0;
        double translationNs = 0;

        string line;

        while (getline(input, line)) {
            uint32_t va;

            if (!parseAddress(line, va)) {
                continue;
            }

            // Time translation only, excluding
            // printing and file reads.
            auto begin = chrono::steady_clock::now();

            auto result = simulator.translate(va);

            auto finish = chrono::steady_clock::now();

            translationNs +=
                chrono::duration<double, nano>(
                    finish - begin
                ).count();

            ++accesses;

            if (verbose) {
                if (result.pageFault) {
                    cout << "PAGE FAULT: VPN "
                         << dec << (va >> 12)
                         << " -> frame " << result.frame;

                    if (result.replaced) {
                        cout << " (frame replacement)";
                    }

                    cout << '\n';
                }

                cout << "VA: 0x"
                     << hex << setw(8) << setfill('0')
                     << va
                     << " -> PA: 0x"
                     << result.physicalAddress
                     << dec
                     << " | TLB "
                     << (result.tlbHit ? "HIT" : "MISS")
                     << '\n';
            }
        }

        cout << "\n--- Summary ---\n";

        cout << "TLB size: " << tlbSize << '\n';
        cout << "Total accesses: " << accesses << '\n';
        cout << "TLB hits: " << simulator.hitCount() << '\n';
        cout << "TLB misses: " << simulator.missCount() << '\n';
        cout << "Page faults: " << simulator.faultCount() << '\n';
        cout << "Frame replacements: "
             << simulator.replacementCount() << '\n';

        cout << fixed << setprecision(2);

        cout << "TLB hit rate: "
             << (accesses
                 ? 100.0 * simulator.hitCount() / accesses
                 : 0.0)
             << "%\n";

        cout << "Average translation time: "
             << (accesses
                 ? translationNs / accesses
                 : 0.0)
             << " ns\n";

        cout << "Timing measures simulator translation code,"
             << " not hardware RAM latency.\n";

    } catch (const exception& error) {
        cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
