//-----------------------------------------------------------------------bl-
//--------------------------------------------------------------------------
//
// QUESO - a library to support the Quantification of Uncertainty
// for Estimation, Simulation and Optimization
//
// Copyright (C) 2008-2015 The PECOS Development Team
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the Version 2.1 GNU Lesser General
// Public License as published by the Free Software Foundation.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc. 51 Franklin Street, Fifth Floor,
// Boston, MA  02110-1301  USA
//
//-----------------------------------------------------------------------el-

#ifndef UQ_SCALEDCOV_TRANSITION_KERNEL_GROUP_H
#define UQ_SCALEDCOV_TRANSITION_KERNEL_GROUP_H

#include <queso/TKGroup.h>
#include <queso/VectorRV.h>
#include <queso/ScalarFunctionSynchronizer.h>

namespace QUESO {

//*****************************************************
// TK with scaled cov matrix
//*****************************************************
/*! \class ScaledCovMatrixTKGroup
 *  \brief This class allows the representation of a transition kernel with a scaled covariance matrix. */

template<class V, class M>
class ScaledCovMatrixTKGroup : public BaseTKGroup<V,M> {
public:
  //! @name Constructor/Destructor methods
  //@{
  //! Default constructor.
  ScaledCovMatrixTKGroup(const char*                    prefix,
                                const VectorSpace<V,M>& vectorSpace,
                                const std::vector<double>&     scales,
                                const M&                       covMatrix);
  //! Destructor.
  ~ScaledCovMatrixTKGroup();
  //@}

  //! @name Statistical/Mathematical methods
  //@{
  //! Whether or not the matrix is symmetric. Always 'true'.
  /*! \todo: It only returns 'true', thus a test for its symmetricity must be included.*/
  bool                          symmetric                 () const;

  //! Gaussian increment property to construct a transition kernel.
  const GaussianVectorRV<V,M>& rv                        (unsigned int                     stageId ) const;

  //! Gaussian increment property to construct a transition kernel.
  const GaussianVectorRV<V,M>& rv                        (const std::vector<unsigned int>& stageIds);

  //! Scales the covariance matrix.
  /*! The covariance matrix is scaled by a factor of \f$ 1/scales^2 \f$.*/
  void                          updateLawCovMatrix        (const M& covMatrix);
  //@}

  //! @name Misc methods
  //@{
  //! Sets the pre-computing positions \c m_preComputingPositions[stageId] with a new vector of size \c position.
  bool                          setPreComputingPosition   (const V& position, unsigned int stageId);

  //! Clears the pre-computing positions \c m_preComputingPositions[stageId]
  void                          clearPreComputingPositions();
  //@}

  //! @name I/O methods
  //@{
  //! TODO: Prints the transition kernel.
  /*! \todo: implement me!*/
  void                          print                     (std::ostream& os) const;
  //@}
private:
  //! Sets the mean of the RVs to zero.
  void                          setRVsWithZeroMean        ();
  using BaseTKGroup<V,M>::m_env;
  using BaseTKGroup<V,M>::m_prefix;
  using BaseTKGroup<V,M>::m_vectorSpace;
  using BaseTKGroup<V,M>::m_scales;
  using BaseTKGroup<V,M>::m_preComputingPositions;
  using BaseTKGroup<V,M>::m_rvs;

  M m_originalCovMatrix;
};

}  // End namespace QUESO

#endif // UQ_SCALEDCOV_TRANSITION_KERNEL_GROUP_H
