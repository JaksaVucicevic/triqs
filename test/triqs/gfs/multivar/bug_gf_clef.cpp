#include "../common.hpp"
using namespace triqs::clef;
using namespace triqs::lattice;

TEST(Gf, Bubble) { 

  placeholder<0> i_;
  placeholder<1> j_;
  placeholder<2> r_;
  placeholder<3> om_;
  placeholder<4> nu_;

auto g = gf<imfreq, matrix_valued, no_tail>{{1.0,Fermion},{1,1}};
g(om_)(i_,j_) << 0.0;


auto g2 = gf<cartesian_product<imfreq,imfreq>, matrix_valued, no_tail>{{{1.0,Fermion},{1.0,Fermion}},{1,1}};
g2(om_,nu_)(i_,j_) << 0.0;

}
MAKE_MAIN;



