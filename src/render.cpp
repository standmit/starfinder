#include <chrono>
#include <execution>
#include <fstream>
#include <iostream>
#include <optional>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>


namespace po = boost::program_options;


constexpr char OPT_HELP[] = "help";
constexpr char OPT_FILE[] = "FILE";
constexpr char OPT_MIN_RA[] = "min-ra";
constexpr char OPT_MAX_RA[] = "max-ra";
constexpr char OPT_MIN_DEC[] = "min-dec";
constexpr char OPT_MAX_DEC[] = "max-dec";
constexpr char OPT_MAX_MAGNITUDE[] = "max-magnitude";
constexpr char OPT_WIDTH[] = "width";
constexpr char OPT_HEIGHT[] = "height";
constexpr char OPT_OUTPUT[] = "output";


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
    ) noexcept:
            ra_deg(ra_deg),
            de_deg(de_deg),
            mag(mag)
    {}
};


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
            const auto v_mag = vt - 0.090 * (bt - vt);
            // std::cout << boost::format("Debug: Calculated V_Mag = %1$.3f") % v_mag << std::endl;
            return v_mag;
        } else {
            // std::cout << boost::format("Debug: Using BT_Mag as V_Mag = %1$.3f") % bt << std::endl;
            return bt;
        }
    } else {
        if (vt_mag) {
            const auto vt = vt_mag.value();
            // std::cout << boost::format("Debug: Using VT_Mag as V_Mag = %1$.3f") % vt << std::endl;
            return vt;
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


template <class Clock = std::chrono::high_resolution_clock>
class Stopwatch {
    public:
        Stopwatch();

        template <typename Duration = std::chrono::milliseconds>
        float elapsed() const;

    private:
        const typename Clock::time_point start;
};


template <class Clock>
Stopwatch<Clock>::Stopwatch():
        start(Clock::now())
{}


template <class Clock>
template <typename Duration>
float Stopwatch<Clock>::elapsed() const {
    const auto stop = Clock::now();
    return static_cast<float>(std::chrono::duration_cast<Duration>(stop - start).count()) / Duration::period::den;
}


std::vector<Star> read_stars(const std::string& path) {
    std::vector<Star> stars;
    {
        std::vector<std::vector<std::string>> records;
        const Stopwatch start_reading;
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
                reading_elapsed = start_reading.elapsed();
                std::cout << "Time taken to read catalog: " << reading_elapsed << std::endl;
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

        std::size_t i = 0;
        std::size_t skipped_rows = 0;
        for (const auto& record : records) {
            i++;
            std::string errmes;
            const auto star = parse_star_record(record, errmes);
            if (not star.has_value()) {
                skipped_rows++;
                if (skipped_rows <= 10)
                    std::cerr << boost::format("Skipping row %1% due to error: %2%") % (i-1) % errmes << std::endl;
                else if (skipped_rows == 11)
                    std::cerr << "Further skipped rows will not be printed..." << std::endl;
                continue;
            }

            stars.push_back(star.value());
        }
        stars.shrink_to_fit();
        std::cout << "Time taken to parsing: " << (start_reading.elapsed() - reading_elapsed) << std::endl;
    }

    return stars;
};


std::vector<Star> filter_stars(
        const std::vector<Star>& all_stars,
        const double min_ra,
        const double max_ra,
        const double min_dec,
        const double max_dec,
        const double max_magnitude
) {
    std::vector<Star> filtered_stars;
    filtered_stars.reserve(all_stars.size());
    const Stopwatch filtering;
    std::copy_if(
        std::execution::par,
        all_stars.cbegin(),
        all_stars.cend(),
        std::back_inserter(filtered_stars),
        [min_ra, max_ra, min_dec, max_dec, max_magnitude] (const Star& star) noexcept {
            return (
                star.ra_deg >= min_ra
                &&
                star.ra_deg <= max_ra
                &&
                star.de_deg >= min_dec
                &&
                star.de_deg <= max_dec
                &&
                star.mag <= max_magnitude
            );
        }
    );
    std::cout << "Time taken to filtering: " << filtering.elapsed<std::chrono::microseconds>() << std::endl;
    filtered_stars.shrink_to_fit();
    return filtered_stars;
}


void render_stars(
        const std::vector<Star>& stars,
        const uint32_t width,
        const uint32_t height,
        const double min_ra,
        const double max_ra,
        const double min_dec,
        const double max_dec,
        cv::OutputArray dst
) {
    dst.create(height, width, CV_8UC1);
    cv::Mat img = dst.getMat();
    img.setTo(0);

    if (stars.size() == 0)
        return;

    // Find the minimum and maximum magnitudes in the dataset
    const auto [min_mag_star, max_mag_star] = std::minmax_element(
        stars.cbegin(),
        stars.cend(),
        [] (const Star& a, const Star& b) {
            return (a.mag < b.mag);
        }
    );
    const auto min_mag = min_mag_star->mag;
    const auto max_mag = max_mag_star->mag;

    const auto ra_range = max_ra - min_ra;
    const auto dec_range = max_dec - min_dec;
    const auto mag_range = max_mag - min_mag;
    for (const Star& star : stars) {
        const uint32_t x = (star.ra_deg - min_ra) / ra_range * width;
        const uint32_t y = (star.de_deg - min_dec) / dec_range * height;

        if (x < width && y < height) {
            // Inverse the magnitude scale (brighter stars have lower magnitudes)
            const auto normalized_mag = (max_mag - star.mag) / mag_range;

            // Apply a non-linear scaling to emphasize brighter stars
            const uint8_t brightness = std::pow(normalized_mag, 2.5) * 255;

            cv::circle(
                img,
                cv::Point(x, y),
                0,
                cv::Scalar(brightness)
            );
        }
    }
}


int main(int argc, char** argv) {
    po::variables_map vm;
    {
        po::options_description general_options("General options");
        general_options.add_options()
            (OPT_HELP, "print this message")
            (OPT_WIDTH, po::value<uint32_t>()->default_value(800), "Output image width in pixels")
            (OPT_HEIGHT, po::value<uint32_t>()->default_value(600), "Output image height in pixels")
            (OPT_OUTPUT, po::value<std::string>()->default_value("star_map.png"), "Output image file name")
        ;

        po::options_description filter_options("Filter options");
        filter_options.add_options()
            (OPT_MIN_RA, po::value<double>()->default_value(0), "Minimum Right Ascension (degrees)")
            (OPT_MAX_RA, po::value<double>()->default_value(360), "Maximum Right Ascension (degrees)")
            (OPT_MIN_DEC, po::value<double>()->default_value(-90), "Minimum Declination (degrees)")
            (OPT_MAX_DEC, po::value<double>()->default_value(90), "Maximum Declination (degrees)")
            (OPT_MAX_MAGNITUDE, po::value<double>()->default_value(6), "Maximum visual magnitude (lower is brighter)")
        ;

        po::options_description arguments("Arguments");
        arguments.add_options()
            (OPT_FILE, po::value<std::string>()->default_value("data/tycho2/catalog.dat"), "Path to the Tycho-2 catalog file")
        ;

        po::positional_options_description arguments_positions;
        arguments_positions.add(OPT_FILE, 1);

        po::options_description all_options("All options");
        all_options.add(general_options).add(filter_options).add(arguments);

        po::store(
            po::command_line_parser(argc, argv).options(all_options).positional(arguments_positions).run(),
            vm
        );
        po::notify(vm);

        if (vm.count(OPT_HELP) != 0) {
            std::cout << "render [options]";
            std::cout << ' ' << OPT_FILE;
            std::cout << std::endl << std::endl;
            std::cout << arguments << std::endl;
            std::cout << general_options << std::endl;
            std::cout << filter_options << std::endl;
            return -1;
        }
    }


    std::cout << boost::format("Reading stars from: %1%") % vm[OPT_FILE].as<std::string>() << std::endl;
    std::cout << boost::format("RA range: %1% to %2%") % vm[OPT_MIN_RA].as<double>() % vm[OPT_MAX_RA].as<double>() << std::endl;
    std::cout << boost::format("Dec range: %1% to %2%") % vm[OPT_MIN_DEC].as<double>() % vm[OPT_MAX_DEC].as<double>() << std::endl;
    std::cout << boost::format("Max magnitude: %1%") % vm[OPT_MAX_MAGNITUDE].as<double>() << std::endl;

    const auto stars = filter_stars(
        read_stars(vm[OPT_FILE].as<std::string>()),
        vm[OPT_MIN_RA].as<double>(),
        vm[OPT_MAX_RA].as<double>(),
        vm[OPT_MIN_DEC].as<double>(),
        vm[OPT_MAX_DEC].as<double>(),
        vm[OPT_MAX_MAGNITUDE].as<double>()
    );
    std::cout << "Total stars: " << stars.size() << std::endl;

    cv::Mat img;
    const Stopwatch<std::chrono::high_resolution_clock> render_start;
    render_stars(
        stars,
        vm[OPT_WIDTH].as<uint32_t>(),
        vm[OPT_HEIGHT].as<uint32_t>(),
        vm[OPT_MIN_RA].as<double>(),
        vm[OPT_MAX_RA].as<double>(),
        vm[OPT_MIN_DEC].as<double>(),
        vm[OPT_MAX_DEC].as<double>(),
        img
    );
    cv::imwrite(vm[OPT_OUTPUT].as<std::string>(), img);
    const auto render_duration = render_start.elapsed();

    std::cout << "Time taken to render and save image: " << render_duration << std::endl;
    std::cout << "Image saved as: " << vm[OPT_OUTPUT].as<std::string>() << std::endl;

    return 0;
}