#include <iostream>
#include <vector>
#include <queue>
#include <string>
#include <algorithm>
#include <utility>
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
    vector<Page> pages;
    int page_size;

    PageTable(int numPages, int size)
        : pages(numPages), page_size(size) {}

    // Find a page's frame and check access permissions.
    // Returns -1 if the page is missing or access is denied.
    int getFrameNumber(int pageNum, Protection accessType) {

        if (pageNum < 0 || pageNum >= static_cast<int>(pages.size())) {
            cout << "Invalid page number.\n";
            return -1;
        }

        Page& page = pages[pageNum];

        if (!page.present) {
            cout << "Page Fault: Page " << pageNum
                 << " is not in memory.\n";
            return -1;
        }

        if (accessType == READ_WRITE &&
            page.protection == READ_ONLY) {
            cout << "Protection Violation: Cannot write to read-only page.\n";
            return -1;
        }

        page.last_access++;

        return page.frame_number;
    }

    // Map a page to a physical frame.
    void setFrame(int pageNum, int frame, Protection protection) {

        if (pageNum < 0 || pageNum >= static_cast<int>(pages.size())) {
            cout << "Invalid page number.\n";
            return;
        }

        pages[pageNum].frame_number = frame;
        pages[pageNum].present = true;
        pages[pageNum].protection = protection;
        pages[pageNum].last_access = 0;
    }
};


class PhysicalMemory {
public:
    int num_frames;

    // Whether each physical frame is occupied
    vector<bool> free_frames;

    // Order in which frames were allocated
    queue<int> fifo_queue;

    // Total number of replacements
    int replacements = 0;

    PhysicalMemory(int frames)
        : num_frames(frames),
          free_frames(frames, true) {}

    // Allocate a physical frame using FIFO
    int allocateFrame() {

        // Step 1: Search for an empty frame
        for (int i = 0; i < num_frames; i++) {

            if (free_frames[i]) {

                free_frames[i] = false;

                fifo_queue.push(i);

                cout << "Allocated free frame: "
                     << i << "\n";

                return i;
            }
        }

        // Step 2: Memory is full; replace oldest frame
        int frame = fifo_queue.front();
        fifo_queue.pop();

        // Step 3: Put reused frame at the back
        fifo_queue.push(frame);

        replacements++;

        cout << "FIFO Replacement: Frame "
             << frame << " reused.\n";

        return frame;
    }

    // Free a physical frame
    void freeFrame(int frame) {

        if (frame < 0 || frame >= num_frames) {
            cout << "Invalid frame.\n";
            return;
        }

        free_frames[frame] = true;

        // Remove freed frame from FIFO queue
        queue<int> updated;

        while (!fifo_queue.empty()) {

            int current = fifo_queue.front();
            fifo_queue.pop();

            if (current != frame) {
                updated.push(current);
            }
        }

        fifo_queue = updated;
    }

    // Percentage of occupied frames
    double utilization() const {

        int used = 0;

        for (bool isFree : free_frames) {
            if (!isFree) {
                used++;
            }
        }

        if (num_frames == 0) {
            return 0.0;
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
        Page& page = table.pages[pageNum];

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

            // Invalidate any previous owner of this frame.
            auto [oldSegment, oldPage] = frame_owners[frame];

            if (oldSegment != -1) {
                Page& evicted =
                    page_tables[oldSegment].pages[oldPage];

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

        int physicalAddress = frame * page_size + offset;

        cout << "Physical address: "
             << physicalAddress << "\n";

        return physicalAddress;
    }

    int getPageFaults() const {
        return page_faults;
    }
};



int main() {

    cout << "M5A1 Address Translation Test\n\n";

    PhysicalMemory memory(3);
    SegmentTable segments(memory, 1000);

    // Segment 0: 4000 bytes, read/write
    segments.addSegment(4000, READ_WRITE);

    // Segment 1: 2000 bytes, read-only
    segments.addSegment(2000, READ_ONLY);

    int latency = 0;

    cout << "\nTest 1: Valid translation\n";
    segments.translateAddress(0, 2, 50, READ_ONLY, latency);

    cout << "\nTest 2: Same page (page hit)\n";
    segments.translateAddress(0, 2, 100, READ_ONLY, latency);

    cout << "\nTest 3: Invalid segment\n";
    segments.translateAddress(5, 0, 20, READ_ONLY, latency);

    cout << "\nTest 4: Invalid offset\n";
    segments.translateAddress(0, 1, 1500, READ_ONLY, latency);

    cout << "\nTest 5: Protection violation\n";
    segments.translateAddress(1, 0, 50, READ_WRITE, latency);

    cout << "\nTest 6: Fill remaining frames\n";
    segments.translateAddress(0, 0, 10, READ_ONLY, latency);
    segments.translateAddress(0, 1, 10, READ_ONLY, latency);

    cout << "\nTest 7: Trigger FIFO replacement\n";
    segments.translateAddress(0, 3, 20, READ_ONLY, latency);

    cout << "\nPage faults: "
         << segments.getPageFaults() << "\n";

    cout << "Replacements: "
         << memory.replacements << "\n";

    cout << "Memory utilization: "
         << memory.utilization() << "%\n";

    cout << "Simulated latency: "
         << latency << "\n";

    return 0;
}
