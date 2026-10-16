#include "DSP/Clipper.h"
#include "DSP/Saturation.h"
#include <iostream>
#include <random>
#include <stdexcept>
void testDynamics();
void testAdvanced();
static void check(bool v, const char *message) {
    if (!v)
        throw std::runtime_error(message);
}
static void nonlinearTests() {
    for (double sr : {44100., 48000., 96000., 192000.}) {
        gummy::Saturation sat;
        sat.prepare(sr);
        gummy::Settings s;
        for (int n = 0; n < 2000; ++n) {
            float l = .3f * std::sin(float(n)), r = l;
            float ref = l;
            sat.process(l, r, s);
            check(l == ref && r == ref, "zero saturation must be exact unity");
        }
        std::array<double, 3> fingerprints{};
        for (int mode = 0; mode < 3; ++mode) {
            sat.prepare(sr);
            s.colorType = mode;
            s.colorDrive = 60;
            s.colorMix = 100;
            double dc = 0, power = 0;
            for (int n = 0; n < int(sr); ++n) {
                float l = .4f * std::sin(2 * gummy::pi * 997 * float(n) / float(sr)), r = l;
                sat.process(l, r, s);
                check(std::isfinite(l) && std::abs(l) < 4, "saturation stable");
                if (n > int(sr / 2)) {
                    dc += l;
                    power += double(l) * l;
                }
            }
            check(std::abs(dc / (sr / 2)) < .002, "tube DC removal");
            fingerprints[size_t(mode)] = power;
        }
        check(std::abs(fingerprints[0] - fingerprints[1]) > 1 &&
                  std::abs(fingerprints[1] - fingerprints[2]) > 1,
              "saturation modes measurably distinct");
        for (int mode = 0; mode < 2; ++mode)
            for (float ceiling : {0.f, -6.f, -12.f}) {
                gummy::Clipper clip;
                clip.prepare(sr);
                s.clipMode = mode;
                s.clipDrive = 24;
                s.ceiling = ceiling;
                for (int n = 0; n < 10000; ++n) {
                    float l = 20.f * std::sin(float(n)), r = -l;
                    clip.process(l, r, s);
                    check(std::abs(l) <= gummy::dbToGain(ceiling) + 1e-6f &&
                              std::abs(r) <= gummy::dbToGain(ceiling) + 1e-6f,
                          "clip ceiling");
                    check(std::abs(l + r) < 1e-6f, "odd clipping symmetry");
                }
            }
    }
    check(gummy::Clipper::hard(2) == 1 && gummy::Clipper::hard(-2) == -1, "hard transfer boundary");
    check(gummy::Clipper::gentle(.3f) == .3f && gummy::Clipper::gentle(.9f) < .9f, "soft transfer knee");
    for (float x : {-100.f, -2.f, -1.f, -.8f, 0.f, .8f, 1.f, 2.f, 100.f})
        check(std::abs(gummy::Clipper::gentle(x)) <= 1, "bounded soft clip");
    std::cout << "Nonlinear transfer, mode, DC, and ceiling tests passed\n";
}
int main() {
    try {
        testDynamics();
        testAdvanced();
        nonlinearTests();
        std::cout << "All standalone DSP tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
