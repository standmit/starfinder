#pragma once


#include <vector>
#include <string>


namespace starfinder {


/**
 * \brief   Represents a star with its right ascension, declination, and magnitude.
 */
struct Star {
    const double ra_deg;
    const double de_deg;
    const double mag;

    Star(
        const double ra_deg,
        const double de_deg,
        const double mag
    ) noexcept;
};


std::vector<Star> read_stars(const std::string& path);


} // namespace starfinder