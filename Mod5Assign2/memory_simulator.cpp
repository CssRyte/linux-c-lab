#include <iostream>
#include <vector>
#include <unordered_map>
#include <list>
#include <stdexcept>
using namespace std;

// Represents a segment in virtual memory
struct Segment {
    int base;
    int limit;
    bool valid;
    bool readable = true;
    bool writable = false;
    bool executable = false;
};

// Represents a page in virtual memory
struct Page {
    int frameNumber;
    bool present;
    bool dirty;
    bool referenced;
};

// Represents a page table
struct PageTable {
    vector<Page> pages;
};

// Represents the first level of a two-level page table
struct DirectoryTable {
    vector<PageTable> tables;
};

// Translation Lookaside Buffer (TLB)

struct TLB {
    unordered_map<int, int> cache;
    list<int> lruOrder;
    size_t maxSize;

    int hits = 0;
    int misses = 0;

    explicit TLB(size_t size) : maxSize(size) {}


    int lookup(int pageNumber) {
    auto it = cache.find(pageNumber);

    // Case 1: TLB miss
    if (it == cache.end()) {
        misses++;

        cout << "TLB MISS: Page "
             << pageNumber << '\n';

        return -1;
    }

    // Case 2: TLB hit
    hits++;

    // Update LRU order
    lruOrder.remove(pageNumber);
    lruOrder.push_front(pageNumber);

    cout << "TLB HIT: Page "
         << pageNumber
         << " -> Frame "
         << it->second << '\n';

    return it->second;
}

    void insert(int pageNumber, int frameNumber) {
        if (maxSize == 0) return;

        if (cache.count(pageNumber)) {
            cache[pageNumber] = frameNumber;
            lruOrder.remove(pageNumber);
            lruOrder.push_front(pageNumber);
            return;
        }

        if (cache.size() >= maxSize) {
            int victim = lruOrder.back();
            lruOrder.pop_back();
            cache.erase(victim);

            cout << "TLB EVICTION: Page " << victim << '\n';
        }

        cache[pageNumber] = frameNumber;
        lruOrder.push_front(pageNumber);
    }

        // Remove a stale virtual-page translation from the TLB
void invalidate(int pageNumber) {
    auto it = cache.find(pageNumber);

    if (it == cache.end()) {
        return;
    }

    cache.erase(it);
    lruOrder.remove(pageNumber);

    cout << "TLB INVALIDATION: Page "
         << pageNumber << '\n';
}
    void display() const {
        cout << "TLB contents (MRU -> LRU):\n";

        for (int page : lruOrder) {
            cout << "Page " << page
                 << " -> Frame " << cache.at(page) << '\n';
        }
    }

        void displayMetrics() const {
            int total = hits + misses;

            double hitRate = total > 0
                ? 100.0 * hits / total
                : 0.0;

            cout << "\n===== TLB PERFORMANCE METRICS =====\n";
            cout << "TLB Hits: " << hits << '\n';
            cout << "TLB Misses: " << misses << '\n';
            cout << "Total Lookups: " << total << '\n';
            cout << "TLB Hit Rate: " << hitRate << "%\n";
        }
};



// Segment table and associated operations
struct SegmentTable {
    vector<Segment> segments;
    vector<DirectoryTable> directoryTables;
    TLB tlb;

    explicit SegmentTable(size_t tlbSize) : tlb(tlbSize) {}

    void addSegment(const Segment& segment,
                    const DirectoryTable& directory) {
        segments.push_back(segment);
        directoryTables.push_back(directory);
    }

    void displayStats() const {
        cout << "Segments: " << segments.size() << '\n';
        cout << "Directory tables: " << directoryTables.size() << '\n';
        cout << "TLB capacity: " << tlb.maxSize << '\n';
    }
};

// Physical memory manager
struct PhysicalMemory {
    vector<bool> freeFrames;
    list<int> lruList;

    explicit PhysicalMemory(int totalFrames)
        : freeFrames(totalFrames, true) {
        if (totalFrames <= 0) {
            throw invalid_argument("Frame count must be positive");
        }
    }

    int allocateFrame() {
        for (size_t i = 0; i < freeFrames.size(); i++) {
            if (freeFrames[i]) {
                freeFrames[i] = false;
                lruList.push_back(static_cast<int>(i));
                return static_cast<int>(i);
            }
        }
        return -1; // No free frame
    }

    void freeFrame(int frame) {
        if (frame < 0 ||
            static_cast<size_t>(frame) >= freeFrames.size()) {
            throw out_of_range("Invalid frame number");
        }

        freeFrames[frame] = true;
        lruList.remove(frame);
    }

    // Record a physical frame access and update its LRU position
void accessFrame(int frame) {
    if (frame < 0 ||
        static_cast<size_t>(frame) >= freeFrames.size() ||
        freeFrames[frame]) {
        throw out_of_range("Cannot access an unallocated frame");
    }

    lruList.remove(frame);
    lruList.push_back(frame);
}

// Inspect the LRU victim without changing its position
int peekLRU() const {
    if (lruList.empty()) {
        return -1;
    }

    return lruList.front();
}

// Choose and reuse the least recently used frame
int replaceLRU() {
    if (lruList.empty()) {
        return -1;
    }

    int victim = lruList.front();
    lruList.pop_front();
    lruList.push_back(victim);

    cout << "PAGE REPLACEMENT: Evicted frame "
         << victim << " using LRU\n";

    return victim;
}

    // Allocate multiple frames
// Allocate multiple physical memory frames
vector<int> allocateFrames(int count) {
    vector<int> allocated;

    if (count <= 0) {
        return allocated;
    }

    for (int i = 0; i < count; i++) {
        int frame = allocateFrame();

        if (frame == -1) {
            // Roll back all frames allocated in this request
            for (int allocatedFrame : allocated) {
                freeFrame(allocatedFrame);
            }

            cout << "ALLOCATION FAILED: Not enough free frames\n";
            return {};
        }

        allocated.push_back(frame);
    }

    return allocated;
}

// Deallocate multiple physical memory frames
void deallocateFrames(const vector<int>& frames) {
    for (int frame : frames) {
        freeFrame(frame);
    }
}

    double utilization() const {
        int used = 0;

        for (bool isFree : freeFrames) {
            if (!isFree) {
                used++;
            }
        }

        return 100.0 * used / freeFrames.size();
    }
};

bool validateSegmentAccess(
    const Segment& segment,
    int offset,
    char operation
) {
    if (!segment.valid) {
        cout << "SEGMENT FAULT: Invalid segment\n";
        return false;
    }

    if (offset < 0 || offset >= segment.limit) {
        cout << "SEGMENT FAULT: Address outside segment bounds\n";
        return false;
    }

    if (operation == 'R' && !segment.readable) {
        cout << "PROTECTION FAULT: Read denied\n";
        return false;
    }

    if (operation == 'W' && !segment.writable) {
        cout << "PROTECTION FAULT: Write denied\n";
        return false;
    }

    if (operation == 'X' && !segment.executable) {
        cout << "PROTECTION FAULT: Execute denied\n";
        return false;
    }

    if (operation != 'R' && operation != 'W' && operation != 'X') {
        cout << "INVALID OPERATION\n";
        return false;
    }

    return true;
}

struct MemoryStats {
    int pageFaults = 0;
    int pageReplacements = 0;

    void display() const {
        cout << "\n===== MEMORY MANAGEMENT METRICS =====\n";
        cout << "Page Faults: " << pageFaults << '\n';
        cout << "Page Replacements: " << pageReplacements << '\n';
    }
};

int translateAddress(
    DirectoryTable& directory,
    TLB& tlb,
    int virtualAddress,
    int pageSize,
    int pagesPerTable,
    MemoryStats* stats = nullptr
) {
    // Validate the virtual address and paging configuration
    if (virtualAddress < 0 || pageSize <= 0 ||
        pagesPerTable <= 0) {
        cout << "Invalid virtual address or configuration\n";
        return -1;
    }

    // Calculate the virtual page number and offset
    int virtualPage = virtualAddress / pageSize;
    int offset = virtualAddress % pageSize;

    // Check the TLB first
    int cachedFrame = tlb.lookup(virtualPage);

    if (cachedFrame != -1) {
        int physicalAddress = cachedFrame * pageSize + offset;

        cout << "Physical Address (TLB): "
             << physicalAddress << '\n';

        return physicalAddress;
    }

    // Calculate two-level paging indexes
    int directoryIndex = virtualPage / pagesPerTable;
    int pageIndex = virtualPage % pagesPerTable;

    // Check whether the directory entry exists
    if (directoryIndex >= static_cast<int>(directory.tables.size())) {
        if (stats != nullptr) {
            stats->pageFaults++;
        }

        cout << "PAGE FAULT: Directory entry missing\n";
        return -1;
    }

    PageTable& table = directory.tables[directoryIndex];

    // Check whether the page table entry exists
    if (pageIndex >= static_cast<int>(table.pages.size())) {
        if (stats != nullptr) {
            stats->pageFaults++;
        }

        cout << "PAGE FAULT: Page table entry missing\n";
        return -1;
    }

    Page& page = table.pages[pageIndex];

    // Check whether the page is present in physical memory
    if (!page.present) {
        if (stats != nullptr) {
            stats->pageFaults++;
        }

        cout << "PAGE FAULT: Page not present\n";
        return -1;
    }

    // Mark the page as referenced
    page.referenced = true;

    // Cache the successful translation in the TLB
    tlb.insert(virtualPage, page.frameNumber);

    // Calculate the physical address
    int physicalAddress = page.frameNumber * pageSize + offset;

    cout << "Virtual Address: " << virtualAddress << '\n';
    cout << "Directory Index: " << directoryIndex << '\n';
    cout << "Page Index: " << pageIndex << '\n';
    cout << "Offset: " << offset << '\n';
    cout << "Physical Address: " << physicalAddress << '\n';

    return physicalAddress;
}

    int translateSegmentedAddress(
    const Segment& segment,
    DirectoryTable& directory,
    TLB& tlb,
    int segmentOffset,
    char operation,
    int pageSize,
    int pagesPerTable
) {
    // Validate segment bounds and permissions
    if (!validateSegmentAccess(segment, segmentOffset, operation)) {
        return -1;
    }

    // Calculate virtual address from segment base and offset
    int virtualAddress = segment.base + segmentOffset;

    cout << "Segment access validated\n";

    // Perform TLB and two-level page table translation
    return translateAddress(
        directory,
        tlb,
        virtualAddress,
        pageSize,
        pagesPerTable
    );
}

int findPageByFrame(
    const DirectoryTable& directory,
    int frameNumber,
    int pagesPerTable
) {
    if (frameNumber < 0 || pagesPerTable <= 0) {
        return -1;
    }

    for (size_t dir = 0; dir < directory.tables.size(); dir++) {
        const PageTable& table = directory.tables[dir];

        for (size_t page = 0; page < table.pages.size(); page++) {
            const Page& entry = table.pages[page];

            if (entry.present && entry.frameNumber == frameNumber) {
                return static_cast<int>(dir) * pagesPerTable
                     + static_cast<int>(page);
            }
        }
    }

    return -1;
}

bool replaceMappedPage(
    DirectoryTable& directory,
    TLB& tlb,
    PhysicalMemory& memory,
    int oldPageNumber,
    int newPageNumber,
    int pagesPerTable,
    MemoryStats* stats = nullptr
) {
    // Validate page numbers and configuration
    if (oldPageNumber < 0 || newPageNumber < 0 ||
        oldPageNumber == newPageNumber || pagesPerTable <= 0) {
        cout << "REPLACEMENT FAILED: Invalid page numbers\n";
        return false;
    }

    // Calculate old page indexes
    int oldDir = oldPageNumber / pagesPerTable;
    int oldIndex = oldPageNumber % pagesPerTable;

    // Calculate new page indexes
    int newDir = newPageNumber / pagesPerTable;
    int newIndex = newPageNumber % pagesPerTable;

    // Validate directory entries
    if (oldDir >= static_cast<int>(directory.tables.size()) ||
        newDir >= static_cast<int>(directory.tables.size())) {
        cout << "REPLACEMENT FAILED: Missing directory entry\n";
        return false;
    }

    PageTable& oldTable = directory.tables[oldDir];
    PageTable& newTable = directory.tables[newDir];

    // Validate page table entries
    if (oldIndex >= static_cast<int>(oldTable.pages.size()) ||
        newIndex >= static_cast<int>(newTable.pages.size())) {
        cout << "REPLACEMENT FAILED: Missing page entry\n";
        return false;
    }

    Page& oldPage = oldTable.pages[oldIndex];
    Page& newPage = newTable.pages[newIndex];

    // Validate current page states
    if (!oldPage.present || newPage.present) {
        cout << "REPLACEMENT FAILED: Invalid page state\n";
        return false;
    }

   // Inspect the LRU victim without changing its order
int victimFrame = memory.peekLRU();

if (victimFrame == -1 ||
    victimFrame != oldPage.frameNumber) {
    cout << "REPLACEMENT FAILED: LRU victim mismatch\n";
    return false;
}

// All validation has passed; commit the replacement
memory.replaceLRU();

    // Invalidate the old TLB mapping
    tlb.invalidate(oldPageNumber);

    // Remove the old page from physical memory
    oldPage.present = false;
    oldPage.frameNumber = -1;
    oldPage.referenced = false;

    // Map the new page to the reused physical frame
    newPage.frameNumber = victimFrame;
    newPage.present = true;
    newPage.dirty = false;
    newPage.referenced = false;

    cout << "PAGE REPLACEMENT COMPLETE: Page "
         << oldPageNumber << " -> Page "
         << newPageNumber << " in frame "
         << victimFrame << '\n';

    // Count successful page replacements
    if (stats != nullptr) {
        stats->pageReplacements++;
    }

    return true;
}

bool replaceMappedPageAutomatic(
    DirectoryTable& directory,
    TLB& tlb,
    PhysicalMemory& memory,
    int newPageNumber,
    int pagesPerTable,
    MemoryStats* stats = nullptr
) {
    if (newPageNumber < 0 || pagesPerTable <= 0) {
        cout << "AUTO REPLACEMENT FAILED: Invalid arguments\n";
        return false;
    }

    // Identify the LRU physical frame
    int victimFrame = memory.peekLRU();

    if (victimFrame == -1) {
        cout << "AUTO REPLACEMENT FAILED: No victim frame\n";
        return false;
    }

    // Find the virtual page mapped to that frame
    int oldPageNumber = findPageByFrame(
        directory, victimFrame, pagesPerTable
    );

    if (oldPageNumber == -1) {
        cout << "AUTO REPLACEMENT FAILED: Frame owner not found\n";
        return false;
    }

    cout << "AUTO REPLACEMENT: Frame " << victimFrame
         << " belongs to page " << oldPageNumber << '\n';

    // Reuse the existing replacement implementation
    return replaceMappedPage(
        directory,
        tlb,
        memory,
        oldPageNumber,
        newPageNumber,
        pagesPerTable,
        stats
    );
}

int main() {
    cout << "Memory Simulator Initialized!\n";

    SegmentTable segmentTable(4);

    Segment codeSegment{0, 10, true};
    DirectoryTable directory;

    segmentTable.addSegment(codeSegment, directory);
    segmentTable.displayStats();

    PhysicalMemory memory(8);

    int frame1 = memory.allocateFrame();
    int frame2 = memory.allocateFrame();

    cout << "Allocated frame: " << frame1 << '\n';
    cout << "Allocated frame: " << frame2 << '\n';
    cout << "Memory utilization: "
         << memory.utilization() << "%\n";

    memory.freeFrame(frame1);

    cout << "Utilization after freeing frame: "
         << memory.utilization() << "%\n";


    cout << "\n===== TLB LRU TEST =====\n";

TLB testTLB(3);

testTLB.insert(1, 10);
testTLB.insert(2, 20);
testTLB.insert(3, 30);

testTLB.display();

testTLB.lookup(1); // Hit; page 1 becomes most recently used

testTLB.insert(4, 40); // Evicts page 2

testTLB.display();

testTLB.lookup(2); // Miss
testTLB.lookup(4); // Hit


cout << "\n===== TWO-LEVEL PAGING TEST =====\n";

DirectoryTable pagingDirectory;

PageTable firstTable;
firstTable.pages.push_back({2, true, false, false});
firstTable.pages.push_back({5, true, false, false});
firstTable.pages.push_back({7, true, false, false});
firstTable.pages.push_back({9, true, false, false});

pagingDirectory.tables.push_back(firstTable);

TLB pagingTLB(4);

translateAddress(pagingDirectory, pagingTLB, 300, 256, 4);

cout << "\n===== REPEATED ADDRESS TEST =====\n";
translateAddress(pagingDirectory, pagingTLB, 300, 256, 4);

cout << "\n===== PAGE FAULT TEST =====\n";
translateAddress(pagingDirectory, pagingTLB, 2000, 256, 4);

pagingTLB.displayMetrics();

cout << "\n===== SEGMENT PROTECTION TEST =====\n";

Segment dataSegment{0, 1024, true, true, false, false};

cout << "Valid read: ";
cout << (validateSegmentAccess(dataSegment, 300, 'R')
             ? "ALLOWED\n" : "DENIED\n");

cout << "Write attempt: ";
cout << (validateSegmentAccess(dataSegment, 300, 'W')
             ? "ALLOWED\n" : "DENIED\n");

cout << "Out-of-bounds read: ";
cout << (validateSegmentAccess(dataSegment, 1500, 'R')
             ? "ALLOWED\n" : "DENIED\n");


    cout << "\n===== SEGMENTED ADDRESS TRANSLATION TEST =====\n";

Segment protectedSegment{0, 1024, true, true, false, false};
TLB segmentedTLB(4);

// Test 1: Valid read
cout << "\nTest 1: Valid read\n";
translateSegmentedAddress(
    protectedSegment, pagingDirectory, segmentedTLB,
    300, 'R', 256, 4
);

// Test 2: Repeated read (TLB hit)
cout << "\nTest 2: Repeated valid read\n";
translateSegmentedAddress(
    protectedSegment, pagingDirectory, segmentedTLB,
    300, 'R', 256, 4
);

// Test 3: Unauthorized write
cout << "\nTest 3: Unauthorized write\n";
translateSegmentedAddress(
    protectedSegment, pagingDirectory, segmentedTLB,
    300, 'W', 256, 4
);

// Test 4: Out-of-bounds read
cout << "\nTest 4: Out-of-bounds read\n";
translateSegmentedAddress(
    protectedSegment, pagingDirectory, segmentedTLB,
    1500, 'R', 256, 4
);

segmentedTLB.displayMetrics();


cout << "\n===== LRU PAGE REPLACEMENT TEST =====\n";

PhysicalMemory replacementMemory(3);

int f0 = replacementMemory.allocateFrame();
int f1 = replacementMemory.allocateFrame();
int f2 = replacementMemory.allocateFrame();

cout << "Allocated frames: "
     << f0 << ", " << f1 << ", " << f2 << '\n';

// Access frame 0, making it recently used
replacementMemory.accessFrame(f0);

// All frames are occupied
int noFreeFrame = replacementMemory.allocateFrame();
cout << "Next free frame: " << noFreeFrame << '\n';

// Replace the least recently used frame
int victimFrame = replacementMemory.replaceLRU();

cout << "Frame selected for replacement: "
     << victimFrame << '\n';

cout << "Memory utilization: "
     << replacementMemory.utilization() << "%\n";

cout << "\n===== DYNAMIC MEMORY ALLOCATION TEST =====\n";

PhysicalMemory dynamicMemory(6);

// Allocate three frames
vector<int> block1 = dynamicMemory.allocateFrames(3);

cout << "Block 1 allocated frames: ";
for (int frame : block1) {
    cout << frame << " ";
}
cout << '\n';

cout << "Memory utilization: "
     << dynamicMemory.utilization() << "%\n";

// Allocate two more frames
vector<int> block2 = dynamicMemory.allocateFrames(2);

cout << "Block 2 allocated frames: ";
for (int frame : block2) {
    cout << frame << " ";
}
cout << '\n';

cout << "Memory utilization: "
     << dynamicMemory.utilization() << "%\n";

// Release the first block
dynamicMemory.deallocateFrames(block1);
cout << "Utilization after freeing Block 1: "
     << dynamicMemory.utilization() << "%\n";

// Attempt to allocate more frames than available
vector<int> block3 = dynamicMemory.allocateFrames(5);

cout << "Block 3 allocation successful: "
     << (!block3.empty() ? "Yes" : "No") << '\n';

cout << "Final memory utilization: "
     << dynamicMemory.utilization() << "%\n";


cout << "\n===== TLB INVALIDATION TEST =====\n";

TLB invalidationTLB(3);

invalidationTLB.insert(1, 10);
invalidationTLB.insert(2, 20);

cout << "Before invalidation:\n";
invalidationTLB.display();

invalidationTLB.invalidate(1);

cout << "After invalidation:\n";
invalidationTLB.display();

invalidationTLB.lookup(1);
invalidationTLB.lookup(2);

invalidationTLB.displayMetrics();

cout << "\n===== PAGE REPLACEMENT INTEGRATION TEST =====\n";

PhysicalMemory integratedMemory(2);
DirectoryTable integratedDirectory;
TLB integratedTLB(2);

PageTable integratedTable;

// Page 0 and page 1 are resident; page 2 is absent.
integratedTable.pages.push_back({0, true, false, false});
integratedTable.pages.push_back({1, true, false, false});
integratedTable.pages.push_back({-1, false, false, false});

integratedDirectory.tables.push_back(integratedTable);

// Allocate the frames used by pages 0 and 1.
integratedMemory.allocateFrame();
integratedMemory.allocateFrame();

// Cache translations for pages 0 and 1.
integratedTLB.insert(0, 0);
integratedTLB.insert(1, 1);

cout << "Before replacement:\n";
integratedTLB.display();

// Frame 0 is the least recently used.
bool replaced = replaceMappedPage(
    integratedDirectory,
    integratedTLB,
    integratedMemory,
    0,
    2,
    4
);

cout << "Replacement successful: "
     << (replaced ? "Yes" : "No") << '\n';

cout << "After replacement:\n";
integratedTLB.display();

cout << "Old page present: "
     << integratedDirectory.tables[0].pages[0].present << '\n';

cout << "New page present: "
     << integratedDirectory.tables[0].pages[2].present << '\n';

cout << "\nAccessing new page:\n";
translateAddress(
    integratedDirectory, integratedTLB, 512, 256, 4
);

cout << "\nAccessing evicted page:\n";
translateAddress(
    integratedDirectory, integratedTLB, 0, 256, 4
);

cout << "\n===== MEMORY METRICS TEST =====\n";

MemoryStats memoryStats;
PhysicalMemory metricsMemory(1);
DirectoryTable metricsDirectory;
TLB metricsTLB(2);

PageTable metricsTable;

// Page 0 is present; page 1 is not present
metricsTable.pages.push_back({0, true, false, false});
metricsTable.pages.push_back({-1, false, false, false});

metricsDirectory.tables.push_back(metricsTable);

// Allocate one physical frame
metricsMemory.allocateFrame();

// Cache page 0
metricsTLB.insert(0, 0);

// Test 1: Page fault on page 1
translateAddress(
    metricsDirectory,
    metricsTLB,
    256,
    256,
    4,
    &memoryStats
);

// Test 2: Replace page 0 with page 1
replaceMappedPage(
    metricsDirectory,
    metricsTLB,
    metricsMemory,
    0,
    1,
    4,
    &memoryStats
);

// Test 3: Page fault on the evicted page 0
translateAddress(
    metricsDirectory,
    metricsTLB,
    0,
    256,
    4,
    &memoryStats
);

// Display the final statistics
memoryStats.display();


cout << "\n===== CROSS-DIRECTORY PAGE REPLACEMENT TEST =====\n";

PhysicalMemory crossMemory(2);
DirectoryTable crossDirectory;
TLB crossTLB(4);
MemoryStats crossStats;

// First second-level page table: virtual pages 0-3
PageTable crossTable0;
crossTable0.pages.push_back({0, true, false, false});
crossTable0.pages.push_back({-1, false, false, false});
crossTable0.pages.push_back({-1, false, false, false});
crossTable0.pages.push_back({-1, false, false, false});

// Second second-level page table: virtual pages 4-7
PageTable crossTable1;
crossTable1.pages.push_back({1, true, false, false});
crossTable1.pages.push_back({-1, false, false, false});
crossTable1.pages.push_back({-1, false, false, false});
crossTable1.pages.push_back({-1, false, false, false});

crossDirectory.tables.push_back(crossTable0);
crossDirectory.tables.push_back(crossTable1);

// Reserve both physical frames
crossMemory.allocateFrame();
crossMemory.allocateFrame();

// Cache page 0 and page 4
crossTLB.insert(0, 0);
crossTLB.insert(4, 1);

cout << "\nBefore replacement:\n";
crossTLB.display();

// Frame 0 is the LRU victim.
// Replace virtual page 0 with virtual page 5.
bool crossReplaced = replaceMappedPage(
    crossDirectory,
    crossTLB,
    crossMemory,
    0,
    5,
    4,
    &crossStats
);

cout << "Replacement successful: "
     << (crossReplaced ? "Yes" : "No") << '\n';

cout << "\nAccessing new page 5:\n";
translateAddress(
    crossDirectory,
    crossTLB,
    5 * 256,
    256,
    4,
    &crossStats
);

cout << "\nAccessing evicted page 0:\n";
translateAddress(
    crossDirectory,
    crossTLB,
    0,
    256,
    4,
    &crossStats
);

// Display final statistics
crossStats.display();

cout << "\n===== AUTOMATIC PAGE OWNERSHIP TEST =====\n";

PhysicalMemory autoMemory(2);
DirectoryTable autoDirectory;
TLB autoTLB(2);
MemoryStats autoStats;

PageTable autoTable;

// Pages 0 and 1 are resident; page 2 is absent
autoTable.pages.push_back({0, true, false, false});
autoTable.pages.push_back({1, true, false, false});
autoTable.pages.push_back({-1, false, false, false});

autoDirectory.tables.push_back(autoTable);

autoMemory.allocateFrame();
autoMemory.allocateFrame();

// Make frame 0 recently used, so frame 1 becomes LRU
autoMemory.accessFrame(0);

autoTLB.insert(0, 0);
autoTLB.insert(1, 1);

cout << "LRU victim frame: "
     << autoMemory.peekLRU() << '\n';

bool autoReplaced = replaceMappedPageAutomatic(
    autoDirectory,
    autoTLB,
    autoMemory,
    2,
    4,
    &autoStats
);

cout << "Automatic replacement successful: "
     << (autoReplaced ? "Yes" : "No") << '\n';

cout << "\nAccessing new page 2:\n";
translateAddress(
    autoDirectory, autoTLB, 512, 256, 4, &autoStats
);

cout << "\nAccessing evicted page 1:\n";
translateAddress(
    autoDirectory, autoTLB, 256, 256, 4, &autoStats
);

autoStats.display();

cout << "\n===== FAILED REPLACEMENT SAFETY TEST =====\n";

PhysicalMemory safetyMemory(2);
DirectoryTable safetyDirectory;
TLB safetyTLB(2);

PageTable safetyTable;
safetyTable.pages.push_back({0, true, false, false});
safetyTable.pages.push_back({1, true, false, false});
safetyTable.pages.push_back({-1, false, false, false});

safetyDirectory.tables.push_back(safetyTable);

safetyMemory.allocateFrame();
safetyMemory.allocateFrame();

safetyTLB.insert(0, 0);
safetyTLB.insert(1, 1);

// Frame 0 is LRU, but we intentionally request page 1
cout << "LRU before failed replacement: "
     << safetyMemory.peekLRU() << '\n';

bool safetyReplaced = replaceMappedPage(
    safetyDirectory,
    safetyTLB,
    safetyMemory,
    1,
    2,
    4
);

cout << "Replacement successful: "
     << (safetyReplaced ? "Yes" : "No") << '\n';

cout << "LRU after failed replacement: "
     << safetyMemory.peekLRU() << '\n';

cout << "Page 1 still present: "
     << safetyDirectory.tables[0].pages[1].present << '\n';

cout << "Page 2 still absent: "
     << !safetyDirectory.tables[0].pages[2].present << '\n';

 return 0;
}
