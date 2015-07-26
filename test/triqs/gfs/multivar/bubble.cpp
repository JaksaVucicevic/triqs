#include "../common.hpp"
using namespace triqs::clef;
using namespace triqs::lattice;

TEST(Gf, Bubble) { 
  int N = 4; // 16; // too long, 30 s
  int nw =3;

  placeholder<0> k_;
  placeholder<1> q_;
  placeholder<2> r_;
  placeholder<3> iw_;
  placeholder<4> inu_;

  double beta = 20, mu=0.;

  auto bz = brillouin_zone{bravais_lattice{{ {1, 0}, {0, 1} }}};

  auto chi0q = gf<cartesian_product<imfreq, imfreq, brillouin_zone>>{{{beta, Fermion, nw}, {beta, Boson, nw}, {bz, N}}, {1, 1}};
  auto chi0r = gf<cartesian_product<imfreq, imfreq, cyclic_lattice>>{{{beta, Fermion, nw}, {beta, Boson, nw}, {N,  N}}, {1, 1}};

  auto chi0q_from_r = chi0q;

  auto Gk = gf<cartesian_product<imfreq, brillouin_zone>,matrix_valued>{{{beta, Fermion, nw}, {bz, N}}, {1, 1}};
  auto Gr = gf<cartesian_product<imfreq, cyclic_lattice>,matrix_valued>{{{beta, Fermion, nw}, {N,  N}}, {1, 1}};

  auto eps_k_ = -2 * (cos(k_(0)) + cos(k_(1)));
  Gk(inu_, k_) << 1 / (inu_ + mu - eps_k_);

  auto Gmesh = std::get<1>(Gk.mesh());
  chi0q(inu_, iw_, q_) << sum(Gk(inu_, k_) * Gk(inu_ + iw_, k_ + q_), k_ = Gmesh) / Gmesh.size();

  curry<0>(Gr)[inu_] << inverse_fourier(curry<0>(Gk)[inu_]);

  chi0r(inu_, iw_, r_) << Gr(inu_, r_) * Gr(inu_ + iw_, -r_);

  curry<0,1>(chi0q_from_r)(inu_, iw_) << fourier(on_mesh(curry<0,1>(chi0r))(inu_, iw_));
  //curry<0,1>(chi0q_from_r)(inu_, iw_) << fourier(curry<0,1>(chi0r)(inu_, iw_));

  EXPECT_CLOSE_ARRAY(chi0q_from_r.data(),chi0q.data());

  //simplified, without frequency dependence
  auto gk = gf<brillouin_zone>{{bz, N} , {1,1}};
  auto gr = gf<cyclic_lattice>{{N,  N} , {1,1}};
  auto ggr = gr;
  auto ggq = gk;
  auto ggq_from_r = gk;

  gk(k_) << 1./ (M_PI/beta*1_j - eps_k_);
  ggq(q_) << sum(gk(k_) * gk(k_ + q_), k_=gk.mesh())/gk.mesh().size();
  
  gr() = inverse_fourier(gk);
  ggr(r_) << gr(r_)*gr(r_);
  ggq_from_r() = fourier(ggr);

  EXPECT_CLOSE_ARRAY(ggq_from_r.data() , ggq.data()); //<<"fourier pb";

  // hdf5
  rw_h5(chi0q);
  rw_h5(chi0r);
}
MAKE_MAIN;



