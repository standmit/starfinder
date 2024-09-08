#include <starfinder/parsing.hpp>
#include <execution>
#include <fstream>
#include <optional>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/format.hpp>


namespace starfinder {


Star::Star(
            const double ra_arg,
            const double dec_arg,
            const double mag_arg
) noexcept :
        ra(ra_arg),
        dec(dec_arg),
        mag(mag_arg)
{}


constexpr uint8_t RA_FIELD = 24;
constexpr uint8_t DEC_FIELD = 25;
constexpr uint8_t BT_FIELD = 17;
constexpr uint8_t VT_FIELD = 19;
constexpr uint8_t MIN_FIELDS_COUNT = std::max(
    {
        RA_FIELD,
        DEC_FIELD,
        BT_FIELD,
        VT_FIELD
    }
) + 1;


std::vector<Star> read_stars(const std::string& path) {
    std::vector<Star> stars;
    {
        std::vector<std::vector<std::string>> records;
        {
            std::vector<std::string> rows;
            {
                std::ifstream file(path);
                file.seekg(0, std::ios::end);
                rows.reserve(file.tellg());
                file.seekg(0);
                for (std::string line; std::getline(file, line);)
                    rows.emplace_back(std::move(line));
            }

            records.resize(rows.size());
            std::transform(
                std::execution::par_unseq,
                rows.cbegin(),
                rows.cend(),
                records.begin(),
                [] (const std::string& row) {
                    std::vector<std::string> record;
                    boost::split(
                        record,
                        row,
                        boost::is_any_of("|")
                    );
                    return record;
                }
            );
        }

        stars.reserve(records.size());
        for (const auto& record : records) {
            if (record.size() < MIN_FIELDS_COUNT)
                continue;

            double ra, dec;
            try {
                ra = std::stod(record.at(RA_FIELD));
                dec = std::stod(record.at(DEC_FIELD));
            }
            catch (const std::invalid_argument&) {
                continue;
            }

            std::optional<double> bt_mag;
            try {
                bt_mag = std::stod(record.at(BT_FIELD));
            } catch (const std::invalid_argument&) {}

            std::optional<double> vt_mag;
            try {
                vt_mag = std::stod(record.at(VT_FIELD));
            } catch (const std::invalid_argument&) {}

            double mag;
            if (bt_mag.has_value()) {
                if (vt_mag.has_value()) {
                    const auto vt = vt_mag.value();
                    mag = vt - 0.090 * (bt_mag.value() - vt);
                } else
                    mag = bt_mag.value();
            } else {
                if (vt_mag.has_value())
                    mag = vt_mag.value();
                else
                    continue;
            }

            stars.emplace_back(ra, dec, mag);
        }
    }

    stars.shrink_to_fit();
    return stars;
}


} // namespace starfinder