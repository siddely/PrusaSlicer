#ifndef slic3r_FillExpandedMetal_hpp_
#define slic3r_FillExpandedMetal_hpp_

#include "libslic3r/libslic3r.h"
#include "libslic3r/Fill/FillBase.hpp"
#include "libslic3r/ExPolygon.hpp"
#include "libslic3r/Point.hpp"
#include "libslic3r/Polyline.hpp"

namespace Slic3r {

// Expanded metal mesh.
//
// Diamond openings formed by zig-zag strands running along the long way of
// the diamond (LWD). Every vertex of a strand is a "bond": a short straight
// segment where it meets the neighbouring strand. The two strands are laid
// side by side there (one extrusion width apart), so each bond comes out as a
// double-width knot, like the uncut land in real expanded steel.
//
// The pattern is identical on every layer, so the openings go straight
// through the part. Infill density sets the diamond size.
// Fill angle 0 puts the LWD along Y; change it to rotate the mesh.
class FillExpandedMetal : public Fill
{
public:
    ~FillExpandedMetal() override = default;
    bool is_self_crossing() override { return false; }

    // Diamond shape. Edit to taste.
    static constexpr double aspect        = 1.5;  // LWD / SWD (long way / short way)
    static constexpr double bond_ratio    = 0.12; // bond length as a fraction of LWD
    static constexpr double min_swd_lines = 5.;   // smallest SWD, in extrusion widths

protected:
    Fill* clone() const override { return new FillExpandedMetal(*this); }

    void _fill_surface_single(
        const FillParams                &params,
        unsigned int                     thickness_layers,
        const std::pair<float, Point>   &direction,
        ExPolygon                        expolygon,
        Polylines                       &polylines_out) override;

    // Same orientation on every layer.
    float _layer_angle(size_t /* idx */) const override { return 0.f; }
};

} // namespace Slic3r

#endif // slic3r_FillExpandedMetal_hpp_
