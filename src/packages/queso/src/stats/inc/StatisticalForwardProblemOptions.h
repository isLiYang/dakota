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

#ifndef UQ_SFP_OPTIONS_H
#define UQ_SFP_OPTIONS_H

#include <queso/Environment.h>
//#include <queso/MonteCarloSGOptions.h>

#undef UQ_SFP_READS_SOLVER_OPTION

#define UQ_SFP_FILENAME_FOR_NO_FILE "."

// _ODV = option default value
#define UQ_SFP_COMPUTE_SOLUTION_ODV        1
#define UQ_SFP_COMPUTE_COVARIANCES_ODV     1
#define UQ_SFP_COMPUTE_CORRELATIONS_ODV    1
#define UQ_SFP_DATA_OUTPUT_FILE_NAME_ODV   UQ_SFP_FILENAME_FOR_NO_FILE
#define UQ_SFP_DATA_OUTPUT_ALLOWED_SET_ODV ""
#ifdef UQ_SFP_READS_SOLVER_OPTION
#define UQ_SFP_SOLVER_ODV                  "mc" // Monte Carlo
#endif

namespace QUESO {

/*! \file uqStatisticalForwardProblemOptions.h
    \brief Classes to allow options to be passed to a Statistical Forward Problem.
*/

/*! \class SfpOptionsValues
 *  \brief This class provides options for a Statistical Forward Problem if no input file is available.
 *
 * In order to solve a Statistical Forward Problem (SFP), QUESO expects some options for its methods to be
 * fully defined. This class provides default values for such options if no input file is available. */

class SfpOptionsValues
{
public:
  //! Constructor/Destructor methods
  //@{
  //! Default constructor.
  /*! Assigns the default suite of options to the Statistical Forward Problem.*/
  SfpOptionsValues            ();

  //! Copy constructor.
  /*! It assigns the same options values from  \c src to \c this.*/
  SfpOptionsValues            (const SfpOptionsValues& src);

  //! Destructor
  ~SfpOptionsValues            ();
  //@}

  //! @name Set methods
  //@{
  //! Assignment operator; it copies \c rhs to \c this.
  SfpOptionsValues& operator= (const SfpOptionsValues& rhs);
  //@}

  bool                   m_computeSolution;
  bool                   m_computeCovariances;
  bool                   m_computeCorrelations;
  std::string            m_dataOutputFileName;
  std::set<unsigned int> m_dataOutputAllowedSet;
#ifdef UQ_SFP_READS_SOLVER_OPTION
  std::string            m_solverString;
#endif

  //McOptionsValues m_mcOptionsValues;

private:
  //! Copies the option values from \c src to \c this.
  void copy(const SfpOptionsValues& src);
};

/*! \class StatisticalForwardProblemOptions
 *  \brief This class reads option values for a Statistical Forward Problem from an input file.
 *
 *  This class reads the option values for the Statistical Forward Problem (SFP) from an input file
 * provided by the user. The class expects the prefix '\<prefix\>_fp_'. For instance, if 'prefix'
 * is 'foo_775_', then the constructor will read all options that begin with 'foo_775_fp_'. If the
 * options request data to be written in the output file (MATLAB .m format only, for now), the user
 * can run 'grep zeros \<OUTPUT FILE NAME\>' after the solution procedure ends in order to check
 * which MATLAB variables are defined and set. The names of the variables are self explanatory.
*/

class StatisticalForwardProblemOptions
{
public:
  //! @name Constructor/Destructor methods
  //@{
  //! Constructor: reads options from the input file.
  StatisticalForwardProblemOptions(const BaseEnvironment& env, const char* prefix);

  //! Constructor: with alternative option values.
  /*! In this constructor, the input options are given by \c alternativeOptionsValues, rather than the
   * options input file*/
  StatisticalForwardProblemOptions(const BaseEnvironment& env, const char* prefix, const SfpOptionsValues& alternativeOptionsValues);

  //! Destructor
  ~StatisticalForwardProblemOptions();
  //@}

  //! @name I/O methods
  //@{
  //! It scans the option values from the options input file.
  void scanOptionsValues();

  //!  It prints the option values.
  void print            (std::ostream& os) const;
  //@}

  SfpOptionsValues       m_ov;
  std::string                   m_prefix;

private:
  //! Define my SFP options as the default options.
  void   defineMyOptions  (po::options_description& optionsDesc) const;

  //! Gets the option values of the SFP.
  void   getMyOptionValues(po::options_description& optionsDesc);

  const BaseEnvironment& m_env;

  po::options_description*      m_optionsDesc;
  std::string                   m_option_help;
  std::string                   m_option_computeSolution;
  std::string                   m_option_computeCovariances;
  std::string                   m_option_computeCorrelations;
  std::string                   m_option_dataOutputFileName;
  std::string                   m_option_dataOutputAllowedSet;
#ifdef UQ_SFP_READS_SOLVER_OPTION
  std::string                   m_option_solver;
#endif
};
//! Prints the object \c obj, overloading an operator.
std::ostream& operator<<(std::ostream& os, const StatisticalForwardProblemOptions& obj);

}  // End namespace QUESO

#endif // UQ_SFP_OPTIONS_H
