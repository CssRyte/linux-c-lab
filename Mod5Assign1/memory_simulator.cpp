#include <iostream>
#include <vector>
#include <queue>
#include <string>
#include <algorithm>
#include <limits>
#include <utility>
#include <climits>
#include <random>
#include <fstream>
#include <iomanip>
using namespace std;

// Memory protection types
enum Protection {
    READ_ONLY,
    READ_WRITE
};

// Represents an individual page
struct Page {
    int frame_number = -1;
    bool present = false;
    Protection protection = READ_WRITE;
    int last_access = 0;
};

// Represents a memory segment
struct Segment {
    int base_address;
    int limit;
    Protection protection;
};

// Page table for a segment


class PageTable {
public:
    static constexpr int ENTRIES_PER_TABLE = 4;
    // Each entry in this directory is a second-level page table.
    vector<vector<Page>> directory;

    int page_size;
    int num_pages;

    PageTable(int numPages, int size)
        : page_size(size), num_pages(numPages) {

        int tableCount =
            (numPages + ENTRIES_PER_TABLE - 1)
            / ENTRIES_PER_TABLE;

        for (int i = 0; i < tableCount; i++) {
            int remaining = numPages - i * ENTRIES_PER_TABLE;
            int entries = min(ENTRIES_PER_TABLE, remaining);

            directory.emplace_back(entries);
        }
    }

    // Locate a page through the two-level directory.
    Page& getPage(int pageNum) {
        int directoryIndex = pageNum / ENTRIES_PER_TABLE;
        int tableIndex = pageNum % ENTRIES_PER_TABLE;

        return directory.at(directoryIndex).at(tableIndex);
    }

    int getFrameNumber(int pageNum, Protection accessType) {

        if (pageNum < 0 || pageNum >= num_pages) {
            cout << "Invalid page number.\n";
            return -1;
        }

        Page& page = getPage(pageNum);

        if (!page.present) {
            cout << "Page Fault: Page " << pageNum
                 << " is not in memory.\n";
            return -1;
        }

        if (accessType == READ_WRITE &&
            page.protection == READ_ONLY) {
            cout << "Protection Violation.\n";
            return -1;
        }

        page.last_access++;
        return page.frame_number;
    }

    void setFrame(int pageNum, int frame,
                  Protection protection) {

        if (pageNum < 0 || pageNum >= num_pages) {
            cout << "Invalid page number.\n";
            return;
        }

        Page& page = getPage(pageNum);

        page.frame_number = frame;
        page.present = true;
        page.protection = protection;
        page.last_access = 0;
    }

    void printDirectory() {

        cout << "\n===== Two-Level Page Directory =====\n";

        for (int i = 0; i < static_cast<int>(directory.size()); i++) {

            cout << "\nDirectory Entry " << i << ":\n";

            for (int j = 0;
                 j < static_cast<int>(directory[i].size());
                 j++) {

                Page& page = directory[i][j];

                int logicalPage = i * ENTRIES_PER_TABLE + j;

                cout << "  Page " << logicalPage
                     << " -> Frame " << page.frame_number
                     << " | Present: " << page.present
                     << "\n";
            }
        }
    }
};



class PhysicalMemory {
public:
    enum ReplacementPolicy {
        FIFO,
        LRU
    };

    int num_frames;
    vector<bool> free_frames;
    queue<int> fifo_queue;

    vector<long long> last_used;
    long long clock = 0;

    ReplacementPolicy policy;
    int replacements = 0;

    PhysicalMemory(int frames, ReplacementPolicy mode = FIFO)
        : num_frames(frames),
          free_frames(frames, true),
          last_used(frames, 0),
          policy(mode) {}

    // Record an access to a physical frame.
    void accessFrame(int frame) {
        if (frame >= 0 && frame < num_frames &&
            !free_frames[frame]) {
            last_used[frame] = ++clock;
        }
    }

    int allocateFrame() {

        // First, look for an unoccupied frame.
        for (int i = 0; i < num_frames; i++) {
            if (free_frames[i]) {
                free_frames[i] = false;
                fifo_queue.push(i);
                accessFrame(i);

                cout << "Allocated free frame: "
                     << i << "\n";
                return i;
            }
        }

        if (num_frames == 0) {
            cout << "No physical frames available.\n";
            return -1;
        }

        int victim = -1;

        if (policy == FIFO) {
            victim = fifo_queue.front();
            fifo_queue.pop();

            cout << "FIFO Replacement: ";
        } else {
            // Select the least recently accessed frame.
            long long oldest = LLONG_MAX;

            for (int i = 0; i < num_frames; i++) {
                if (last_used[i] < oldest) {
                    oldest = last_used[i];
                    victim = i;
                }
            }

            // Keep queue consistent for possible mode changes.
            queue<int> updated;

            while (!fifo_queue.empty()) {
                int current = fifo_queue.front();
                fifo_queue.pop();

                if (current != victim)
                    updated.push(current);
            }

            fifo_queue = updated;

            cout << "LRU Replacement: ";
        }

        replacements++;
        fifo_queue.push(victim);
        accessFrame(victim);

        cout << "Frame " << victim << " reused.\n";
        return victim;
    }

    void freeFrame(int frame) {
        if (frame < 0 || frame >= num_frames)
            return;

        free_frames[frame] = true;
        last_used[frame] = 0;

        queue<int> updated;

        while (!fifo_queue.empty()) {
            int current = fifo_queue.front();
            fifo_queue.pop();

            if (current != frame)
                updated.push(current);
        }

        fifo_queue = updated;
    }

    double utilization() const {
        if (num_frames == 0)
            return 0.0;

        int used = 0;

        for (bool isFree : free_frames) {
            if (!isFree)
                used++;
        }

        return 100.0 * used / num_frames;
    }
};


class SegmentTable {
    private:
        vector<Segment> segments;
        vector<PageTable> page_tables;
        PhysicalMemory& memory;

        // Tracks the segment and page occupying each frame.
        vector<pair<int, int>> frame_owners;

        int page_size;
        int page_faults = 0;

    public:

void printPhysicalMemoryMap() const {

    cout << "\n========== PHYSICAL MEMORY MAP ==========\n";

    for (int frame = 0; frame < memory.num_frames; frame++) {

        cout << "Frame " << frame << ": ";

        if (memory.free_frames[frame]) {
            cout << "[FREE]\n";
            continue;
        }

        auto [segNum, pageNum] = frame_owners[frame];

        if (segNum == -1 || pageNum == -1) {
            cout << "[OCCUPIED - OWNER UNKNOWN]\n";
        } else {
            cout << "Segment " << segNum
                 << " | Page " << pageNum << "\n";
        }
    }

    cout << "-----------------------------------------\n";
    cout << "Memory utilization: "
         << memory.utilization() << "%\n";

    cout << "Page faults: " << page_faults << "\n";
    cout << "Page replacements: "
         << memory.replacements << "\n";

    cout << "=========================================\n";
}

        void printPageDirectories() {

    for (int i = 0;
         i < static_cast<int>(page_tables.size());
         i++) {

        cout << "\nSegment " << i << ":\n";
        page_tables[i].printDirectory();
    }
}
        SegmentTable(PhysicalMemory& mem, int size)
            : memory(mem),
            frame_owners(mem.num_frames, {-1, -1}),
            page_size(size) {}

        void addSegment(int limit, Protection protection) {
            int numPages = (limit + page_size - 1) / page_size;

            segments.push_back({0, limit, protection});
            page_tables.emplace_back(numPages, page_size);

            cout << "Created segment "
                << segments.size() - 1
                << " with " << numPages << " pages.\n";
        }

        int translateAddress(int segNum, int pageNum,
                            int offset, Protection accessType,
                            int& latency) {

            // Validate segment number.
            if (segNum < 0 ||
                segNum >= static_cast<int>(segments.size())) {
                cout << "Invalid segment number.\n";
                return -1;
            }

            Segment& segment = segments[segNum];

            // Validate page and offset.
            if (pageNum < 0 || offset < 0 ||
                offset >= page_size ||
                1LL * pageNum * page_size + offset >= segment.limit) {
                cout << "Invalid logical address.\n";
                return -1;
            }

            // Check segment-level access permissions.
            if (accessType == READ_WRITE &&
                segment.protection == READ_ONLY) {
                cout << "Segment protection violation.\n";
                return -1;
            }

            PageTable& table = page_tables[segNum];
            Page& page = table.getPage(pageNum);

            // Check page-level access permissions.
            if (page.present &&
                accessType == READ_WRITE &&
                page.protection == READ_ONLY) {
                cout << "Page protection violation.\n";
                return -1;
            }

            if (!page.present) {
                cout << "Page fault detected.\n";
                page_faults++;
                latency += 100;

                int frame = memory.allocateFrame();

                if (frame == -1) {
                    cout << "Address translation failed: no frames.\n";
                    return -1;
                }
                // Invalidate any previous owner of this frame.
                auto [oldSegment, oldPage] = frame_owners[frame];

                if (oldSegment != -1) {
                    Page& evicted = page_tables[oldSegment].getPage(oldPage);

                    evicted.present = false;
                    evicted.frame_number = -1;
                }

                table.setFrame(pageNum, frame, segment.protection);
                frame_owners[frame] = {segNum, pageNum};
            }

            latency += 1;

        int frame = table.getFrameNumber(pageNum, accessType);

            if (frame == -1) {
                return -1;
            }

            // Update usage timestamp for LRU
            memory.accessFrame(frame);

            int physicalAddress = frame * page_size + offset;

            cout << "Physical address: "
                << physicalAddress << "\n";

            return physicalAddress;
        }

        int getPageFaults() const {
            return page_faults;
        }


void runStressTest(int testCount, int validPercent,
                   int& latency) {

    if (testCount <= 0 || validPercent < 0 ||
        validPercent > 100 || segments.empty()) {
        cout << "Invalid stress test configuration.\n";
        return;
    }

    ofstream results("results.txt");
    ofstream errors("errors.log", ios::app);

    if (!results || !errors) {
        cerr << "Unable to open test output files.\n";
        return;
    }

    mt19937 rng(42);

    auto randomInt = [&](int low, int high) {
        uniform_int_distribution<int> dist(low, high);
        return dist(rng);
    };

    int successes = 0;
    int failures = 0;
    int validRequests = 0;
    int invalidRequests = 0;

    int initialFaults = page_faults;
    int initialReplacements = memory.replacements;
    int initialLatency = latency;

    results << "M5A1 - Random Address Stress Test\n";
    results << "================================\n";
    results << "Total requests: " << testCount << "\n";
    results << "Target valid-address ratio: "
            << validPercent << "%\n\n";

    for (int i = 1; i <= testCount; i++) {

        // Decide whether this request should be
        // within the configured logical address bounds.
        bool validRequest = randomInt(1, 100) <= validPercent;

        int segNum;
        int pageNum;
        int offset;

        if (validRequest) {
            validRequests++;

            segNum = randomInt(
                0, static_cast<int>(segments.size()) - 1
            );

            int limit = segments[segNum].limit;

            // Choose a byte within the segment.
            int logicalByte = randomInt(0, limit - 1);

            pageNum = logicalByte / page_size;
            offset = logicalByte % page_size;

        } else {
            invalidRequests++;

            // An out-of-range segment guarantees
            // an invalid logical address.
            segNum = static_cast<int>(segments.size())
                     + randomInt(0, 3);

            pageNum = randomInt(0, 3);
            offset = randomInt(0, page_size - 1);
        }

        Protection accessType =
            randomInt(0, 1) == 0
                ? READ_ONLY
                : READ_WRITE;

        results << "Test " << i
                << ": Segment=" << segNum
                << ", Page=" << pageNum
                << ", Offset=" << offset
                << ", Access="
                << (accessType == READ_ONLY
                    ? "READ" : "WRITE")
                << "\n";

        int address = translateAddress(
            segNum, pageNum, offset, accessType, latency
        );

        if (address != -1) {
            successes++;

            results << "Result: SUCCESS"
                    << " | Physical Address: "
                    << address << "\n\n";

        } else {
            failures++;

            results << "Result: FAILED\n\n";

            errors << "Stress Test " << i
                   << " | Segment=" << segNum
                   << " | Page=" << pageNum
                   << " | Offset=" << offset
                   << " | Access="
                   << (accessType == READ_ONLY
                       ? "READ" : "WRITE")
                   << " | Translation failed\n";
        }
    }

    int newFaults = page_faults - initialFaults;
    int newReplacements =
        memory.replacements - initialReplacements;
    int latencyUsed = latency - initialLatency;

    double faultRate =
        100.0 * newFaults / testCount;

    double successRate =
        100.0 * successes / testCount;

    double averageLatency =
        static_cast<double>(latencyUsed) / testCount;

    results << fixed << setprecision(2);

    results << "\n========== SUMMARY ==========\n";
    results << "Total requests: " << testCount << "\n";
    results << "Valid logical addresses generated: "
            << validRequests << "\n";
    results << "Invalid logical addresses generated: "
            << invalidRequests << "\n";
    results << "Successful translations: "
            << successes << "\n";
    results << "Failed translations: "
            << failures << "\n";
    results << "Success rate: " << successRate << "%\n";
    results << "Page faults: " << newFaults << "\n";
    results << "Page fault rate: " << faultRate << "%\n";
    results << "Page replacements: "
            << newReplacements << "\n";
    results << "Memory utilization: "
            << memory.utilization() << "%\n";
    results << "Average simulated latency: "
            << averageLatency << "\n";

    cout << fixed << setprecision(2);
    cout << "\n========== STRESS TEST SUMMARY ==========\n";
    cout << "Total requests: " << testCount << "\n";
    cout << "Successful translations: " << successes << "\n";
    cout << "Failed translations: " << failures << "\n";
    cout << "Success rate: " << successRate << "%\n";
    cout << "Page faults: " << newFaults << "\n";
    cout << "Page fault rate: " << faultRate << "%\n";
    cout << "Page replacements: "
         << newReplacements << "\n";
    cout << "Memory utilization: "
         << memory.utilization() << "%\n";
    cout << "Average simulated latency: "
         << averageLatency << "\n";

    cout << "\nResults saved to results.txt\n";
    cout << "Failures logged to errors.log\n";
}

 };






int main() {

    cout << "=====================================\n";
    cout << " M5A1 Advanced Memory Simulator\n";
    cout << "=====================================\n";

    int numFrames, pageSize, numSegments;
    int policyChoice;

    cout << "Enter number of physical frames: ";
    cin >> numFrames;

    cout << "Enter page size (bytes): ";
    cin >> pageSize;

    cout << "Enter number of segments: ";
    cin >> numSegments;

    if (!cin || numFrames <= 0 || pageSize <= 0 ||
        numSegments <= 0) {
        cerr << "Invalid memory configuration.\n";
        return 1;
    }

    cout << "\nChoose replacement algorithm:\n";
    cout << "1. FIFO\n";
    cout << "2. LRU\n";
    cout << "Choice: ";
    cin >> policyChoice;

    if (!cin || (policyChoice != 1 && policyChoice != 2)) {
        cerr << "Invalid replacement policy.\n";
        return 1;
    }

    PhysicalMemory::ReplacementPolicy policy =
        (policyChoice == 1)
        ? PhysicalMemory::FIFO
        : PhysicalMemory::LRU;

    PhysicalMemory memory(numFrames, policy);
    SegmentTable segments(memory, pageSize);

    // Configure each segment.
    for (int i = 0; i < numSegments; i++) {

        int segmentSize, protectionChoice;

        cout << "\nSegment " << i << " configuration\n";

        cout << "Enter segment size (bytes): ";
        cin >> segmentSize;

        cout << "Protection (1=Read Only, 2=Read/Write): ";
        cin >> protectionChoice;

        if (!cin || segmentSize <= 0 ||
            (protectionChoice != 1 && protectionChoice != 2)) {
            cerr << "Invalid segment configuration.\n";
            return 1;
        }

        Protection prot =
            (protectionChoice == 1)
            ? READ_ONLY
            : READ_WRITE;

        segments.addSegment(segmentSize, prot);
    }

    int latency = 0;
    int choice = 0;

    do {
        cout << "1. Translate Address\n";
        cout << "2. Display Page Directories\n";
        cout << "3. Display Physical Memory Map\n";
        cout << "4. Display Statistics\n";
        cout << "5. Run Random Stress Test\n";
        cout << "6. Exit\n";

        if (!(cin >> choice)) {
            cerr << "Invalid menu input.\n";
            return 1;
        }

        switch (choice) {

        case 1: {
            int segNum, pageNum, offset, accessChoice;

            cout << "Segment number: ";
            cin >> segNum;

            cout << "Page number: ";
            cin >> pageNum;

            cout << "Offset: ";
            cin >> offset;

            cout << "Access (1=Read, 2=Write): ";
            cin >> accessChoice;

            if (!cin ||
                (accessChoice != 1 && accessChoice != 2)) {
                cout << "Invalid access type.\n";
                if (!cin) return 1;
                break;
            }

            Protection accessType =
                (accessChoice == 1)
                ? READ_ONLY
                : READ_WRITE;

            int address = segments.translateAddress(
                segNum, pageNum, offset, accessType, latency
            );

            if (address == -1) {
                cout << "Translation failed.\n";
            } else {
                cout << "Translation successful: "
                     << address << "\n";
            }

            break;
        }

        case 2:
            segments.printPageDirectories();
            break;

        case 3:
            segments.printPhysicalMemoryMap();
            break;

        case 4:
            cout << "\n===== STATISTICS =====\n";
            cout << "Page faults: "
                 << segments.getPageFaults() << "\n";

            cout << "Replacements: "
                 << memory.replacements << "\n";

            cout << "Memory utilization: "
                 << memory.utilization() << "%\n";

            cout << "Simulated latency: "
                 << latency << "\n";
            break;

        case 5: {
    int validPercent;

    cout << "Enter valid address percentage (0-100): ";
    cin >> validPercent;

    if (!cin || validPercent < 0 || validPercent > 100) {
        cout << "Invalid percentage.\n";
        if (!cin) return 1;
        break;
    }

    segments.runStressTest(100, validPercent, latency);
    break;
}

case 6:
    cout << "Exiting simulator.\n";
    break;
        default:
            cout << "Invalid menu option.\n";
        }

    } while (choice != 6);

    return 0;
}
