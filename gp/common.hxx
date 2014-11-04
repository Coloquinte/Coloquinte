
#ifndef COLOQUINTE_GP_COMMON
#define COLOQUINTE_GP_COMMON

#include <cstdint>
#include <algorithm>

namespace coloquinte{

using float_t    = float;
using int_t      = std::int32_t;
using index_t    = std::uint32_t;
using capacity_t = std::int64_t;
using mask_t     = std::uint32_t;

using ext_object = std::uint64_t;

enum PlacementType{
    Optimist  = 0,
    Pessimist = 1
};

enum Movability{
    XMovable   = 1     ,
    YMovable   = 1 << 1,
    XFlippable = 1 << 2,
    YFlippable = 1 << 3,
    SoftMacro  = 1 << 4
};

template<typename T>
struct point{
    T x_, y_;
    point(){}
    point(T x, T y): x_(x), y_(y){}

    template<typename S>
    point<S> cast() const{
        return point<S>(static_cast<S>(x_), static_cast<S>(y_));
    }
    template<typename S>
    operator point<S>() const{
        return cast<S>();
    }
};

template<typename T>
point<T> operator+(point<T> const a, point<T> const b){
    return point<T>(a.x_+b.x_, a.y_+b.y_);
}
template<typename T>
point<T> operator-(point<T> const a, point<T> const b){
    return point<T>(a.x_-b.x_, a.y_-b.y_);
}
template<typename T>
point<T> operator*(T lambda, point<T> const p){
    return point<T>(lambda * p.x_, lambda * p.y_);
}
template<typename T>
point<T> operator*(point<T> const a, point<T> const b){
    return point<T>(a.x_*b.x_, a.y_*b.y_);
}

template<typename T>
struct box{
    T x_min_, y_min_, x_max_, y_max_;
    box(){}
    box(T x_mn, T x_mx, T y_mn, T y_mx) : x_min_(x_mn), x_max_(x_mx), y_min_(y_mn), y_max_(y_mx){}

    bool intersects(box<T> const o) const{
        return x_min_   < o.x_max_
            && y_min_   < o.y_max_
            && o.x_min_ < x_max_
            && o.y_min_ < y_max_;
    }
    box<T> intersection(box<T> const o) const{
        return box<T>(
            std::max(x_min_, o.x_min_),
            std::min(x_max_, o.x_max_),
            std::max(y_min_, o.y_min_),
            std::min(y_max_, o.y_max_)
        );
    }
};

using orientation_t = point<bool>;

} // Namespace coloquinte

#endif

