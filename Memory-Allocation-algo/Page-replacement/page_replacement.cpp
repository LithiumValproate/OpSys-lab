#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

enum class ReplaceAlgo {
    Fifo_algo,
    Opt_algo,
    Lru_algo,
};

struct Frame {
    int page   = 0;
    bool valid = false;
};

struct AccessRes {
    bool hit;
    std::size_t victim;

    AccessRes(const bool h, const std::size_t v) : hit(h), victim(v) {}
    AccessRes(const bool h, const int intV) : hit(h), victim(static_cast<std::size_t>(intV)) {}
};

class AlgoState {
public:
    virtual ~AlgoState() = default;
    virtual AccessRes access(int step, int page, vector<Frame>& frames, const vector<int>& ref) = 0;
};

class FifoState final : public AlgoState {
    int nextIndex_;

public:
    explicit FifoState(int /*frameCount*/) : nextIndex_(0) {}

    AccessRes access(int /*step*/, int page, vector<Frame>& frames, const vector<int>& /*ref*/) override {
        for (const auto& [p, valid] : frames) {
            if (valid && p == page) {
                return {true, -1};
            }
        }

        for (std::size_t i = 0; i < frames.size(); ++i) {
            if (!frames[i].valid) {
                frames[i].page  = page;
                frames[i].valid = true;
                return {false, i};
            }
        }

        int victim           = nextIndex_;
        nextIndex_           = (nextIndex_ + 1) % static_cast<int>(frames.size());
        frames[victim].page  = page;
        frames[victim].valid = true;
        return {false, victim};
    }
};

class LruState final : public AlgoState {
    vector<int> lastUsed_;

public:
    explicit LruState(int frameCount) : lastUsed_(frameCount, -1) {}

    AccessRes access(int step, int page, vector<Frame>& frames, const vector<int>& /*ref*/) override {
        for (std::size_t i = 0; i < frames.size(); ++i) {
            if (frames[i].valid && frames[i].page == page) {
                lastUsed_[i] = step;
                return {true, -1};
            }
        }

        for (std::size_t i = 0; i < frames.size(); ++i) {
            if (!frames[i].valid) {
                frames[i].page  = page;
                frames[i].valid = true;
                lastUsed_[i]    = step;
                return {false, i};
            }
        }

        int victim = 0;
        int oldest = lastUsed_[0];
        for (std::size_t i = 1; i < frames.size(); ++i) {
            if (lastUsed_[i] < oldest) {
                oldest = lastUsed_[i];
                victim = static_cast<int>(i);
            }
        }

        frames[victim].page  = page;
        frames[victim].valid = true;
        lastUsed_[victim]    = step;
        return AccessRes{false, victim};
    }
};

class OptState final : public AlgoState {
public:
    explicit OptState(int /*frameCount*/) {}

    AccessRes access(int step, int page, vector<Frame>& frames, const vector<int>& ref) override {
        for (const auto& [p, valid] : frames) {
            if (valid && p == page) {
                return {true, -1};
            }
        }

        for (std::size_t i = 0; i < frames.size(); ++i) {
            if (!frames[i].valid) {
                frames[i].page  = page;
                frames[i].valid = true;
                return {false, static_cast<int>(i)};
            }
        }

        int victim          = -1;
        int farthestNextUse = -1;

        for (std::size_t i = 0; i < frames.size(); ++i) {
            int nextUse = -1;
            for (std::size_t j = step + 1; j < ref.size(); ++j) {
                if (ref[j] == frames[i].page) {
                    nextUse = static_cast<int>(j);
                    break;
                }
            }

            if (nextUse == -1) {
                victim = static_cast<int>(i);
                break;
            }

            if (nextUse > farthestNextUse) {
                farthestNextUse = nextUse;
                victim          = static_cast<int>(i);
            }
        }

        if (victim == -1) {
            victim = 0;
        }

        frames[victim].page  = page;
        frames[victim].valid = true;
        return {false, victim};
    }
};

unique_ptr<AlgoState> newAlgoState(ReplaceAlgo algo, int frameCount) {
    switch (algo) {
        case ReplaceAlgo::Fifo_algo:
            return make_unique<FifoState>(frameCount);
        case ReplaceAlgo::Opt_algo:
            return make_unique<OptState>(frameCount);
        case ReplaceAlgo::Lru_algo:
            return make_unique<LruState>(frameCount);
        default:
            return make_unique<FifoState>(frameCount);
    }
}

struct StepResult {
    int step;
    int page;
    bool hit;
    std::size_t victim;
    vector<Frame> frames;
};

vector<StepResult> simulate(ReplaceAlgo algo, int frameCount, const vector<int>& ref) {
    vector<Frame> frames(frameCount);
    auto state = newAlgoState(algo, frameCount);
    vector<StepResult> results;
    results.reserve(ref.size());

    for (std::size_t step = 0; step < ref.size(); ++step) {
        auto [hit, victim] = state->access(static_cast<int>(step), ref[step], frames, ref);
        results.push_back(StepResult{static_cast<int>(step), ref[step], hit, victim, frames,});
    }

    return results;
}

string algoName(ReplaceAlgo algo) {
    switch (algo) {
        case ReplaceAlgo::Fifo_algo: return "FIFO";
        case ReplaceAlgo::Opt_algo: return "OPT";
        case ReplaceAlgo::Lru_algo: return "LRU";
    }
    return "Unknown";
}

string frameSnapshot(const vector<Frame>& frames) {
    ostringstream oss;
    oss << "[";
    for (std::size_t i = 0; i < frames.size(); ++i) {
        if (i) oss << " | ";
        if (frames[i].valid) oss << frames[i].page;
        else oss << "-";
    }
    oss << "]";
    return oss.str();
}

void printResults(const vector<StepResult>& results) {
    int hits   = 0;
    int faults = 0;

    cout << left
            << setw(6) << "Step"
            << setw(8) << "Page"
            << setw(8) << "Hit?"
            << setw(10) << "Victim"
            << "Frames\n";
    cout << string(60, '-') << "\n";

    for (const auto& [step, page, hit, victim, frames] : results) {
        if (hit) ++hits;
        else ++faults;
        cout << left
                << setw(6) << step
                << setw(8) << page
                << setw(8) << (hit ? "Yes" : "No")
                << setw(10) << (victim >= 0 ? to_string(victim) : "-")
                << frameSnapshot(frames) << "\n";
    }

    cout << "\nHits: " << hits << ", Faults: " << faults
            << ", Hit Ratio: " << (results.empty() ? 0.0 : static_cast<double>(hits) / results.size())
            << "\n";
}

ReplaceAlgo selectAlgo(int choice) {
    switch (choice) {
        case 1: return ReplaceAlgo::Fifo_algo;
        case 2: return ReplaceAlgo::Opt_algo;
        case 3: return ReplaceAlgo::Lru_algo;
        default: return ReplaceAlgo::Fifo_algo;
    }
}

void runTests() {
    struct TestCase {
        ReplaceAlgo algo;
        int frames;
        vector<int> refs;
        string desc;
    };

    const vector<TestCase> cases = {
            {ReplaceAlgo::Fifo_algo, 3, {7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2}, "FIFO example with 3 frames"},
            {ReplaceAlgo::Opt_algo, 4, {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5}, "OPT example with 4 frames"},
            {ReplaceAlgo::Lru_algo, 3, {2, 3, 2, 1, 5, 2, 4, 5, 3, 2, 5, 2}, "LRU example with 3 frames"},
    };

    cout << "\n===== Running Built-in Tests =====\n";
    for (const auto& [algo, frames, refs, desc] : cases) {
        cout << "\nTest: " << desc << "\n";
        cout << "Algorithm: " << algoName(algo) << ", Frames: " << frames
                << ", Reference length: " << refs.size() << "\n";
        auto results = simulate(algo, frames, refs);
        printResults(results);
    }
    cout << "\n===== Tests Finished =====\n\n";
}

int main() {
    cout << "==== Page Replacement Simulator ====\n";
    cout << "Algorithms: 1) FIFO  2) OPT  3) LRU  4) Run Tests\n";
    cout << "Enter 0 as algorithm choice to exit.\n\n";

    while (true) {
        cout << "Select algorithm (0 to exit): ";
        int algoChoice;
        if (!(cin >> algoChoice)) {
            cin.clear();
            cin.ignore(1024, '\n');
            continue;
        }
        if (algoChoice == 0) {
            cout << "Exiting...\n";
            return 0;
        }
        if (algoChoice == 4) {
            runTests();
            continue;
        }

        cout << "Enter frame count: ";
        int frames;
        if (!(cin >> frames) || frames <= 0) {
            cout << "Invalid frame count.\n";
            cin.clear();
            cin.ignore(1024, '\n');
            continue;
        }

        cout << "Enter reference string (space separated integers):\n";
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        string line;
        getline(cin, line);
        istringstream iss(line);
        vector<int> refs;
        int value;
        while (iss >> value) {
            refs.push_back(value);
        }

        if (refs.empty()) {
            cout << "Reference string cannot be empty.\n";
            continue;
        }

        auto algo = selectAlgo(algoChoice);
        cout << "\nRunning " << algoName(algo) << " with "
                << frames << " frames on " << refs.size() << " references.\n\n";

        auto results = simulate(algo, frames, refs);
        printResults(results);
        cout << "\n";
    }
}
