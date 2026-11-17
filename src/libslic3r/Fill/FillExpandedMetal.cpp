#include <algorithm>
#include <cmath>

#include "../ClipperUtils.hpp"
#include "../ShortestPath.hpp"
#include "FillExpandedMetal.hpp"
#include "libslic3r/BoundingBox.hpp"
#include "libslic3r/Polygon.hpp"

namespace Slic3r {

void FillExpandedMetal::_fill_surface_single(
    const FillParams                &params,
    unsigned int                     /* thickness_layers */,
    const std::pair<float, Point>   &direction,
    ExPolygon                        expolygon,
    Polylines                       &polylines_out)
{
    // ---- Cell size from density -------------------------------------------
    // One strand per half SWD, each of length ~ SWD * sqrt(1 + aspect^2) per LWD.
    // Covered fraction ~ 2 * s * sqrt(1 + aspect^2) / (aspect * SWD), solved for SWD.
    const double s       = this->spacing; // extrusion spacing, mm
    const double density = std::clamp(double(params.density), 0.01, 1.0);
    double swd = 2. * s * std::sqrt(1. + aspect * aspect) / (aspect * density);
    swd = std::max(swd, min_swd_lines * s);
    const double lwd  = aspect * swd;
    const double bond = std::max(s, bond_ratio * lwd);

    const coord_t pitch_x = coord_t(scale_(0.5 * swd)); // distance between strand centre lines
    const coord_t period  = coord_t(scale_(lwd));       // one full zig-zag
    const coord_t half_y  = period / 2;
    const coord_t hb      = coord_t(scale_(0.5 * bond));
    const coord_t hs      = coord_t(scale_(0.5 * s));

    // ---- Work area in the rotated frame, snapped to a world grid ------------
    // Snapping keeps the diamonds stacked exactly from layer to layer.
    const Point  centre = direction.second;
    BoundingBox  bb     = get_extents(expolygon.contour);
    {
        Polygon bb_polygon = bb.polygon();
        bb_polygon.rotate(direction.first, centre);
        bb = get_extents(bb_polygon);
        bb.merge(align_to_grid(bb.min, Point(2 * pitch_x, period)));
    }
    // One extra strand and one extra zig-zag on every side, trimmed by the clip below.
    const coord_t x_start = bb.min.x() - 2 * pitch_x;
    const coord_t y_start = bb.min.y() - period;
    const int     n_strands = int((bb.max.x() - x_start) / pitch_x) + 3;
    const int     n_nodes   = int((bb.max.y() - y_start) / half_y) + 3;

    // ---- Strands -------------------------------------------------------------
    // Strand k lies between lines x = k*P and x = (k+1)*P. At height m*LWD/2 its
    // vertex sits on the left line when (k + m) is odd, on the right line otherwise.
    // Pulled in by half an extrusion width so the neighbour fits alongside it.
    Polylines all_polylines;
    all_polylines.reserve(n_strands);
    for (int k = 0; k < n_strands; ++ k) {
        const coord_t x_left  = x_start + coord_t(k) * pitch_x + hs;
        const coord_t x_right = x_start + coord_t(k + 1) * pitch_x - hs;
        Polyline p;
        p.points.reserve(2 * n_nodes);
        for (int m = 0; m < n_nodes; ++ m) {
            const coord_t y = y_start + coord_t(m) * half_y;
            const coord_t x = ((k + m) & 1) ? x_left : x_right;
            p.points.emplace_back(x, y - hb); // bond: short straight run
            p.points.emplace_back(x, y + hb);
        }
        // Alternate direction so the unconnected case still snakes nicely.
        if (k & 1)
            p.reverse();
        p.rotate(-direction.first, centre);
        all_polylines.emplace_back(std::move(p));
    }

    // ---- Clip to the region and link ends along the boundary ----------------
    all_polylines = intersection_pl(std::move(all_polylines), expolygon);
    if (params.dont_connect() || all_polylines.size() <= 1)
        append(polylines_out, chain_polylines(std::move(all_polylines)));
    else
        connect_infill(std::move(all_polylines), expolygon, polylines_out, this->spacing, params);
}

} // namespace Slic3r
