#include <iostream>
#include <vector>
#include <queue>
#include <string>
#include <algorithm>
#include <limits>
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

            if (frame == -1) {
                cout << "Address translation failed: no frames.\n";
                return -1;
            }
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
};




int main() {
    cout << "M5A1 - FIFO vs LRU Test\n";

    vector<int> requests = {0, 1, 2, 0, 3};

    for (int mode = 0; mode < 2; mode++) {

        PhysicalMemory::ReplacementPolicy policy =
            mode == 0 ? PhysicalMemory::FIFO
                      : PhysicalMemory::LRU;

        cout << "\n========================\n";
        cout << (mode == 0 ? "Testing FIFO" : "Testing LRU");
        cout << "\n========================\n";

        PhysicalMemory memory(3, policy);
        SegmentTable segments(memory, 1000);

        segments.addSegment(4000, READ_WRITE);

        int latency = 0;

        for (int page : requests) {
            cout << "\nRequesting page " << page << "\n";

            segments.translateAddress(
                0, page, 0, READ_ONLY, latency
            );
        }

        cout << "\nPage faults: "
             << segments.getPageFaults() << "\n";

        cout << "Replacements: "
             << memory.replacements << "\n";

        cout << "Utilization: "
             << memory.utilization() << "%\n";

        cout << "Simulated latency: "
             << latency << "\n";
    }

    return 0;
}
