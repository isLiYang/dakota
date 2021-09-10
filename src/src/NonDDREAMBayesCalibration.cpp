/*  _______________________________________________________________________

    DAKOTA: Design Analysis Kit for Optimization and Terascale Applications
    Copyright 2014 Sandia Corporation.
    This software is distributed under the GNU Lesser General Public License.
    For more information, see the README file in the top Dakota directory.
    _______________________________________________________________________ */

//- Class:	 NonDDREAMBayesCalibration
//- Description: Derived class for Bayesian inference using DREAM
//- Owner:       Brian Adams
//- Checked by:
//- Version:

#include "NonDDREAMBayesCalibration.hpp"
#include "ProblemDescDB.hpp"
#include "DakotaModel.hpp"
#include "ProbabilityTransformation.hpp"

// BMA TODO: remove this header
// for uniform PDF and samples
//#include "pdflib.hpp"

// included to set seed in RNGLIB:
#include "rnglib.hpp"

// for core dream functionality
using std::string;
#include "dream.hpp"

static const char rcsId[]="@(#) $Id$";

// five forwards to the class static functions
// BMA TODO: change interface to pass function pointers and
// initializers and remove these globals

namespace dream {

/// forwarder to problem_size needed by DREAM
void 
problem_size (int &chain_num, int &cr_num, int &gen_num, int &pair_num, 
	      int &par_num)
{
  Dakota::NonDDREAMBayesCalibration::
    problem_size(chain_num, cr_num, gen_num, pair_num, par_num);
}

/// forwarder to problem_value needed by DREAM
void 
problem_value (std::string *chain_filename, std::string *gr_filename,
	       double &gr_threshold, int &jumpstep, double limits[], 
	       int par_num, int &printstep, std::string *restart_read_filename,
	       std::string *restart_write_filename)
{
  Dakota::NonDDREAMBayesCalibration::
    problem_value(chain_filename, gr_filename, gr_threshold, jumpstep, limits, 
		  par_num, printstep, restart_read_filename, 
		  restart_write_filename);
}

/// forwarder to prior_density needed by DREAM
double prior_density (int par_num, double zp[]) 
{
  return Dakota::NonDDREAMBayesCalibration::
    prior_density (par_num, zp); 
}

/// forwarder to prior_sample needed by DREAM
double* prior_sample (int par_num)
{
  return Dakota::NonDDREAMBayesCalibration::
    prior_sample(par_num);
}

/// forwarder to sample_likelihood needed by DREAM
double sample_likelihood (int par_num, double zp[])
{
  return Dakota::NonDDREAMBayesCalibration::
    sample_likelihood(par_num, zp);
}


} // namespace dream


namespace Dakota {

//initialization of statics
NonDDREAMBayesCalibration* NonDDREAMBayesCalibration::NonDDREAMInstance(NULL);

/** This constructor is called for a standard letter-envelope iterator 
    instantiation.  In this case, set_db_list_nodes has been called and 
    probDescDB can be queried for settings from the method specification. */
NonDDREAMBayesCalibration::
NonDDREAMBayesCalibration(ProblemDescDB& problem_db, Model& model):
  NonDBayesCalibration(problem_db, model),
  likelihoodScale(probDescDB.get_real("method.likelihood_scale")),
  calibrateSigma(probDescDB.get_bool("method.nond.calibrate_sigma")),
  numChains(probDescDB.get_int("method.dream.num_chains")),
  numCR(probDescDB.get_int("method.dream.num_cr")),
  crossoverChainPairs(probDescDB.get_int("method.dream.crossover_chain_pairs")),
  grThreshold(probDescDB.get_real("method.dream.gr_threshold")),
  jumpStep(probDescDB.get_int("method.dream.jump_step"))
{ 
  // don't use max_function_evaluations, since we have num_samples
  // consider max_iterations = generations, and adjust as needed?

  Cout << "INFO (DREAM): requested samples = " << numSamples
       << "\nINFO (DREAM): requested chains = " << numChains << std::endl;

  if (numChains < 3) {
    numChains = 3;
    Cout << "WARN (DREAM): Increasing requested chains to minimum (3)"
	 << std::endl;
  }

  numGenerations = std::floor((Real)numSamples/numChains);
  if (numGenerations < 2) {
    numGenerations = 2;
    numSamples = numGenerations * numChains;
    Cout << "WARN (DREAM): generations = samples / chains = " << numGenerations 
	 << " is less than 2.\n             setting generations = 2, for "
	 << numSamples << " total samples." << std::endl;
  }
  else {
    numSamples = numGenerations * numChains;
    Cout << "INFO (DREAM): will use " << numChains << " chains with "
	 << numGenerations << " generations,\nfor " << numSamples 
	 << " total samples." << std::endl;
  }

  if (numCR < 1) {
    numCR = 1;
    Cout << "WARN (DREAM): num_cr < 1, resetting to 3 (default)." << std::endl;
  }

  if (crossoverChainPairs < 0) {
    numCR = 3;
    Cout << "WARN (DREAM): crossover_chain_pairs < 0, resetting to 3 (default)."
	 << std::endl;
  }

  if (grThreshold < 0.0) {
    grThreshold = 1.2;
    Cout << "WARN (DREAM): gr_threshold < 0.0, resetting to 1.2 (default)." 
	 << std::endl;
  }

  if (jumpStep < 1) {
    jumpStep = 5;
    Cout << "WARN (DREAM): jump_step < 1, resetting to 5 (default)." 
	 << std::endl;
  }
}


NonDDREAMBayesCalibration::~NonDDREAMBayesCalibration()
{ }


/** Perform the uncertainty quantification */
void NonDDREAMBayesCalibration::quantify_uncertainty()
{
  // instantiate DREAM objects and execute
  NonDDREAMInstance = this;

  // diagnostic information
  Cout << "INFO (DREAM): Standardized space " << standardizedSpace << '\n';
  Cout << "INFO (DREAM): Num Samples " << numSamples << '\n';
  Cout << "INFO (DREAM): Calibrate Sigma " << calibrateSigma  << '\n';
 
  // initialize the mcmcModel (including emulator construction) if needed
  initialize_model();

  // Set seed in both local generator and the one underlying DREAM in RNGLIB
  // BMA TODO: Burkhardt says replace RNGLIB with Dakota RNG
  if (randomSeed) {
    dream::set_seed(randomSeed, randomSeed);
    rnumGenerator.seed(randomSeed);
    Cout << " DREAM Seed (user-specified) = " << randomSeed << std::endl;
  }
  else {
    // Use NonD convenience function for system seed
    int clock_seed = generate_system_seed();
    dream::set_seed(clock_seed, clock_seed); 
    rnumGenerator.seed(clock_seed);
    Cout << " DREAM Seed (system-generated) = " << clock_seed << std::endl;
  }

  // BMA TODO: share most of this code with QUESO class

  // Read in all of the experimental data:  any x configuration 
  // variables, y observations, and y_std if available 
  bool calc_sigma_from_data = true; // calculate sigma if not provided
  //if (outputLevel > NORMAL_OUTPUT)
  //  Cout << "Read data from file " << calibrationData << '\n';
  if (calibrationData)
    expData.load_data("DREAM Bayes Calibration", calc_sigma_from_data);
  else
    Cout << "No experiment data from files\n"
	 << "DREAM is assuming the simulation is returning the residuals\n";
  if (calibrateSigma && !calibrationData) {
    Cerr << "\nError: you are attempting to calibrate the measurement error "
         << "but have not provided experimental data information."<<std::endl;
    abort_handler(METHOD_ERROR);
  }


  ////////////////////////////////////////////////////////
  // Step 2 of 5: Instantiate the parameter domain
  ////////////////////////////////////////////////////////
  int total_num_params = (calibrateSigma) ?
    numContinuousVars + numFunctions : numContinuousVars; 
  
  // resize, initializing to zero
  paramMins.size(total_num_params);
  paramMaxs.size(total_num_params);

  const RealVector& lower_bounds = mcmcModel.continuous_lower_bounds();
  const RealVector& upper_bounds = mcmcModel.continuous_upper_bounds();
  const RealVector& init_point   = mcmcModel.continuous_variables();
  Cout << "Initial Points " << init_point << '\n';

  if (emulatorType == GP_EMULATOR || emulatorType == KRIGING_EMULATOR || 
      emulatorType == NO_EMULATOR) {
    for (size_t i=0;i<numContinuousVars;i++) {
      paramMins[i] = lower_bounds[i];
      paramMaxs[i] = upper_bounds[i];
    }
  }
  else { // case PCE_EMULATOR: case SC_EMULATOR:
    Iterator* se_iter = NonDDREAMInstance->stochExpIterator.iterator_rep();
    Pecos::ProbabilityTransformation& nataf = 
      ((NonD*)se_iter)->variable_transformation(); 
    RealVector lower_u, upper_u;
    nataf.trans_X_to_U(lower_bounds,lower_u);
    nataf.trans_X_to_U(upper_bounds,upper_u);
    for (size_t i=0;i<numContinuousVars;i++) {
//      paramMins[i]=lower_bounds[i];
//      paramMaxs[i]=upper_bounds[i];
      paramMins[i]=lower_u[i];
      paramMaxs[i]=upper_u[i];
    }
  }
  // the parameter domain will now be expanded by sigma terms if 
  // calibrateSigma is true
  if (calibrateSigma) {
    RealVector covariance_diagonal;
    // Todo: pass in corrrect experiment number as second argument
    expData.get_main_diagonal( covariance_diagonal, 0 );
    for (int j=0; j<numFunctions; j++){
      Real std_j = std::sqrt( covariance_diagonal[j] );
      paramMins[numContinuousVars+j] = 0.01*std_j;
      paramMaxs[numContinuousVars+j] = 2.0*std_j;
      //paramMins[numContinuousVars+j] = 1.0; //0.01*std_0_j;
      //paramMaxs[numContinuousVars+j] = 1.0; //2.0*std_0_j;
    }
  }
 
  Cout << "INFO (DREAM): calibrateSigma  " << calibrateSigma << '\n';
  Cout << "INFO (DREAM): paramMins  " << paramMins << '\n';
  Cout << "INFO (DREAM): paramMaxs  " << paramMaxs << '\n';
  
  ////////////////////////////////////////////////////////
  // Step 3 of 5: Instantiate the likelihood function object
  ////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////
  // Step 4 of 5: Instantiate the inverse problem
  ////////////////////////////////////////////////////////
  
  // initialize the prior PDF and sampler
  // the prior is assumed uniform for DREAM

  // clear since this is a run-time operation
  priorDistributions.clear();
  priorSamplers.clear();
  for (int i=0; i<total_num_params; ++i) {
    priorDistributions.
      push_back(boost::math::uniform(paramMins[i], paramMaxs[i]));
    priorSamplers.
      push_back(boost::uniform_real<double>(paramMins[i], paramMaxs[i]));
  }

  ////////////////////////////////////////////////////////
  // Step 5 of 5: Solve the inverse problem
  ////////////////////////////////////////////////////////

  // BMA TODO: set initial point and proposal covariance if relevant

  // uqGslVectorClass paramInitials(paramSpace.zeroVector());
  // uqGslVectorClass covDiag(paramSpace.zeroVector());
  // //uqGslMatrixClass proposalCovMatrix(paramSpace.zeroVector());
  // for (int i=0;i<numContinuousVars;i++) {
  //   if (init_point[i])
  //     paramInitials[i]=init_point[i];
  //   else 
  //     paramInitials[i]=(paramMaxs[i]+paramMins[i])/2.0;
  // }
  // for (int i=numContinuousVars;i<total_num_params;i++) {
  //   paramInitials[i]=(paramMaxs[i]+paramMins[i])/2.0;
  // }
  // Cout << "initParams " << paramInitials << '\n';

  // uqGslMatrixClass* proposalCovMatrix = postRv.imageSet().vectorSpace().newProposalMatrix(&covDiag,&paramInitials); 
  // //uqGslMatrixClass proposalCovMatrix(covDiag);
  // Cout << "ProposalCovMatrix " << '\n'; 
  // for (size_t i=0;i<total_num_params;i++) {
  //   for (size_t j=0;j<total_num_params;j++) 
  //      Cout <<  (*proposalCovMatrix)(i,j) << "  " ; 
  // }
  // ip.solveWithBayesMetropolisHastings(calIpMhOptionsValues,
  //                                   paramInitials, proposalCovMatrix);

  Cout << "INFO (DREAM): Running DREAM for Bayesian inference." << std::endl;
  dream_main();

  return;
}


//void NonDDREAMBayesCalibration::print_results(std::ostream& s)
//{
//  NonDBayesCalibration::print_results(s);
//
//  additional DREAM output
//}

// BMA TODO: share most of this code with QUESO
/** Static callback function to evaluate the likelihood */
double NonDDREAMBayesCalibration::sample_likelihood (int par_num, double zp[])
{
  double result = 0.;
  size_t i,j,k;
  int num_exp = NonDDREAMInstance->numExperiments,
    num_funcs = NonDDREAMInstance->numFunctions,
    num_cont  = NonDDREAMInstance->numContinuousVars; 

  // BMA TODO:
  // Bug: if calibrating sigma, this would be bigger
  //RealVector x(Teuchos::View, zp, par_num);
  RealVector x(Teuchos::View, zp, num_cont);
    
  //Cout << "numExperiments" << num_exp << '\n';
  //Cout << "numFunctions" << num_funcs << '\n';
  //Cout << "numExpStdDeviationsRead " << NonDDREAMInstance->numExpStdDeviationsRead << '\n';

  // DREAM searches in either the original space (default for GPs and no
  // emulator) or standardized space (PCE/SC, optional for GP/no emulator).  
  NonDDREAMInstance->mcmcModel.continuous_variables(x); 

  // Compute simulation response to use in likelihood 
  NonDDREAMInstance->mcmcModel.compute_response();
  const Response& curr_resp = NonDDREAMInstance->mcmcModel.current_response();
  const RealVector& fn_vals = curr_resp.function_values();
 
  // Calculate the likelihood depending on what information is available 
  // for the standard deviations
  // NOTE:  If the calibration of the sigma terms is included, we assume 
  // ONE sigma term per function is calibrated. 
  // Otherwise, we assume that yStdData has already had the correct values 
  // placed depending if there is zero, one, num_funcs, or a full num_exp*num_func 
  // matrix of standard deviations.  Thus, we just have to iterate over this to 
  // calculate the likelihood. 
  if (!NonDDREAMInstance->calibrationData) {
    for (j=0; j<num_funcs; j++)
      result += std::pow(fn_vals[j],2.);
  }
  else if (NonDDREAMInstance->calibrateSigma) {
    for (i=0; i<num_exp; i++) {
      const RealVector& exp_data = NonDDREAMInstance->expData.all_data(i);
      for (j=0; j<num_funcs; j++)
	result += pow((fn_vals(j)-exp_data[j])/zp[num_cont+j],2.0);
    }
  }
  else {
    RealVector total_residuals( NonDDREAMInstance->expData.num_total_exppoints() );
    int cntr = 0;
    for (i=0; i<num_exp; i++) {
      RealVector residuals;
      NonDDREAMInstance->expData.form_residuals_deprecated(curr_resp, i, residuals);
      copy_data_partial( residuals, 0, residuals.length(), total_residuals,
			 cntr );
      cntr += residuals.length();
    }
    if (NonDDREAMInstance->expData.variance_active()){
      for (i=0; i<num_exp; i++)
  	result += NonDDREAMInstance->expData.apply_covariance(total_residuals, i);	
    }else
      result = total_residuals.dot(total_residuals);
    // replace with data_difference core which needs to be raised to expData
  }

  result = (result/(NonDDREAMInstance->likelihoodScale));
  result = -0.5*result;
  Cout << "Log likelihood is " << result << '\n';
  if (NonDDREAMInstance->outputLevel > NORMAL_OUTPUT) {
    Cout << "Likelihood is " << exp(result) << '\n';
    std::ofstream QuesoOutput;
    QuesoOutput.open("DreamOutput.txt", std::ios::out | std::ios::app);
    for (i=0; i<num_cont; i++) 
      QuesoOutput << x(i) << ' ' ;
    for (j=0; j<num_funcs; j++)
      QuesoOutput << fn_vals(j) << ' ' ;
    QuesoOutput << result << '\n';
    QuesoOutput.close();
  }
  return result;
}


/** See documentation in DREAM examples) */			     
void NonDDREAMBayesCalibration::
problem_size(int &chain_num, int &cr_num, int &gen_num, int &pair_num,
	     int &par_num)
{
  chain_num = NonDDREAMInstance->numChains;
  cr_num    = NonDDREAMInstance->numCR;
  gen_num   = NonDDREAMInstance->numGenerations;
  pair_num  = NonDDREAMInstance->crossoverChainPairs;
  par_num   = (NonDDREAMInstance->calibrateSigma) ?
    NonDDREAMInstance->numFunctions + NonDDREAMInstance->numContinuousVars :
    NonDDREAMInstance->numContinuousVars;

  return;
}

/** See documentation in DREAM examples) */			     
void  NonDDREAMBayesCalibration::
problem_value(string *chain_filename, string *gr_filename, double &gr_threshold,
	      int &jumpstep, double limits[], int par_num, int &printstep, 
	      string *restart_read_filename, string *restart_write_filename)
{
  int j;

  // BMA TODO: 
  // * Allow user to set the output filenames

  // parameters to expose to user...

  // DREAM will replace the zeros with unique file tags; use one
  // placeholder for 1--10 chains, two for 11--100 chains, etc.
  int chain_tag_len = 1;
  if (NonDDREAMInstance->numChains > 10)
    chain_tag_len = 
      (int) std::ceil(std::log10((double) NonDDREAMInstance->numChains));
  std::string chain_tag(chain_tag_len, '0');
  std::string chain_fname("dakota_dream_chain");
  chain_fname += chain_tag + ".txt";

  *chain_filename = chain_fname.c_str();
  *gr_filename = "dakota_dream_gr.txt";
  gr_threshold = NonDDREAMInstance->grThreshold;
  jumpstep = NonDDREAMInstance->jumpStep;

  for ( j = 0; j < par_num; j++ )
  {
    limits[0+j*2] = NonDDREAMInstance->paramMins[j];
    limits[1+j*2] = NonDDREAMInstance->paramMaxs[j];
    Cout << "min " << j << " = " << limits[0+j*2] << std::endl; 
    Cout << "max " << j << " = " << limits[1+j*2] << std::endl; 
  }

  // parameters to expose to user...
  printstep = 10;
  *restart_read_filename = "";
  *restart_write_filename = "dakota_dream_restart.txt";

  return;
}

/** See documentation in DREAM examples) */			     
double  NonDDREAMBayesCalibration::prior_density ( int par_num, double zp[] )
{
  int i;
  double value;

  value = 1.0;

  for ( i = 0; i < par_num; i++ )    
  {
    //value = value * r8_uniform_pdf (NonDDREAMInstance->paramMins[i],
    //				    NonDDREAMInstance->paramMaxs[i], zp[i] );
    value *= boost::math::pdf(NonDDREAMInstance->priorDistributions[i], zp[i]);
  }

  return value;
}

/** See documentation in DREAM examples) */			     
double *  NonDDREAMBayesCalibration::prior_sample ( int par_num )
{
  int i;
  double *zp;

  zp = ( double * ) malloc ( par_num * sizeof ( double ) );

  for ( i = 0; i < par_num; i++ )
  {
    //    zp[i] = r8_uniform_sample ( NonDDREAMInstance->paramMins[i], 
    //				NonDDREAMInstance->paramMaxs[i] );
    zp[i] =
      NonDDREAMInstance->priorSamplers[i](NonDDREAMInstance->rnumGenerator);
  }

  return zp;
}


} // namespace Dakota
