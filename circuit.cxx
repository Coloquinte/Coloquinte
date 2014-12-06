#include "gp/circuit.hxx"

namespace coloquinte{
namespace gp{

void add_force(pin_1D const p1, pin_1D const p2, linear_system & L, float_t force){
    if(p1.movable && p2.movable){
        L.add_force(
            force,
            p1.cell_ind, p2.cell_ind,
            p1.offs,     p2.offs
        );
    }
    else if(p1.movable){
        L.add_fixed_force(
            force,
            p1.cell_ind,
            p2.pos,
            p1.offs
        );
    }
    else if(p2.movable){
        L.add_fixed_force(
            force,
            p2.cell_ind,
            p1.pos,
            p2.offs
        );
    }
}

void add_force(pin_1D const p1, pin_1D const p2, linear_system & L, float_t tol, float_t scale){
    add_force(p1, p2, L, scale/std::max(tol, std::abs(p2.pos-p1.pos)));
}

point<linear_system> empty_linear_systems(netlist const & circuit, placement_t const & pl){
    point<linear_system> ret = point<linear_system>(linear_system(circuit.cell_cnt()), linear_system(circuit.cell_cnt()));

    for(index_t i=0; i<circuit.cell_cnt(); ++i){
        if( (XMovable & circuit.get_cell(i).attributes) == 0){
            ret.x_.add_triplet(i, i, 1.0);
            ret.x_.add_doublet(i, pl.positions_[i].x_);
        }
        if( (YMovable & circuit.get_cell(i).attributes) == 0){
            ret.y_.add_triplet(i, i, 1.0);
            ret.y_.add_doublet(i, pl.positions_[i].y_);
        }
    }

    return ret;
}

std::vector<pin_2D>         get_pins_2D(netlist const & circuit, placement_t const & pl, index_t net_ind){
    std::vector<pin_2D> ret;
    for(auto p : circuit.get_net(net_ind)){
        point<float_t> offs = static_cast<point<float_t> >(p.offset) * pl.orientations_[p.cell_ind];
        point<float_t> pos  = static_cast<point<float_t> >(offs)     + pl.positions_[p.cell_ind];

        bool movable = (circuit.get_cell(p.cell_ind).attributes & (XMovable|YMovable)) != 0;
        ret.push_back(pin_2D(p.cell_ind, pos, offs, movable));
    }
    return ret;
}

point<std::vector<pin_1D> > get_pins_1D(netlist const & circuit, placement_t const & pl, index_t net_ind){
    point<std::vector<pin_1D> > ret;
    for(auto p : circuit.get_net(net_ind)){
        point<float_t> offs = static_cast<point<float_t> >(p.offset) * pl.orientations_[p.cell_ind];
        point<float_t> pos  = static_cast<point<float_t> >(offs)     + pl.positions_[p.cell_ind];

        bool x_movable = (circuit.get_cell(p.cell_ind).attributes & XMovable) != 0;
        bool y_movable = (circuit.get_cell(p.cell_ind).attributes & YMovable) != 0;
        ret.x_.push_back(pin_1D(p.cell_ind, pos.x_, offs.x_, x_movable));
        ret.y_.push_back(pin_1D(p.cell_ind, pos.y_, offs.y_, y_movable));
    }
    return ret;
}

void get_HPWLF(std::vector<pin_1D> const & pins, linear_system & L, float_t tol){
    if(pins.size() >= 2){
        auto min_elt = std::min_element(pins.begin(), pins.end()), max_elt = std::max_element(pins.begin(), pins.end());

        for(auto it = pins.begin(); it != pins.end(); ++it){
            // Just comparing the iterator is poorer due to redundancies in the benchmarks!
            if(it != min_elt){
                add_force(*it, *min_elt, L, tol, 1.0/(pins.size()-1));
                if(it != max_elt){ // Hopefully only one connexion between the min and max pins
                    add_force(*it, *max_elt, L, tol, 1.0/(pins.size()-1));
                }
            }
        }
    }
}

float_t get_HPWL_wirelength(netlist const & circuit, placement_t const & pl){
    float_t sum = 0.0;
    for(index_t i=0; i<circuit.net_cnt(); ++i){
        auto pins = get_pins_1D(circuit, pl, i);
        auto minmaxX = std::minmax_element(pins.x_.begin(), pins.x_.end()), minmaxY = std::minmax_element(pins.y_.begin(), pins.y_.end());
        sum += ((minmaxX.second->pos - minmaxX.first->pos) + (minmaxY.second->pos - minmaxY.first->pos));
    }
    return sum;
}

point<linear_system> get_HPWLF_linear_system (netlist const & circuit, placement_t const & pl, float_t tol, index_t min_s, index_t max_s){
    point<linear_system> L = empty_linear_systems(circuit, pl);
    for(index_t i=0; i<circuit.net_cnt(); ++i){
        // Has the net the right pin count?
        index_t pin_cnt = circuit.get_net(i).pin_cnt;
        if(pin_cnt < min_s or pin_cnt >= max_s) continue;

        auto pins = get_pins_1D(circuit, pl, i);
        get_HPWLF(pins.x_, L.x_, tol);
        get_HPWLF(pins.y_, L.y_, tol);
    }
    return L;
}

void get_HPWLR(std::vector<pin_1D> const & pins, linear_system & L, float_t tol){
    std::vector<pin_1D> sorted_pins = pins;
    std::sort(sorted_pins.begin(), sorted_pins.end());
    for(index_t i=0; i+1<sorted_pins.size(); ++i){
        add_force(sorted_pins[i], sorted_pins[i+1], L, tol, 1.0);
    }
}

point<linear_system> get_HPWLR_linear_system (netlist const & circuit, placement_t const & pl, float_t tol, index_t min_s, index_t max_s){
    point<linear_system> L = empty_linear_systems(circuit, pl);
    for(index_t i=0; i<circuit.net_cnt(); ++i){
        // Has the net the right pin count?
        index_t pin_cnt = circuit.get_net(i).pin_cnt;
        if(pin_cnt < min_s or pin_cnt >= max_s) continue;

        auto pins = get_pins_1D(circuit, pl, i);
        get_HPWLR(pins.x_, L.x_, tol);
        get_HPWLR(pins.y_, L.y_, tol);
    }
    return L;
}

void get_star(std::vector<pin_1D> const & pins, linear_system & L, float_t tol, index_t star_index){
    // The net is empty, but we still populate the diagonal to avoid divide by zeros
    if(pins.size() < 2){
        L.add_triplet(star_index, star_index, 1.0);
        return;
    }

    auto minmax = std::minmax_element(pins.begin(), pins.end());

    // Get the median position; create the pin
    float_t med_pos = 0.5* (minmax.second->pos + minmax.first->pos);
    float_t half_width = 0.5 * (minmax.second->pos - minmax.first->pos);
    
    for(pin_1D p : pins){
        // We want a force of ~1.0 when at the same distance as the extremum
        // And a force of about 0.0 otherwise
        // We add a small region of width boundary_tol where this transition takes place, which directly impacts the flexibility of the procedure
        pin_1D star_pin = pin_1D(star_index, med_pos, 0.0, true);
        add_force(p, star_pin, L, 1.0/(std::max(tol, half_width)));
    }
}

point<linear_system> get_star_linear_system  (netlist const & circuit, placement_t const & pl, float_t tol, index_t min_s, index_t max_s){
    point<linear_system> L = empty_linear_systems(circuit, pl);
    L.x_.add_variables(circuit.net_cnt());
    L.y_.add_variables(circuit.net_cnt());
    for(index_t i=0; i<circuit.net_cnt(); ++i){
        // Has the net the right pin count?
        index_t pin_cnt = circuit.get_net(i).pin_cnt;
        if(pin_cnt < min_s or pin_cnt >= max_s){
            // Put a one in the intermediate variable in order to avoid non-invertible matrices
            L.x_.add_triplet(i+circuit.cell_cnt(), i+circuit.cell_cnt(), 1.0);
            L.y_.add_triplet(i+circuit.cell_cnt(), i+circuit.cell_cnt(), 1.0);
            continue;
        }

        auto pins = get_pins_1D(circuit, pl, i);
        // Provide the index of the star's central pin in the linear system
        get_star(pins.x_, L.x_, tol, i+circuit.cell_cnt());
        get_star(pins.y_, L.y_, tol, i+circuit.cell_cnt());
    }
    return L;
}

void get_result(netlist const & circuit, placement_t & pl, point<linear_system> & L, float_t tol){
    std::vector<float_t> x_sol, y_sol;
    std::vector<float_t> x_guess(pl.cell_cnt()), y_guess(pl.cell_cnt());
    
    assert(L.x_.internal_size() == x_guess.size());
    assert(L.y_.internal_size() == y_guess.size());

    for(index_t i=0; i<pl.cell_cnt(); ++i){
        x_guess[i] = pl.positions_[i].x_;
        y_guess[i] = pl.positions_[i].y_;
    }
    #pragma omp parallel sections num_threads(2)
    {
    #pragma omp section
    x_sol = L.x_.solve_CG(x_guess, tol);
    #pragma omp section
    y_sol = L.y_.solve_CG(y_guess, tol);
    }
    for(index_t i=0; i<pl.cell_cnt(); ++i){
        if( (circuit.get_cell(i).attributes & XMovable) != 0){
            pl.positions_[i].x_ = x_sol[i];
        }
        if( (circuit.get_cell(i).attributes & YMovable) != 0){
            pl.positions_[i].y_ = y_sol[i];
        }
    }
}

// Intended to be used by pulling forces to adapt the forces to the cell's areas
std::vector<float_t> get_area_scales(netlist const & circuit){
    std::vector<float_t> ret(circuit.cell_cnt());
    capacity_t int_tot_area = 0;
    for(index_t i=0; i<circuit.cell_cnt(); ++i){
        capacity_t A = circuit.get_cell(i).area;
        ret[i] = static_cast<float_t>(A);
        int_tot_area += A;
    }
    float_t average_area = static_cast<float_t>(int_tot_area) / circuit.cell_cnt();
    for(index_t i=0; i<circuit.cell_cnt(); ++i){
        ret[i] /= average_area;
    }
    return ret;
}

point<linear_system> get_pulling_forces (netlist const & circuit, placement_t const & pl, float_t typical_distance){
    point<linear_system> L = empty_linear_systems(circuit, pl);
    float_t typical_force = 1.0 / typical_distance;
    for(index_t i=0; i<pl.cell_cnt(); ++i){
        L.x_.add_anchor(
            typical_force,
            i, pl.positions_[i].x_
        );
        L.y_.add_anchor(
            typical_force,
            i, pl.positions_[i].y_
        );
    }
    
    return L;
}

point<linear_system> get_linear_pulling_forces (netlist const & circuit, placement_t const & UB_pl, placement_t const & LB_pl, float_t force, float_t min_distance){
    point<linear_system> L = empty_linear_systems(circuit, UB_pl);
    assert(LB_pl.cell_cnt() == UB_pl.cell_cnt());
    std::vector<float_t> scaling = get_area_scales(circuit);
    for(index_t i=0; i<LB_pl.cell_cnt(); ++i){
        L.x_.add_anchor(
            force * scaling[i] / (std::max(std::abs(UB_pl.positions_[i].x_ - LB_pl.positions_[i].x_), min_distance)),
            i, UB_pl.positions_[i].x_
        );
        L.y_.add_anchor(
            force * scaling[i] / (std::max(std::abs(UB_pl.positions_[i].y_ - LB_pl.positions_[i].y_), min_distance)),
            i, UB_pl.positions_[i].y_
        );
    }
    

    return L;
}

region_distribution get_rough_legalizer(netlist const & circuit, placement_t const & pl, box<int_t> surface){
    std::vector<region_distribution::movable_cell> movable_cells;
    std::vector<region_distribution::fixed_cell>   fixed_cells;

    for(index_t i=0; i<circuit.cell_cnt(); ++i){
        auto C = circuit.get_cell(i);
        if((C.attributes & (XMovable|YMovable)) != 0){
            movable_cells.push_back(region_distribution::movable_cell(C.area, pl.positions_[i], i));
        }
        else{
            fixed_cells.push_back(region_distribution::fixed_cell(C.size, pl.positions_[i]));
        }
    }

    return region_distribution(surface, movable_cells, fixed_cells);
}

void get_result(netlist const & circuit, placement_t & pl, region_distribution const & legalizer){
    auto exportation = legalizer.export_spread_positions();
    for(auto const C : exportation){
        pl.positions_[C.index_in_placement_] = C.pos_;
    }
}

float_t get_mean_linear_disruption(netlist const & circuit, placement_t const & LB_pl, placement_t const & UB_pl){
    float_t tot_cost = 0.0;
    float_t tot_area = 0.0;
    for(index_t i=0; i<circuit.cell_cnt(); ++i){
        float_t area = static_cast<float_t>(circuit.get_cell(i).area);
        point<float_t> diff = LB_pl.positions_[i] - UB_pl.positions_[i];
        tot_cost += area * (std::abs(diff.x_) + std::abs(diff.y_));
        tot_area += area;
    }
    return tot_cost / tot_area;
}

float_t get_mean_quadratic_disruption(netlist const & circuit, placement_t const & LB_pl, placement_t const & UB_pl){
    float_t tot_cost = 0.0;
    float_t tot_area = 0.0;
    for(index_t i=0; i<circuit.cell_cnt(); ++i){
        float_t area = static_cast<float_t>(circuit.get_cell(i).area);
        point<float_t> diff = LB_pl.positions_[i] - UB_pl.positions_[i];
        float_t manhattan = (std::abs(diff.x_) + std::abs(diff.y_));
        tot_cost += area * manhattan * manhattan;
        tot_area += area;
    }
    return std::sqrt(tot_cost / tot_area);
}

} // namespace gp
} // namespace coloquinte


