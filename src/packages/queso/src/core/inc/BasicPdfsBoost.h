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

#ifndef UQ_BASIC_PDFS_BOOST_H
#define UQ_BASIC_PDFS_BOOST_H

#include <queso/BasicPdfsBase.h>
#include <boost/random.hpp>
#include <boost/math/distributions.hpp>

namespace QUESO {

/*! \file BasicPdfsBoost.h
    \brief Class for Basic PDFs using Boost library.
*/

/*! \class BasicPdfsBoost
    \brief TODO: Base class for basic PDFs using Boost library.

    \todo This class \b will acommodate the definition of a Joint PDF using distributions
    available in the Boost library. It will ultimately be called by BaseJointPdf and/or its
    derived classes (via m_env.basicPdfs()) during the construction of Joint PDFs.
*/
class BasicPdfsBoost : public BasicPdfsBase
{
public:
  //! @name Constructor/Destructor methods
  //@{
  //! Default constructor.
  BasicPdfsBoost();

  //! Constructor.
  BasicPdfsBoost(int worldRank);

  //! Destructor.
 ~BasicPdfsBoost();
  //@}

  //! @name Mathematical methods
  //@{
  //! TODO: Actual value of the Beta PDF.
  double betaPdfActualValue (double x, double alpha, double beta) const;

  //! TODO: Actual value of the Gamma PDF.
  double gammaPdfActualValue(double x, double a,     double b   ) const;
  //@}

private:
};

}  // End namespace QUESO

#endif // UQ_BASIC_PDFS_BOOST_H
