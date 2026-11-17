#pragma once

namespace ProGraphics {

/** 实时与静态 PRPD 共用的默认规格。 */
struct PRPDConstants {
    static constexpr float GL_AXIS_LENGTH = 5.0f;
    static constexpr float POINT_SIZE = 8.0f;
    static constexpr int PHASE_POINTS = 200;
    static constexpr int MAX_CYCLES = 500;
    static constexpr float PHASE_MAX = 360.0f;
    static constexpr float PHASE_MIN = 0.0f;
    static constexpr int AMPLITUDE_BINS = 100;
};

} // namespace ProGraphics
