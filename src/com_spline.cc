/**
 @file    com_spline.cc
 @author  Alexander W. Winkler (winklera@ethz.ch)
 @date    Aug 23, 2016
 @brief   Brief description
 */

#include <xpp/zmp/com_spline.h>

namespace xpp {
namespace zmp {

static int kDim2d = xpp::utils::kDim2d;

ComSpline::ComSpline ()
{
  // TODO Auto-generated constructor stub
}

ComSpline::~ComSpline ()
{
  // TODO Auto-generated destructor stub
}

void
ComSpline::Init (const PhaseInfoVec& phases)
{
  int id = 0;

  for (const auto& phase : phases) {


    if (phase.type_ == PhaseInfo::kStancePhase) {
      double t_reaction = 0.15;
      polynomials_.push_back(ComPolynomial(id++, t_reaction, phase));
      polynomials_.push_back(ComPolynomial(id++, phase.duration_-t_reaction, phase));
    }


    if (phase.type_ == PhaseInfo::kStepPhase) {
      int n_splines_per_step = 1;
      for (int i=0; i<n_splines_per_step; ++i) {
        ComPolynomial spline(id++, phase.duration_/n_splines_per_step, phase);
        spline.SetStep(phase.n_completed_steps_);
        polynomials_.push_back(spline);
      }
    }

  }

  phases_ = phases; // motion_ref remove this

  splines_initialized_ = true;
}

double
ComSpline::GetTotalTime(const VecPolynomials& splines)
{
  double T = 0.0;
  for (const ComPolynomial& s: splines)
    T += s.GetDuration();
  return T;
}

int
ComSpline::Index (int poly, Coords dim, SplineCoeff coeff) const
{
  return NumFreeCoeffPerSpline() * kDim2d * poly + NumFreeCoeffPerSpline() * dim + coeff;
}

int
ComSpline::GetTotalFreeCoeff () const
{
  return polynomials_.size() * NumFreeCoeffPerSpline() * kDim2d;
}

ComSpline::VectorXd
ComSpline::GetCoeffients () const
{
  using namespace xpp::utils::coords_wrapper;
  VectorXd x_abcd(GetTotalFreeCoeff());

  for (const auto& s : polynomials_)
    for (auto dim : { X, Y })
      for (auto coeff :  GetFreeCoeffPerSpline())
        x_abcd[Index(s.GetId(), dim, coeff)] = s.GetCoefficient(dim, coeff);

  return x_abcd;
}

int
ComSpline::GetPolynomialID(double t_global, const VecPolynomials& splines)
{
  double eps = 1e-10; // double imprecision
  assert(t_global<=GetTotalTime(splines)+eps); // machine precision

   double t = 0;
   for (const ComPolynomial& s: splines) {
     t += s.GetDuration();

     if (t >= t_global-eps) // at junctions, returns previous spline (=)
       return s.GetId();
   }
   assert(false); // this should never be reached
}

double
ComSpline::GetLocalTime(double t_global, const VecPolynomials& splines)
{
  int id_spline = GetPolynomialID(t_global,splines);

  double t_local = t_global;
  for (int id=0; id<id_spline; id++) {
    t_local -= splines.at(id).GetDuration();
  }

  return t_local;//-eps_; // just to never get value greater than true duration due to rounding errors
}

ComSpline::Point2d
ComSpline::GetCOM(double t_global, const VecPolynomials& splines)
{
  int id = GetPolynomialID(t_global,splines);
  double t_local = GetLocalTime(t_global, splines);

  return GetCOGxyAtPolynomial(id, t_local, splines);
}

ComSpline::Point2d
ComSpline::GetCOGxyAtPolynomial (int id, double t_local, const VecPolynomials& splines)
{
  Point2d cog_xy;
  cog_xy.p = splines[id].GetState(kPos, t_local);
  cog_xy.v = splines[id].GetState(kVel, t_local);
  cog_xy.a = splines[id].GetState(kAcc, t_local);
  cog_xy.j = splines[id].GetState(kJerk, t_local);

  return cog_xy;
}

ComSpline::JacobianRow
ComSpline::GetJacobian (double t_global, MotionDerivative posVelAcc,
                        Coords3D dim) const
{
  int id = GetPolynomialID(t_global);
  double t_local = GetLocalTime(t_global);

  return GetJacobianWrtCoeffAtPolynomial(posVelAcc, t_local, id, dim);
}

ComSpline::JacobianRow
ComSpline::GetJacobianWrtCoeffAtPolynomial (MotionDerivative posVelAcc, double t_local, int id,
                                            Coords3D dim) const
{
  assert(0 <= id && id <= polynomials_.back().GetId());

  JacobianRow jac(1, GetTotalFreeCoeff());

  switch (posVelAcc) {
    case kPos: GetJacobianPos (t_local, id, dim, jac); break;
    case kVel: GetJacobianVel (t_local, id, dim, jac); break;
    case kAcc: GetJacobianAcc (t_local, id, dim, jac); break;
    case kJerk:GetJacobianJerk(t_local, id, dim, jac); break;
  }

  return jac;
}

void
ComSpline::CheckIfSplinesInitialized() const
{
  if (!splines_initialized_) {
    throw std::runtime_error("ComSpline.splines_ not initialized. Call Init() first");
  }
}


} /* namespace zmp */
} /* namespace xpp */
