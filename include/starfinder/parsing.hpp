#pragma once


#include <vector>
#include <string>


namespace starfinder {


#pragma pack(push, 1)

/**
 * \brief   Represents a star with its right ascension, declination, and magnitude.
 */
struct Star {
    const double ra;
    const double dec;
    const double mag;

    Star(
        const double ra,
        const double dec,
        const double mag
    ) noexcept;
};

#pragma pack(pop)


std::vector<Star> read_stars(const std::string& path);


} // namespace starfinder