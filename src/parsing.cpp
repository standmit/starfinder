#include <starfinder/parsing.hpp>
#include <execution>
#include <fstream>
#include <optional>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/format.hpp>


namespace starfinder {


Star::Star(
            const double ra_deg_arg,
            const double de_deg_arg,
            const double mag_arg
) noexcept :
        ra_deg(ra_deg_arg),
        de_deg(de_deg_arg),
        mag(mag_arg)
{}


std::optional<double> parse_field(
        const std::vector<std::string>& record,
        const size_t index,
        const std::string& field_name,
        std::string& errmes
 ) noexcept {
    double value;

    try {
        value = std::stod(record.at(index));
    }
    catch (const std::out_of_range&) {
        errmes = (boost::format("Missing field: %1%") % field_name).str();
        return {};
    }
    catch (const std::invalid_argument& e) {
        errmes = (boost::format("Failed to parse %1%. (%2%)") % field_name % e.what()).str();
        return {};
    }
    
    return value;
}


std::optional<double> parse_magnitude(const std::vector<std::string>& record, std::string& errmes) noexcept {
    std::string b_err, v_err;
    const std::optional<double> bt_mag = parse_field(record, 17, "BT magnitude", b_err);
    const std::optional<double> vt_mag = parse_field(record, 19, "VT magnitude", v_err);

    if (bt_mag) {
        const auto bt = bt_mag.value();
        if (vt_mag) {
            const auto vt = vt_mag.value();
            return vt - 0.090 * (bt - vt);
        } else {
            return bt;
        }
    } else {
        if (vt_mag) {
            return vt_mag.value();
        } else {
            errmes = "Missing magnitude. ";
            errmes += b_err;
            errmes += ". ";
            errmes += v_err;
            return {};
        }
    }
}


std::optional<Star> parse_star_record(const std::vector<std::string>& record, std::string& errmes) noexcept {
    const auto ra = parse_field(record, 24, "RA", errmes);
    if (!ra.has_value())
        return {};

    const auto dec = parse_field(record, 25, "Dec", errmes);
    if (!dec.has_value())
        return {};

    const auto mag = parse_magnitude(record, errmes);
    if (!mag.has_value())
        return {};

    return Star(
        ra.value(),
        dec.value(),
        mag.value()
    );
}


std::vector<Star> read_stars(const std::string& path) {
    std::vector<Star> stars;
    {
        std::vector<std::vector<std::string>> records;
        float reading_elapsed;
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

        for (const auto& record : records) {
            std::string errmes;
            const auto star = parse_star_record(record, errmes);
            if (not star.has_value())
                continue;

            stars.push_back(star.value());
        }
        stars.shrink_to_fit();
    }

    return stars;
};


} // namespace starfinder