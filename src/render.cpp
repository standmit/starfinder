#include <chrono>
#include <execution>
#include <iostream>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <starfinder/parsing.hpp>


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


std::vector<starfinder::Star> filter_stars(
        const std::vector<starfinder::Star>& all_stars,
        const double min_ra,
        const double max_ra,
        const double min_dec,
        const double max_dec,
        const double max_magnitude
) {
    std::vector<starfinder::Star> filtered_stars;
    filtered_stars.reserve(all_stars.size());
    const Stopwatch filtering;
    std::copy_if(
        std::execution::seq,
        all_stars.cbegin(),
        all_stars.cend(),
        std::back_inserter(filtered_stars),
        [min_ra, max_ra, min_dec, max_dec, max_magnitude] (const starfinder::Star& star) noexcept {
            return (
                star.ra >= min_ra
                &&
                star.ra <= max_ra
                &&
                star.dec >= min_dec
                &&
                star.dec <= max_dec
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
        const std::vector<starfinder::Star>& stars,
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
        [] (const starfinder::Star& a, const starfinder::Star& b) {
            return (a.mag < b.mag);
        }
    );
    const auto min_mag = min_mag_star->mag;
    const auto max_mag = max_mag_star->mag;

    const auto ra_range = max_ra - min_ra;
    const auto dec_range = max_dec - min_dec;
    const auto mag_range = max_mag - min_mag;
    for (const starfinder::Star& star : stars) {
        const uint32_t x = (star.ra - min_ra) / ra_range * width;
        const uint32_t y = (star.dec - min_dec) / dec_range * height;

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
        starfinder::read_stars(vm[OPT_FILE].as<std::string>()),
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