/*******************************************************************************
 *
 * TRIQS: a Toolbox for Research in Interacting Quantum Systems
 *
 * Copyright (C) 2011-2013 by M. Ferrero, O. Parcollet
 *
 * TRIQS is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * TRIQS is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * TRIQS. If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/
#pragma once
#include <triqs/utility/first_include.hpp>
#include <math.h>
#include <triqs/utility/timer.hpp>
#include <triqs/utility/report_stream.hpp>
#include "./mc_measure_aux_set.hpp"
#include "./mc_measure_set.hpp"
#include "./mc_move_set.hpp"
#include "./mc_basic_step.hpp"
#include "./random_generator.hpp"

namespace triqs { namespace mc_tools {

 /**
  * \brief Generic Monte Carlo class.
  */
 template<typename MCSignType, typename MCStepType = Step::Metropolis<MCSignType> >
  class mc_generic {

   public:

    /**
     * Constructor from a set of parameters
     */
   mc_generic(uint64_t n_cycles, uint64_t length_cycle, uint64_t n_warmup_cycles, std::string random_name, int random_seed,
              int verbosity, std::function<bool()> AfterCycleDuty = std::function<bool()>())
      : RandomGenerator(random_name, random_seed)
      , AllMoves(RandomGenerator)
      , AllMeasures()
      , AllMeasures_on_proposed()
      , AllMeasuresAux()
      , report(&std::cout, verbosity)
      , Length_MC_Cycle(length_cycle)
      , NWarmIterations(n_warmup_cycles)
      , NCycles(n_cycles)
      , after_cycle_duty(AfterCycleDuty)
      , sign_av(0) {}

    /**
     * Register move M with its probability of being proposed.
     * NB : the proposition_probability needs to be >0 but does not need to be
     * normalized. Normalization is automatically done with all the added moves
     * before starting the run
     */
    template <typename MoveType>
     void add_move (MoveType && M, std::string name, double proposition_probability = 1.0) {
      static_assert( !std::is_pointer<MoveType>::value, "add_move in mc_generic takes ONLY values !");
      AllMoves.add(std::forward<MoveType>(M), name, proposition_probability);
     }

    /**
     * Register the Measure M
     */
    template<typename MeasureType>
     void add_measure (MeasureType && M, std::string name, bool on_proposed = false) {
      static_assert( !std::is_pointer<MeasureType>::value, "add_measure in mc_generic takes ONLY values !");
      if (on_proposed) AllMeasures_on_proposed.insert(std::forward<MeasureType>(M), name);
      else AllMeasures.insert(std::forward<MeasureType>(M), name);
     }

     /**
     * Register the precomputation 
     */
     template <typename MeasureAuxType> void add_measure_aux(std::shared_ptr<MeasureAuxType> p) { AllMeasuresAux.emplace_back(p); }

     /// get the average sign (to be called after collect_results)
    MCSignType average_sign() const { return sign_av; }

    /// get the current percents done
    uint64_t percent() const { return done_percent; }

    // An access to the random number generator
    random_generator & rng(){ return RandomGenerator;}

    /// Start the Monte Carlo
    bool start(MCSignType sign_init, std::function<bool ()> const & stop_callback) {
     if (!AllMeasures_on_proposed.empty() &&  (!AllMoves.has_reversibles()))
       TRIQS_RUNTIME_ERROR << "There are measures on proposed and some moves do not have the reversibles";
     assert(stop_callback);
     Timer.start();
     sign = sign_init; done_percent = 0; nmeasures = 0;
     auto & the_move = AllMoves;
     sum_sign = 0;
     bool stop_it=false, finished = false;
     uint64_t NCycles_tot = NCycles+ NWarmIterations;
     report << std::endl << std::flush;
     for (NC =0; !stop_it; ++NC) {
      // begin of cycle
      for (uint64_t k=1; (k<=Length_MC_Cycle-1); k++) { MCStepType::generic_step(the_move,RandomGenerator,sign); }
      // last step of the cycle
      if ((!thermalized()) || (AllMeasures_on_proposed.empty())) { // no need of this mess if no proposed measure
        MCStepType::generic_step(the_move,RandomGenerator,sign); // ordinary last step
      } else {
       double r = the_move.attempt();
       double accept_proba = std::min(1.0, r);
       bool will_accept = (RandomGenerator() < accept_proba);

       // we explore the branch that will be reverted but only if 0 < accept_proba < 1
       MCSignType sign_contrib =1;
       if ((accept_proba > 0.0) && (accept_proba < 1.0)) {
         if (will_accept) {
           the_move.reversible_reject(); 
           AllMeasures_on_proposed.accumulate(sign * sign_contrib * (1-accept_proba));
         }
         else {
           sign_contrib = the_move.reversible_accept();
           AllMeasures_on_proposed.accumulate(sign * sign_contrib * accept_proba);
         }
       }
       // then we revert the change and go into the accepted branch
       if (will_accept) {
         if (accept_proba < 1.0) the_move.revert_reject();
         sign *= the_move.accept();
         AllMeasures_on_proposed.accumulate(sign * accept_proba);
       } else {
         if (accept_proba > 0.0) the_move.revert_accept();
         the_move.reject();
         AllMeasures_on_proposed.accumulate(sign * (1-accept_proba));
       }
      } // end of cycle

      if (after_cycle_duty) {after_cycle_duty();}
      if (thermalized()) {
       nmeasures++;
       sum_sign += sign;
       for (auto &x : AllMeasuresAux) x();
       AllMeasures.accumulate(sign);
      }
      // recompute fraction done
      uint64_t dp = uint64_t(floor( ( NC*100.0) / (NCycles_tot-1)));
      if (dp>done_percent)  { done_percent=dp; report << done_percent; report<<"%; "; report <<std::flush; }
      finished = ( (NC >= NCycles_tot -1) || converged () );
      stop_it = (stop_callback() || finished);
     }
     report << std::endl << std::endl << std::flush;
     Timer.stop();
     return finished;

    }

    /// Reduce the results of the measures, and reports some statistics
    void collect_results(boost::mpi::communicator const & c) {

     uint64_t nmeasures_tot;
     MCSignType sum_sign_tot;
     boost::mpi::reduce(c, nmeasures, nmeasures_tot, std::plus<uint64_t>(), 0);
     boost::mpi::reduce(c, sum_sign, sum_sign_tot, std::plus<MCSignType>(), 0);

     report(3) << "[Node "<<c.rank()<<"] Acceptance rate for all moves:\n" << AllMoves.get_statistics(c);
     report(3) << "[Node "<<c.rank()<<"] Simulation lasted: " << double(Timer) << " seconds" << std::endl;
     report(3) << "[Node "<<c.rank()<<"] Number of measures: " << nmeasures  << std::endl;
     report(3) << "[Node "<<c.rank()<<"] Average sign: " << sum_sign / double(nmeasures) << std::endl << std::endl << std::flush;
  
     if (c.rank()==0) {
      sign_av = sum_sign_tot / double(nmeasures_tot);
      report(2) << "Total number of measures: " << nmeasures_tot << std::endl;
      report(2) << "Average sign: " << sign_av << std::endl << std::endl << std::flush;
     }
     boost::mpi::broadcast(c, sign_av, 0);
     AllMeasures.collect_results(c);
     AllMeasures_on_proposed.collect_results(c);

    }

    /// HDF5 interface
    friend void h5_write (h5::group g, std::string const & name, mc_generic const & mc){
     auto gr = g.create_group(name);
     h5_write(gr,"moves", mc.AllMoves);
     h5_write(gr,"measures", mc.AllMeasures);
     h5_write(gr,"measures", mc.AllMeasures_on_proposed);
     h5_write(gr,"length_monte_carlo_cycle", mc.Length_MC_Cycle);
     h5_write(gr,"number_cycle_requested", mc.NCycles);
     h5_write(gr,"number_warming_cycle_requested", mc.NWarmIterations);
     h5_write(gr,"number_cycle_done", mc.NC);
     h5_write(gr,"number_measure_done", mc.nmeasures);
     h5_write(gr,"sign", mc.sign);
     h5_write(gr,"sum_sign", mc.sum_sign);
    }

    /// HDF5 interface
    friend void h5_read (h5::group g, std::string const & name, mc_generic & mc){
     auto gr = g.open_group(name);
     h5_read(gr,"moves", mc.AllMoves);
     h5_read(gr,"measures", mc.AllMeasures);
     h5_read(gr,"measures", mc.AllMeasures_on_proposed);
     h5_read(gr,"length_monte_carlo_cycle", mc.Length_MC_Cycle);
     h5_read(gr,"number_cycle_requested", mc.NCycles);
     h5_read(gr,"number_warming_cycle_requested", mc.NWarmIterations);
     h5_read(gr,"number_cycle_done", mc.NC);
     h5_read(gr,"number_measure_done", mc.nmeasures);
     h5_read(gr,"sign", mc.sign);
     h5_read(gr,"sum_sign", mc.sum_sign);
    }

   private:
    random_generator RandomGenerator;
    move_set<MCSignType> AllMoves;
    measure_set<MCSignType> AllMeasures;
    measure_set<MCSignType> AllMeasures_on_proposed;
    std::vector<measure_aux> AllMeasuresAux;
    utility::report_stream report;
    uint64_t Length_MC_Cycle;/// Length of one Monte-Carlo cycle between 2 measures
    uint64_t NWarmIterations, NCycles;
    uint64_t nmeasures;
    MCSignType sum_sign;
    utility::timer Timer;
    std::function<bool()> after_cycle_duty;
    MCSignType sign, sign_av;
    uint64_t NC,done_percent;// NC = number of the cycle

    bool thermalized() const { return (NC>= NWarmIterations);}
    bool converged() const { return false;}
  };

}}// end namespace

