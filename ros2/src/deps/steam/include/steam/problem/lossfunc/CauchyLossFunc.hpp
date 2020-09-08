//////////////////////////////////////////////////////////////////////////////////////////////
/// \file CauchyLossFunc.hpp
///
/// \author Sean Anderson, ASRL
//////////////////////////////////////////////////////////////////////////////////////////////

#ifndef STEAM_CAUCHY_LOSS_FUNCTION_HPP
#define STEAM_CAUCHY_LOSS_FUNCTION_HPP

#include <Eigen/Core>
#include <boost/shared_ptr.hpp>

#include <steam/problem/lossfunc/LossFunctionBase.hpp>

namespace steam {

//////////////////////////////////////////////////////////////////////////////////////////////
/// \brief Cauchy loss function class
//////////////////////////////////////////////////////////////////////////////////////////////
class CauchyLossFunc : public LossFunctionBase
{
 public:

  /// Convenience typedefs
  typedef boost::shared_ptr<CauchyLossFunc> Ptr;
  typedef boost::shared_ptr<const CauchyLossFunc> ConstPtr;

  //////////////////////////////////////////////////////////////////////////////////////////////
  /// \brief Constructor -- k is the `threshold' based on number of std devs (1-3 is typical)
  //////////////////////////////////////////////////////////////////////////////////////////////
  CauchyLossFunc(double k) : k_(k) {}

  //////////////////////////////////////////////////////////////////////////////////////////////
  /// \brief Cost function (basic evaluation of the loss function)
  //////////////////////////////////////////////////////////////////////////////////////////////
  virtual double cost(double whitened_error_norm) const;

  //////////////////////////////////////////////////////////////////////////////////////////////
  /// \brief Weight for iteratively reweighted least-squares (influence function div. by error)
  //////////////////////////////////////////////////////////////////////////////////////////////
  virtual double weight(double whitened_error_norm) const;

 private:

  //////////////////////////////////////////////////////////////////////////////////////////////
  /// \brief Cauchy constant
  //////////////////////////////////////////////////////////////////////////////////////////////
  double k_;
};

} // steam

#endif // STEAM_CAUCHY_LOSS_FUNCTION_HPP
