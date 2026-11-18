#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

namespace duster {

// Runs the engine at kEngineRate whatever the host rate. Engine samples go into a FIFO that a
// windowed-sinc interpolator reads at the host rate. The FIFO starts kLead samples ahead, so the
// engine is never past a note's position when the note arrives: advanceTo() renders up to the
// note's engine sample, and the note starts exactly there. At a 48 kHz host it copies straight
// through with no latency.
class HostRateResampler {
  public:
    void prepare(double hostRate, int maxHostBlock); // allocates; message thread

    // Fills hostOut with hostSamples. render(float* dest, int count) renders engine samples.
    template <class Render> void process(float* hostOut, int hostSamples, Render&& render) noexcept {
        while (hostSamples > 0) {
            const int chunk = std::min(hostSamples, maxHostBlock_);
            if (passthrough_) {
                render(hostOut, chunk);
                engineRendered_ += chunk;
            } else {
                fill(static_cast<int>(std::ceil(chunk * ratio_)) + 2, render);
                read_ += interpolator_.process(ratio_, fifo_.data() + read_, hostOut, chunk);
            }
            hostDone_ += chunk;
            hostOut += chunk;
            hostSamples -= chunk;
        }
    }

    // Renders the engine up to the engine sample under the current host position, so a note started
    // next lands on it.
    template <class Render> void advanceToNow(Render&& render) noexcept {
        const auto target = std::llround(static_cast<double>(hostDone_) * ratio_);
        if (target > engineRendered_)
            fill(static_cast<int>(target - engineRendered_) + (write_ - read_), render);
    }

    int latencyHostSamples() const noexcept { return latency_; }

  private:
    // Engine samples the FIFO runs ahead: more than a host chunk ever leaves unread (3 at most).
    static constexpr int kLead = 8;

    // Makes at least `unread` engine samples available in the FIFO.
    template <class Render> void fill(int unread, Render&& render) noexcept {
        const int missing = unread - (write_ - read_);
        if (missing <= 0)
            return;
        if (write_ + missing > static_cast<int>(fifo_.size())) {
            std::memmove(fifo_.data(), fifo_.data() + read_,
                         static_cast<std::size_t>(write_ - read_) * sizeof(float));
            write_ -= read_;
            read_ = 0;
        }
        render(fifo_.data() + write_, missing);
        write_ += missing;
        engineRendered_ += missing;
    }

    juce::WindowedSincInterpolator interpolator_;
    std::vector<float> fifo_;
    double ratio_ = 1.0; // engine samples per host sample
    bool passthrough_ = true;
    int maxHostBlock_ = 512;
    int read_ = 0, write_ = 0;
    std::int64_t engineRendered_ = 0, hostDone_ = 0;
    int latency_ = 0;
};

} // namespace duster
