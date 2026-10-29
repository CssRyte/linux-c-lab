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

        if (accessType == READ_WRITE &&
            page.protection == READ_ONLY)

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
        vector<string> translationTimeline;

        // Tracks the segment and page occupying each frame.
        vector<pair<int, int>> frame_owners;

        int page_size;
        int page_faults = 0;

    public:


    void logError(const string& message) {

        ofstream log("errors.log", ios::app);

        if (!log.is_open()) {
            cerr << "Error: Unable to open errors.log\n";
            return;
        }

        log << message << "\n";
        log.close();
    }

void recordTranslation(int segNum, int pageNum,
                       int offset, int physicalAddress) {

    string entry = "Time " +
        to_string(translationTimeline.size() + 1);

    entry += " | Segment " + to_string(segNum);
    entry += " | Page " + to_string(pageNum);
    entry += " | Offset " + to_string(offset);

    if (physicalAddress == -1) {
        entry += " | FAILED";
    } else {
        entry += " | Physical Address " +
                 to_string(physicalAddress);
    }

    translationTimeline.push_back(entry);
}

void printTranslationTimeline() const {

    cout << "\n===== ADDRESS TRANSLATION TIMELINE =====\n";

    if (translationTimeline.empty()) {
        cout << "No translations recorded.\n";
        return;
    }

    for (const string& entry : translationTimeline) {
        cout << entry << "\n";
    }

    cout << "========================================\n";
}

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

    if (limit <= 0 || page_size <= 0) {
        cout << "Invalid segment configuration.\n";
        return;
    }

    // Assign each segment a non-overlapping logical base.
    int base = 0;

    if (!segments.empty()) {
        const Segment& previous = segments.back();
        base = previous.base_address + previous.limit;
    }

    int numPages = 1 + (limit - 1) / page_size;

    segments.push_back({base, limit, protection});
    page_tables.emplace_back(numPages, page_size);

    cout << "Created segment "
         << segments.size() - 1
         << " | Base: " << base
         << " | Size: " << limit
         << " bytes | Pages: " << numPages
         << "\n";
}


       
int translateAddress(int segNum, int pageNum,
                     int offset, Protection accessType,
                     int& latency) {

    // Helper: record and log failed translations.
    auto fail = [&](const string& reason) -> int {

        cout << reason << "\n";

        logError(
            reason +
            " | Segment=" + to_string(segNum) +
            " | Page=" + to_string(pageNum) +
            " | Offset=" + to_string(offset) +
            " | Access=" +
            string(accessType == READ_ONLY ? "READ" : "WRITE")
        );

        recordTranslation(segNum, pageNum, offset, -1);

        return -1;
    };

    // =========================================
    // STEP 1: VALIDATE SEGMENT
    // =========================================

    if (segNum < 0 ||
        segNum >= static_cast<int>(segments.size())) {

        return fail("Segmentation Fault: Invalid segment");
    }

    Segment& segment = segments[segNum];

    // =========================================
    // STEP 2: VALIDATE PAGE AND OFFSET
    // =========================================

    if (pageNum < 0 ||
        pageNum >= page_tables[segNum].num_pages ||
        offset < 0 ||
        offset >= page_size ||
        1LL * pageNum * page_size + offset >= segment.limit) {

        return fail("Invalid Address: Page or offset out of bounds");
    }

    // =========================================
    // STEP 3: CHECK SEGMENT PROTECTION
    // =========================================

    if (accessType == READ_WRITE &&
        segment.protection == READ_ONLY) {

        return fail("Protection Violation: Segment is read-only");
    }

    PageTable& table = page_tables[segNum];
    Page& page = table.getPage(pageNum);

    // =========================================
    // STEP 4: CHECK PAGE PROTECTION
    // =========================================

    if (accessType == READ_WRITE &&
        page.protection == READ_ONLY) {

        return fail("Protection Violation: Page is read-only");
    }

    // =========================================
    // STEP 5: HANDLE PAGE FAULT
    // =========================================

    if (!page.present) {

        cout << "Page fault detected.\n";

        page_faults++;

        // Simulated page fault penalty.
        latency += 100;

        int frame = memory.allocateFrame();

        if (frame == -1) {
            return fail(
                "Memory Error: No physical frame available"
            );
        }

        // Determine whether the selected frame
        // previously belonged to another page.
        auto [oldSegment, oldPage] = frame_owners[frame];

        if (oldSegment != -1 && oldPage != -1) {

            Page& evicted =
                page_tables[oldSegment].getPage(oldPage);

            evicted.present = false;
            evicted.frame_number = -1;

            cout << "Evicted Segment "
                 << oldSegment
                 << ", Page " << oldPage << "\n";
        }

        // Load the requested page into the frame.
        // Preserve the page's existing protection.
        table.setFrame(
            pageNum,
            frame,
            page.protection
        );

        frame_owners[frame] = {segNum, pageNum};
    }

    // =========================================
    // STEP 6: ACCESS PHYSICAL FRAME
    // =========================================

    int frame = table.getFrameNumber(
        pageNum, accessType
    );

    if (frame == -1) {
        return fail(
            "Translation Error: Unable to obtain frame"
        );
    }

    // Update last-used timestamp for LRU.
    memory.accessFrame(frame);

    // Normal simulated translation cost.
    latency += 1;

    // =========================================
    // STEP 7: COMPUTE PHYSICAL ADDRESS
    // =========================================

    long long physicalAddress =
        1LL * frame * page_size + offset;

    if (physicalAddress > INT_MAX) {
        return fail(
            "Address Overflow: Physical address too large"
        );
    }

    cout << "Physical address: "
         << physicalAddress << "\n";

    // =========================================
    // STEP 8: RECORD SUCCESSFUL TRANSLATION
    // =========================================

    recordTranslation(
        segNum,
        pageNum,
        offset,
        static_cast<int>(physicalAddress)
    );

    return static_cast<int>(physicalAddress);
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


bool setPageProtection(int segNum, int pageNum,
                       Protection protection) {

    if (segNum < 0 ||
        segNum >= static_cast<int>(segments.size())) {
        cout << "Invalid segment number.\n";
        return false;
    }

    PageTable& table = page_tables[segNum];

    if (pageNum < 0 || pageNum >= table.num_pages) {
        cout << "Invalid page number.\n";
        return false;
    }

    // A page cannot grant write permission when
    // its containing segment is read-only.
    if (segments[segNum].protection == READ_ONLY &&
        protection == READ_WRITE) {
        cout << "Cannot grant write access to a "
             << "page inside a read-only segment.\n";
        return false;
    }

    Page& page = table.getPage(pageNum);
    page.protection = protection;

    cout << "Protection updated for Segment "
         << segNum << ", Page " << pageNum << "\n";

    return true;
}

};



int getValidatedInput(const string& prompt, int minimum, int maximum) {

    int value;

    while (true) {
        cout << prompt;

        if (!(cin >> value)) {
            cout << "Error: Please enter a valid integer.\n";

            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            continue;
        }

        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        if (value < minimum || value > maximum) {
            cout << "Error: Enter a value between "
                 << minimum << " and " << maximum << ".\n";
            continue;
        }

        return value;
    }
}






int main() {

    cout << "=========================================\n";
    cout << " M5A1 Advanced Segmented Memory Simulator\n";
    cout << "=========================================\n";

    // =========================================
    // STEP 1: MEMORY CONFIGURATION
    // =========================================

    int numFrames = getValidatedInput(
        "Enter number of physical frames: ", 1, 10000
    );

    int pageSize = getValidatedInput(
        "Enter page size (bytes): ", 1, 1000000
    );

    // Ensure physical addresses fit in int.
    while (1LL * numFrames * pageSize > INT_MAX) {
        cout << "Error: Physical memory size exceeds "
             << "the supported address range.\n";

        numFrames = getValidatedInput(
            "Enter number of physical frames: ",
            1, 10000
        );

        pageSize = getValidatedInput(
            "Enter page size (bytes): ",
            1, 1000000
        );
    }

    int numSegments = getValidatedInput(
        "Enter number of segments: ", 1, 100
    );

    // =========================================
    // STEP 2: REPLACEMENT ALGORITHM
    // =========================================

    cout << "\nChoose replacement algorithm:\n";
    cout << "1. FIFO\n";
    cout << "2. LRU\n";

    int policyChoice = getValidatedInput(
        "Choice: ", 1, 2
    );

    PhysicalMemory::ReplacementPolicy policy =
        (policyChoice == 1)
            ? PhysicalMemory::FIFO
            : PhysicalMemory::LRU;

    PhysicalMemory memory(numFrames, policy);
    SegmentTable segments(memory, pageSize);

    // =========================================
    // STEP 3: SEGMENT CONFIGURATION
    // =========================================

    for (int i = 0; i < numSegments; i++) {

        cout << "\n========== SEGMENT " << i
             << " CONFIGURATION ==========\n";

        int segmentSize = getValidatedInput(
            "Enter segment size (bytes): ",
            1, 10000000
        );

        cout << "\nChoose segment protection:\n";
        cout << "1. Read Only\n";
        cout << "2. Read/Write\n";

        int protectionChoice = getValidatedInput(
            "Protection: ", 1, 2
        );

        Protection protection =
            (protectionChoice == 1)
                ? READ_ONLY
                : READ_WRITE;

        segments.addSegment(segmentSize, protection);
    }

    // =========================================
    // STEP 4: MAIN MENU
    // =========================================

    int latency = 0;
    int choice = 0;

    do {

        cout << "\n========== MAIN MENU ==========\n";
        cout << "1. Translate Address\n";
        cout << "2. Display Page Directories\n";
        cout << "3. Display Physical Memory Map\n";
        cout << "4. Display Statistics\n";
        cout << "5. Run Random Stress Test\n";
        cout << "6. Configure Page Protection\n";
        cout << "7. Display Translation Timeline\n";
        cout << "8. Exit\n";

        choice = getValidatedInput(
            "Choice: ", 1, 8
        );

        switch (choice) {

        // =====================================
        // OPTION 1: ADDRESS TRANSLATION
        // =====================================

        case 1: {

            cout << "\n===== ADDRESS TRANSLATION =====\n";

            int segNum = getValidatedInput(
                "Segment number: ", -1000000, 1000000
            );

            int pageNum = getValidatedInput(
                "Page number: ", -1000000, 1000000
            );

            int offset = getValidatedInput(
                "Offset: ", -1000000, 1000000
            );

            cout << "\nAccess type:\n";
            cout << "1. Read\n";
            cout << "2. Write\n";

            int accessChoice = getValidatedInput(
                "Access: ", 1, 2
            );

            Protection accessType =
                (accessChoice == 1)
                    ? READ_ONLY
                    : READ_WRITE;

            int physicalAddress =
                segments.translateAddress(
                    segNum,
                    pageNum,
                    offset,
                    accessType,
                    latency
                );

            if (physicalAddress == -1) {
                cout << "Translation failed.\n";
            } else {
                cout << "Translation successful: "
                     << physicalAddress << "\n";
            }

            break;
        }

        // =====================================
        // OPTION 2: PAGE DIRECTORIES
        // =====================================

        case 2:
            segments.printPageDirectories();
            break;

        // =====================================
        // OPTION 3: PHYSICAL MEMORY MAP
        // =====================================

        case 3:
            segments.printPhysicalMemoryMap();
            break;

        // =====================================
        // OPTION 4: STATISTICS
        // =====================================

        case 4: {

            cout << "\n========== STATISTICS ==========\n";

            cout << "Page faults: "
                 << segments.getPageFaults() << "\n";

            cout << "Page replacements: "
                 << memory.replacements << "\n";

            cout << "Memory utilization: "
                 << memory.utilization() << "%\n";

            cout << "Total simulated latency: "
                 << latency << "\n";

            cout << "================================\n";

            break;
        }

        // =====================================
        // OPTION 5: RANDOM STRESS TEST
        // =====================================

        case 5: {

            cout << "\n===== RANDOM STRESS TEST =====\n";

            int validPercent = getValidatedInput(
                "Valid address percentage (0-100): ",
                0, 100
            );

            segments.runStressTest(
                100, validPercent, latency
            );

            break;
        }

        // =====================================
        // OPTION 6: PAGE PROTECTION
        // =====================================

        case 6: {

            cout << "\n===== CONFIGURE PAGE PROTECTION =====\n";

            int segNum = getValidatedInput(
                "Segment number: ", 0, numSegments - 1
            );

            int pageNum = getValidatedInput(
                "Page number: ", 0, 1000000
            );

            cout << "\nChoose page protection:\n";
            cout << "1. Read Only\n";
            cout << "2. Read/Write\n";

            int protectionChoice = getValidatedInput(
                "Protection: ", 1, 2
            );

            Protection protection =
                (protectionChoice == 1)
                    ? READ_ONLY
                    : READ_WRITE;

            bool success = segments.setPageProtection(
                segNum,
                pageNum,
                protection
            );

            if (success) {
                cout << "Page protection configured "
                     << "successfully.\n";
            } else {
                cout << "Page protection update failed.\n";
            }

            break;
        }

        // =====================================
        // OPTION 7: TRANSLATION TIMELINE
        // =====================================

        case 7: {

            segments.printTranslationTimeline();

            break;
        }

        // =====================================
        // OPTION 8: EXIT
        // =====================================

        case 8: {

            cout << "\nExiting M5A1 Memory Simulator.\n";

            break;
        }

        }

    } while (choice != 8);

    return 0;
}
