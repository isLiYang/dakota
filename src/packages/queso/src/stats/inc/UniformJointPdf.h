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

#ifndef UQ_UNIFORM_JOINT_PROB_DENSITY_H
#define UQ_UNIFORM_JOINT_PROB_DENSITY_H

#include <cmath>

#include <boost/math/special_functions.hpp> // for Boost isnan. Note parentheses are important in function call.

#include <queso/JointPdf.h>
#include <queso/Environment.h>
#include <queso/ScalarFunction.h>
#include <queso/BoxSubset.h>

namespace QUESO {

//*****************************************************
// Uniform probability density class [PDF-04]
//*****************************************************
/*!
 * \class UniformJointPdf
 * \brief A class for handling uniform joint PDFs.
 *
 * This class allows the mathematical definition of a Uniform Joint PDF.*/

template<class V, class M>
class UniformJointPdf : public BaseJointPdf<V,M> {
public:
  //! @name Constructor/Destructor methods
  //@{
  //! Constructor
  /*! Constructs a new object of the class, given a prefix and the domain set of the uniform PDF.  */
  UniformJointPdf(const char*                  prefix,
                         const VectorSet<V,M>& domainSet);
  //! Destructor
 ~UniformJointPdf();
 //@}

   //! @name Math methods
  //@{
  //! Actual value of the uniform PDF.
  /*! If the domain of the PDF is well defined (neither negative nor infinite), then the actual
   * value is given by 1.0/(the volume of the domain), otherwise the actual value is 1.*/
  double actualValue(const V& domainVector, const V* domainDirection, V* gradVector, M* hessianMatrix, V* hessianEffect) const;

  //! Logarithm of the value of the uniform PDF.
  /*! Analogous to the actualValue routine, except that the logarithm of the calculated value is
   * returned. */
  double lnValue    (const V& domainVector, const V* domainDirection, V* gradVector, M* hessianMatrix, V* hessianEffect) const;

  //! Computes the logarithm of the normalization factor.
  /*! This routine calls BaseJointPdf::commonComputeLogOfNormalizationFactor().*/
  double computeLogOfNormalizationFactor(unsigned int numSamples, bool updateFactorInternally) const;
  //@}
protected:
  using BaseScalarFunction<V,M>::m_env;
  using BaseScalarFunction<V,M>::m_prefix;
  using BaseScalarFunction<V,M>::m_domainSet;
  using BaseJointPdf<V,M>::m_normalizationStyle;
  using BaseJointPdf<V,M>::m_logOfNormalizationFactor;
};

}  // End namespace QUESO

#endif // UQ_UNIFORM_JOINT_PROB_DENSITY_H
