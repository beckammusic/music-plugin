#pragma once
#include "Common.h"

namespace gummy {

// Broad, minimum-phase tone shaping. All coefficient/state storage is fixed size.
class MusicalEQ {
public:
    void prepare(double sampleRate) noexcept {
        sr = std::isfinite(sampleRate) ? std::clamp(sampleRate, 8000.0, 768000.0) : 48000.0;
        body.prepare(sr, 30.0f); presence.prepare(sr, 30.0f); air.prepare(sr, 30.0f);
        reset();
    }

    void reset() noexcept {
        for (auto& section : filters) section = {};
        body.reset(); presence.reset(); air.reset();
        updateCounter = 0;
        first = true;
    }

    void process(float& left, float& right, const Settings& settings) noexcept {
        const float bodyDb = .09f * body.next(std::clamp(finite(settings.body), -100.0f, 100.0f));
        const float presenceDb = .09f * presence.next(std::clamp(finite(settings.presence), -100.0f, 100.0f));
        const float airDb = .09f * air.next(std::clamp(finite(settings.air), -100.0f, 100.0f));
        if (updateCounter-- == 0) {
            // BODY divides its gain between a bass shelf and a broad low-mid bell.
            filters[0].target = shelf(150.0, bodyDb * .95, false);
            filters[1].target = bell(280.0, .55, bodyDb * .30);
            filters[2].target = bell(2800.0, .65, presenceDb);
            filters[3].target = shelf(9000.0, airDb, true);
            if (first) {
                for (auto& section : filters) section.current = section.target;
                first = false;
            }
            updateCounter = 15;
        }
        double l = finite(left), r = finite(right);
        for (auto& section : filters) {
            section.current.approach(section.target);
            l = section.tick(l, 0);
            r = section.tick(r, 1);
        }
        left = finite(static_cast<float>(l));
        right = finite(static_cast<float>(r));
    }

private:
    struct Coefficients {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
        void approach(const Coefficients& c) noexcept {
            constexpr double rate = 1.0 / 16.0;
            b0 += rate * (c.b0 - b0); b1 += rate * (c.b1 - b1);
            b2 += rate * (c.b2 - b2); a1 += rate * (c.a1 - a1);
            a2 += rate * (c.a2 - a2);
        }
    };
    struct Section {
        Coefficients current, target;
        std::array<double, 2> z1{}, z2{};
        double tick(double x, std::size_t channel) noexcept {
            const double y = current.b0 * x + z1[channel];
            z1[channel] = current.b1 * x - current.a1 * y + z2[channel];
            z2[channel] = current.b2 * x - current.a2 * y;
            if (std::abs(z1[channel]) < 1.0e-30) z1[channel] = 0;
            if (std::abs(z2[channel]) < 1.0e-30) z2[channel] = 0;
            return y;
        }
    };

    static Coefficients normalized(double b0, double b1, double b2,
                                   double a0, double a1, double a2) noexcept {
        return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
    }
    double angle(double hz) const noexcept {
        return 2.0 * double(pi) * std::min(hz, sr * .42) / sr;
    }
    Coefficients bell(double hz, double q, double db) const noexcept {
        const double a = std::pow(10.0, db / 40.0), w = angle(hz);
        const double c = std::cos(w), alpha = std::sin(w) / (2.0 * q);
        return normalized(1 + alpha * a, -2 * c, 1 - alpha * a,
                          1 + alpha / a, -2 * c, 1 - alpha / a);
    }
    Coefficients shelf(double hz, double db, bool high) const noexcept {
        const double a = std::pow(10.0, db / 40.0), w = angle(hz), c = std::cos(w);
        // Shelf slope S=.75 deliberately avoids resonant overshoot.
        const double alpha = std::sin(w) * .5 * std::sqrt((a + 1 / a) * (1 / .75 - 1) + 2);
        const double beta = 2 * std::sqrt(a) * alpha;
        if (high)
            return normalized(a * ((a + 1) + (a - 1) * c + beta),
                              -2 * a * ((a - 1) + (a + 1) * c),
                              a * ((a + 1) + (a - 1) * c - beta),
                              (a + 1) - (a - 1) * c + beta,
                              2 * ((a - 1) - (a + 1) * c),
                              (a + 1) - (a - 1) * c - beta);
        return normalized(a * ((a + 1) - (a - 1) * c + beta),
                          2 * a * ((a - 1) - (a + 1) * c),
                          a * ((a + 1) - (a - 1) * c - beta),
                          (a + 1) + (a - 1) * c + beta,
                          -2 * ((a - 1) + (a + 1) * c),
                          (a + 1) + (a - 1) * c - beta);
    }

    double sr = 48000.0;
    std::array<Section, 4> filters{};
    Slew body, presence, air;
    unsigned updateCounter = 0;
    bool first = true;
};
}
