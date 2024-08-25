#pragma once
#include <chrono>
#include <span>
#include <vector>

#include "../screen.hpp"
#include "save_state.hpp"

namespace bemu::gb {

namespace detail {
struct VectorOutputBuffer {
    std::vector<u8>& m_buffer;

    void write(const u8 data) { m_buffer.push_back(data); }
};

struct VectorInputBuffer {
    std::vector<u8>& m_buffer;
    size_t m_index = 0;

    u8 read() { return m_buffer[m_index++]; }

    operator bool() const { return m_index < m_buffer.size(); }
};

/// Stores a (start, length, data) of bytes
struct DiffEntry {
    u32 m_start;
    u8 m_length;
    std::vector<u8> m_data;

    void save(auto& ar) const {
        ar(m_start);
        ar(m_length);
        for (const auto& byte : m_data) {
            ar(byte);
        }
    }

    void load(auto& ar) {
        ar(m_start);
        ar(m_length);
        m_data.resize(m_length);
        for (auto& byte : m_data) {
            ar(byte);
        }
    }

    [[nodiscard]] bool contains(const size_t i) const { return i >= m_start && i < m_start + m_length; }
    [[nodiscard]] u32 last_index() const { return m_start + m_length - 1; }
};

/// Writes the diff between a base buffer and new data
struct VectorDiffOutputBuffer {
    /// Base bytes to compare against
    ///
    /// Diffs are calculated against this buffer
    const std::vector<u8>& m_base;

    /// Encoded diff output, sequence of DiffEntry
    std::vector<u8>& m_output;

    /// Next byte being compared against m_base
    size_t m_base_index = 0;

    /// Current diff being evaluated
    std::optional<DiffEntry> m_current_entry;

    void write(const u8 data) {
        if (m_base_index >= m_base.size()) {
            throw std::runtime_error("Inconsistent state sizes");
        }

        const auto existing = m_base[m_base_index++];

        if (existing != data) {
            if (m_current_entry) {
                m_current_entry->m_length++;
                m_current_entry->m_data.push_back(data);
                if (m_current_entry->m_length == 0xFF) {
                    write_entry();
                }
            } else {
                m_current_entry.emplace();
                m_current_entry->m_start = m_base_index - 1;
                m_current_entry->m_length = 1;
                m_current_entry->m_data.push_back(data);
            }
        } else if (m_current_entry) {
            write_entry();
        }
    }

    void write_entry() {
        if (!m_current_entry) return;

        VectorOutputBuffer buffer{m_output};
        StateOutputArchive archive{buffer};
        m_current_entry->save(archive);

        m_current_entry.reset();
    }
};

/// Combines a base buffer and a diff buffer to reconstruct the full data
struct VectorDiffInputBuffer {
    VectorInputBuffer m_base_buffer;
    VectorInputBuffer m_diff_buffer;
    std::optional<DiffEntry> m_current_entry;

    VectorDiffInputBuffer(std::vector<u8>& base, std::vector<u8>& diff_buffer)
        : m_base_buffer(base), m_diff_buffer(diff_buffer) {}

    u8 read() {
        const auto i = m_base_buffer.m_index;
        const auto base = m_base_buffer.read();

        // Load next entry if needed
        read_entry();

        // Inside current entry
        if (m_current_entry && m_current_entry->contains(i)) {
            const auto offset = i - m_current_entry->m_start;
            const auto result = m_current_entry->m_data[offset];

            // End of current entry
            if (i == m_current_entry->last_index()) {
                m_current_entry.reset();
            }

            return result;
        }

        return base;
    }

    void read_entry() {
        if (!m_current_entry && m_diff_buffer) {
            StateInputArchive archive{m_diff_buffer};
            m_current_entry.emplace();
            m_current_entry->load(archive);
        }
    }
};
}  // namespace detail

/// Storage for rewind states
///
/// Save states are stored in buckets, each bucket containing a number of frames. By default, 60 frames are stored per
/// bucket (1 second at 60fps), up to 256 MB of memory or 100'000 buckets (about 24 hours).
///
/// Each bucket consists of a set of states. The first state in each bucket is a full save state, while subsequent
/// states are stored as diffs from the first state. This allows for efficient storage of multiple states while
/// minimizing memory usage.
template <class TEmulator>
struct Rewind {
    explicit Rewind(TEmulator& emulator, const size_t max_bytes = 256 * 1024 * 1024, const size_t max_buckets = 100'000,
                    const size_t frames_in_bucket = 60)
        : m_emulator(emulator),
          m_max_bytes(max_bytes),
          m_max_buckets(max_buckets),
          m_frames_in_bucket(frames_in_bucket) {
        if (m_max_bytes == 0 || m_max_buckets == 0 || m_frames_in_bucket == 0) {
            throw std::invalid_argument("Rewind parameters must be greater than 0");
        }
    }

    [[nodiscard]] size_t get_max_bytes() const { return m_max_bytes; }

    [[nodiscard]] size_t get_used_bytes() const {
        size_t result = 0;
        for (const auto& bucket : m_buckets) {
            for (const auto& state : bucket.m_states) {
                result += state.m_data.size();
            }
        }
        return result;
    }

    [[nodiscard]] size_t get_num_states() const {
        size_t result = 0;
        for (const auto& bucket : m_buckets) {
            result += bucket.m_states.size();
        }
        return result;
    }

    [[nodiscard]] bool is_at_capacity() const {
        return get_used_bytes() >= m_max_bytes || m_buckets.size() >= m_max_buckets;
    }
    [[nodiscard]] size_t get_first_ticks() const {
        if (m_buckets.empty() || m_buckets.front().m_states.empty()) {
            return m_emulator.get_tick_count();
        }
        return m_buckets.front().m_states.front().m_ticks;
    }

    void push_state(std::chrono::system_clock::time_point now = std::chrono::system_clock::now()) {
        auto& bucket = prepare_bucket();
        auto& state = bucket.m_states.emplace_back();
        state.m_wall_time = now;
        state.m_ticks = m_emulator.get_tick_count();
        state.m_screenshot = m_emulator.get_screen();

        if (bucket.m_states.size() == 1) {
            // First state in bucket, save full state
            auto buffer = detail::VectorOutputBuffer{state.m_data};
            auto archive = StateOutputArchive{buffer};
            m_emulator.serialize(archive);
        } else {
            // Save diff from base state
            auto& base_state = bucket.m_states.front();

            auto buffer = detail::VectorDiffOutputBuffer{base_state.m_data, state.m_data};
            auto archive = StateOutputArchive{buffer};
            m_emulator.serialize(archive);
            buffer.write_entry();  // Flush any remaining diff data
        }

        free_space();
    }

    bool pop_state() {
        if (m_buckets.empty()) {
            return false;
        }

        auto& bucket = m_buckets.back();

        // Load ful state
        if (bucket.m_states.size() == 1) {
            auto buffer = detail::VectorInputBuffer{bucket.m_states.front().m_data};
            auto archive = StateInputArchive{buffer};
            m_emulator.serialize(archive);

            // Remove entire bucket
            m_buckets.pop_back();
        } else {
            // Load diff state
            auto& base = bucket.m_states.front();
            auto& state = bucket.m_states.back();

            auto buffer = detail::VectorDiffInputBuffer{base.m_data, state.m_data};
            auto archive = StateInputArchive{buffer};
            m_emulator.serialize(archive);

            // Remove last state
            bucket.m_states.pop_back();
        }

        return true;
    }

    void clear() { m_buckets.clear(); }

   private:
    struct State {
        std::chrono::system_clock::time_point m_wall_time;
        u64 m_ticks;
        Screen m_screenshot;
        std::vector<u8> m_data;
    };

    struct Bucket {
        std::vector<State> m_states;
    };

    /// Free up som space by removing the oldest buckets
    void free_space() {
        while (is_at_capacity()) {
            m_buckets.erase(m_buckets.begin());
        }
    }

    /// Returns [base state, new state]
    Bucket& prepare_bucket() {
        if (m_buckets.empty() || m_buckets.back().m_states.size() >= m_frames_in_bucket) {
            return m_buckets.emplace_back();
        }
        return m_buckets.back();
    }

    TEmulator& m_emulator;
    size_t m_max_bytes;
    size_t m_max_buckets;
    size_t m_frames_in_bucket;
    std::vector<Bucket> m_buckets;
};
}  // namespace bemu::gb
