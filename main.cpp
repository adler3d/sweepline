#include "sweepline.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <istream>
#include <iterator>
#include <limits>
#include <ostream>
#include <random>
#include <sstream>
#include <vector>

#include <cassert>
#include <cmath>
#include <cstdlib>

#include <x86intrin.h>

using size_type = std::size_t;

using value_type = double;

struct point
{

    value_type x, y;

};

struct point_less
{

    value_type const & eps_;

    bool operator () (point const & _lhs, point const & _rhs) const
    {
        value_type const & x = _lhs.x + eps_;
        value_type const & y = _lhs.y + eps_;
        return std::tie(x, y) < std::tie(_rhs.x, _rhs.y);
    }

};

namespace
{

// bounding box
value_type const bbox = value_type(10);
value_type const delta = value_type(1E-3);

value_type const eps = std::numeric_limits< value_type >::epsilon();
value_type const zero = value_type(0);
value_type const one = value_type(1);

point_less const point_less_{delta};

std::ostream & gnuplot_ = std::cout;
std::ostream & log_ = std::clog;

void
generate(std::ostream & _out, size_type const N = 10000)
{
    using seed_type = typename std::mt19937::result_type;
#if 0
    seed_type const seed_ = 23902348445254;//19614518643971;//8864935383105;
#elif 0
    std::random_device D;
    auto const seed_ = static_cast< seed_type >(D());
#elif 1
    //auto const seed_ = static_cast< seed_type >(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    auto const seed_ = static_cast< seed_type >(__rdtsc());
#endif
    log_ << seed_ << '\n';
    gnuplot_ << "set title 'seed = 0x" << std::hex << seed_ << ", N = " <<  std::dec << N << "'\n";
    std::mt19937 g{seed_};
    std::normal_distribution< value_type > normal_;
    std::set< point, point_less > points_{point_less_};
    std::uniform_real_distribution< value_type > zero_to_one_{zero, std::nextafter(one, one + one)};
    _out << N << "\n";
    points_.clear();
    value_type const twosqreps = eps * (eps + eps);
    for (size_type n = 0; n < N; ++n) { // points that are uniformely distributed inside of closed ball
        for (;;) {
            point p{normal_(g), normal_(g)};
            value_type norm = p.x * p.x + p.y * p.y;
            if (twosqreps < norm) {
                using std::sqrt;
                norm = bbox * sqrt(zero_to_one_(g) / std::move(norm));
                p.x *= norm;
                p.y *= norm;
                if (points_.insert(std::move(p)).second) {
                    break;
                }
            } else {
                if (points_.insert({zero, zero}).second) {
                    break;
                }
            }
        }
    }
    for (point const & point_ : points_) {
        _out << point_.x << ' ' << point_.y << '\n';
    }
}

} // namespace

int
main()
{
#if 0
    std::istream & in_ = std::cin;
#elif 1
    std::stringstream in_;
    generate(in_, 10);
#elif 0
    std::stringstream in_;
    in_ << "3\n"
           "0 0\n"
           "1 -0.1\n"
           "3 -1\n";
#elif 0
    std::stringstream in_;
    in_ << "2\n"
           "1 0\n"
           "0 1\n";
#endif
    size_type N{};
    if (!(in_ >> N)) {
        assert(false);
    }
    assert(0 < N);
    using points = std::vector< point >;
    points points_;
    points_.reserve(N);
    for (size_type n = 0; n < N; ++n) {
        point & point_ = points_.emplace_back();
        if (!(in_ >> point_.x)) {
            assert(false);
        }
        if (!(in_ >> point_.y)) {
            assert(false);
        }
    }
    using point_iterator = typename points::const_iterator;
    using sweepline_type = sweepline< point_iterator, value_type >;
    sweepline_type sweepline_{eps};
    {
        auto const pend = std::cend(points_);
        for (auto p = std::cbegin(points_); p != pend; ++p) {
            sweepline_.sites_.emplace_back(p);
        }
    }
    sweepline_();
    {
        value_type const vbox = value_type(3.0) * bbox;
        using site = typename sweepline_type::site;
        using edge = typename sweepline_type::edge;
        using vertex = typename sweepline_type::vertex;
        {
            gnuplot_ << "set size square;\n"
                        "set key left;\n";
            gnuplot_ << "set xrange [" << -vbox << ':' << vbox << "];\n";
            gnuplot_ << "set yrange [" << -vbox << ':' << vbox << "];\n";
        }
        {
            gnuplot_ << "plot";
            gnuplot_ << " '-' with points notitle"
                        ", '' with labels offset character 0, character 1 notitle";
            gnuplot_ << ", '' with lines title 'edges (" << sweepline_.edges_.size() <<  ")'";
            gnuplot_ << ";\n";
            {
                for (site const & site_ : sweepline_.sites_) {
                    auto const & point_ = *site_;
                    gnuplot_ << point_.x << ' ' << point_.y << '\n';
                }
                gnuplot_ << "e\n";
            }
            {
                point_iterator const & pbeg = std::cbegin(points_);
                for (site const & site_ : sweepline_.sites_) {
                    auto const & point_ = *site_;
                    gnuplot_ << point_.x << ' ' << point_.y << ' ' << std::distance(pbeg, site_) << '\n';
                }
                gnuplot_ << "e\n";
            }
            {
                for (edge const & edge_ : sweepline_.edges_) {
                    auto const & l = **edge_.l;
                    auto const & r = **edge_.r;
                    bool const beg = (edge_.b != sweepline_.vend);
                    bool const end = (edge_.e != sweepline_.vend);
                    if (beg != end) {
                        vertex const & p = *(beg ? edge_.b : edge_.e);
                        if (!(p.x < -vbox) && !(vbox < p.x) && !(p.y < -vbox) && !(vbox < p.y)) {
                            gnuplot_ << p.x << ' ' << p.y << '\n';
                            value_type dx = r.y - l.y;
                            value_type dy = l.x - r.x;
                            bool swap = false;
                            if (end) {
                                if (zero < dy) {
                                    if (l.y < p.y) {
                                        swap = true;
                                    }
                                } else if (dy < zero) {
                                    if (p.y < r.y) {
                                        swap = true;
                                    }
                                } else {
                                    swap = true;
                                }
                            }
                            if (swap) {
                                dx = -dx;
                                dy = -dy;
                            }
                            if (eps < dx) {
                                if (zero < dy) {
                                    value_type yy = p.y + (vbox - p.x) * dy / dx;
                                    if (vbox < yy) {
                                        value_type xx = p.x + (vbox - p.y) * dx / dy;
                                        gnuplot_ << xx << ' ' << vbox << '\n';
                                    } else {
                                        gnuplot_ << vbox << ' ' << yy << '\n';
                                    }
                                } else {
                                    value_type yy = p.y + (vbox - p.x) * dy / dx;
                                    if (yy < -vbox) {
                                        value_type xx = p.x - (vbox + p.y) * dx / dy;
                                        gnuplot_ << xx << ' ' << -vbox << '\n';
                                    } else {
                                        gnuplot_ << vbox << ' ' << yy << '\n';
                                    }
                                }
                            } else if (dx < -eps) {
                                if (zero < dy) {
                                    value_type yy = p.y - (vbox + p.x) * dy / dx;
                                    if (vbox < yy) {
                                        value_type xx = p.x + (vbox - p.y) * dx / dy;
                                        gnuplot_ << xx << ' ' << vbox << '\n';
                                    } else {
                                        gnuplot_ << -vbox << ' ' << yy << '\n';
                                    }
                                } else {
                                    value_type yy = p.y - (vbox + p.x) * dy / dx;
                                    if (yy < -vbox) {
                                        value_type xx = p.x - (vbox + p.y) * dx / dy;
                                        gnuplot_ << xx << ' ' << -vbox << '\n';
                                    } else {
                                        gnuplot_ << -vbox << ' ' << yy << '\n';
                                    }
                                }
                            } else {
                                if (zero < dy) {
                                    gnuplot_ << p.x << ' ' << +vbox << '\n';
                                } else {
                                    gnuplot_ << p.x << ' ' << -vbox << '\n';
                                }
                            }
                            gnuplot_ << "\n";
                        }
                    } else if (beg) {
                        assert(end);
                        vertex const & b = *edge_.b;
                        gnuplot_ << b.x << ' ' << b.y << '\n';
                        vertex const & e = *edge_.e;
                        gnuplot_ << e.x << ' ' << e.y << '\n';
                        gnuplot_ << "\n";
                    }
                }
                gnuplot_ << "e\n";
            }
        }
    }
    return EXIT_SUCCESS;
}
