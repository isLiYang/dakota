/*  _______________________________________________________________________

    DAKOTA: Design Analysis Kit for Optimization and Terascale Applications
    Copyright 2014 Sandia Corporation.
    This software is distributed under the GNU Lesser General Public License.
    For more information, see the README file in the top Dakota directory.
    _______________________________________________________________________ */

//- Class:       Minimizer
//- Description: Implementation code for the Minimizer class
//- Owner:       Mike Eldred
//- Checked by:

#include "DakotaMinimizer.hpp"
#include "dakota_system_defs.hpp"
#include "dakota_data_io.hpp"
#include "dakota_tabular_io.hpp"
#include "DakotaModel.hpp"
#include "ProblemDescDB.hpp"
#include "IteratorScheduler.hpp"
#include "ParamResponsePair.hpp"
#include "PRPMultiIndex.hpp"
#include "RecastModel.hpp"
#include "Teuchos_SerialDenseHelpers.hpp"
#ifdef __SUNPRO_CC
#include <math.h>  // for std::log
#endif // __SUNPRO_CC

static const char rcsId[]="@(#) $Id: DakotaMinimizer.cpp 7029 2010-10-22 00:17:02Z mseldre $";


namespace Dakota {

extern PRPCache data_pairs; // global container

// initialization of static needed by RecastModel
Minimizer* Minimizer::minimizerInstance(NULL);


/** This constructor extracts inherited data for the optimizer and least
    squares branches and performs sanity checking on constraint settings. */
Minimizer::Minimizer(ProblemDescDB& problem_db, Model& model): 
  Iterator(BaseConstructor(), problem_db),
  constraintTol(probDescDB.get_real("method.constraint_tolerance")),
  bigRealBoundSize(1.e+30), bigIntBoundSize(1000000000),
  boundConstraintFlag(false),
  speculativeFlag(probDescDB.get_bool("method.speculative")),
  minimizerRecasts(0), optimizationFlag(true),
  calibrationDataFlag(probDescDB.get_bool("responses.calibration_data") ||
		      !probDescDB.get_string("responses.scalar_data_filename").empty()),
  expData(probDescDB, model.current_response().shared_data(), outputLevel),
  numExperiments(0), numTotalCalibTerms(0),
  applyCovariance(false), matrixCovarianceActive(false),
  scaleFlag(probDescDB.get_bool("method.scaling")), varsScaleFlag(false),
  primaryRespScaleFlag(false), secondaryRespScaleFlag(false)
{
  iteratedModel = model;
  update_from_model(iteratedModel); // variable/response counts & checks

  // Re-assign Iterator defaults specialized to Minimizer branch
  if (maxIterations < 0) // DataMethod default set to -1
    maxIterations = 100;
  // Minimizer default number of final solution is 1, unless a
  // multi-objective frontier-based method
  if (!numFinalSolutions && methodName != MOGA)
    numFinalSolutions = 1;
}


Minimizer::Minimizer(unsigned short method_name, Model& model):
  Iterator(NoDBBaseConstructor(), method_name, model), constraintTol(0.),
  bigRealBoundSize(1.e+30), bigIntBoundSize(1000000000),
  boundConstraintFlag(false), speculativeFlag(false), minimizerRecasts(0),
  optimizationFlag(true),
  calibrationDataFlag(false), numExperiments(0), numTotalCalibTerms(0),
  applyCovariance(false), matrixCovarianceActive(false),
  scaleFlag(false), varsScaleFlag(false),
  primaryRespScaleFlag(false), secondaryRespScaleFlag(false)
{
  update_from_model(iteratedModel); // variable,constraint counts & checks
}


Minimizer::Minimizer(unsigned short method_name, size_t num_lin_ineq,
		     size_t num_lin_eq, size_t num_nln_ineq, size_t num_nln_eq):
  Iterator(NoDBBaseConstructor(), method_name),
  bigRealBoundSize(1.e+30), bigIntBoundSize(1000000000),
  numNonlinearIneqConstraints(num_nln_ineq),
  numNonlinearEqConstraints(num_nln_eq), numLinearIneqConstraints(num_lin_ineq),
  numLinearEqConstraints(num_lin_eq),
  numNonlinearConstraints(num_nln_ineq + num_nln_eq),
  numLinearConstraints(num_lin_ineq + num_lin_eq),
  numConstraints(numNonlinearConstraints + numLinearConstraints),
  numUserPrimaryFns(1), numIterPrimaryFns(1), boundConstraintFlag(false),
  speculativeFlag(false), minimizerRecasts(0), optimizationFlag(true),
  calibrationDataFlag(false),
  numExperiments(0), numTotalCalibTerms(0),
  applyCovariance(false), matrixCovarianceActive(false),
  scaleFlag(false), varsScaleFlag(false), primaryRespScaleFlag(false), 
  secondaryRespScaleFlag(false)
{ }


void Minimizer::update_from_model(const Model& model)
{
  Iterator::update_from_model(model);

  numContinuousVars     = model.cv();  numDiscreteIntVars  = model.div();
  numDiscreteStringVars = model.dsv(); numDiscreteRealVars = model.drv();
  numFunctions          = model.num_functions();

  bool err_flag = false;
  // Check for correct bit associated within methodName
  if ( !(methodName & MINIMIZER_BIT) ) {
    Cerr << "\nError: minimizer bit not activated for method instantiation "
	 << "within Minimizer branch." << std::endl;
    err_flag = true;
  }
  // Check for active design variables and discrete variable support
  if (methodName == MOGA      || methodName == SOGA ||
      methodName == COLINY_EA || methodName == SURROGATE_BASED_GLOBAL ||
      methodName == MESH_ADAPTIVE_SEARCH ) {
    if (!numContinuousVars && !numDiscreteIntVars && !numDiscreteStringVars &&
	!numDiscreteRealVars) {
      Cerr << "\nError: " << method_enum_to_string(methodName)
	   << " requires active variables." << std::endl;
      err_flag = true;
    }
  }
  else { // methods supporting only continuous design variables
    if (!numContinuousVars) {
      Cerr << "\nError: " << method_enum_to_string(methodName)
	   << " requires active continuous variables." << std::endl;
      err_flag = true;
    }
    if (numDiscreteIntVars || numDiscreteStringVars || numDiscreteRealVars)
      Cerr << "\nWarning: discrete design variables ignored by "
	   << method_enum_to_string(methodName) << std::endl;
  }
  // Check for response functions
  if ( numFunctions <= 0 ) {
    Cerr << "\nError: number of response functions must be greater than zero."
	 << std::endl;
    err_flag = true;
  }

  // check for gradient/Hessian/minimizer match: abort with an error for cases
  // where insufficient derivative data is available (e.g., full Newton methods
  // require Hessians), but only echo warnings in other cases (e.g., if more
  // derivative data is specified than is needed --> for example, don't enforce
  // that analytic Hessians require full Newton methods).
  const String& grad_type = model.gradient_type();
  const String& hess_type = model.hessian_type();
  if (outputLevel >= VERBOSE_OUTPUT)
    Cout << "Gradient type = " << grad_type << " Hessian type = " << hess_type
	 << '\n';
  if ( grad_type == "none" && ( ( methodName & LEASTSQ_BIT ) ||
       ( ( methodName & OPTIMIZER_BIT ) && methodName >= NONLINEAR_CG ) ) ) {
    Cerr << "\nError: gradient-based minimizers require a gradient "
         << "specification." << std::endl;
    err_flag = true;
  }
  if ( hess_type != "none" && methodName != OPTPP_NEWTON )
    Cerr << "\nWarning: Hessians are only utilized by full Newton methods.\n\n";
  if ( ( grad_type != "none" || hess_type != "none") &&
       ( ( methodName & OPTIMIZER_BIT ) && methodName < NONLINEAR_CG ) )
    Cerr << "\nWarning: Gradient/Hessian specification for a nongradient-based "
	 << "optimizer is ignored.\n\n";
  // TO DO: verify vendor finite differencing support
  vendorNumericalGradFlag
    = (grad_type == "numerical" && model.method_source() == "vendor");

  numNonlinearIneqConstraints = model.num_nonlinear_ineq_constraints();
  numNonlinearEqConstraints = model.num_nonlinear_eq_constraints();
  numLinearIneqConstraints = model.num_linear_ineq_constraints(); 
  numLinearEqConstraints = model.num_linear_eq_constraints();
  numNonlinearConstraints = numNonlinearIneqConstraints +
                            numNonlinearEqConstraints;
  numLinearConstraints = numLinearIneqConstraints + numLinearEqConstraints;
  numConstraints = numNonlinearConstraints + numLinearConstraints;
  numUserPrimaryFns = model.num_primary_fns();
  numIterPrimaryFns = numUserPrimaryFns;
  if (model.primary_fn_type() == CALIB_TERMS)
    numTotalCalibTerms = numUserPrimaryFns;  // default value
  // Check for linear constraint support in method selection
  if ( ( numLinearIneqConstraints   || numLinearEqConstraints ) &&
       ( methodName == NL2SOL       ||
	 methodName == NONLINEAR_CG || methodName == OPTPP_CG             || 
	 ( methodName >= OPTPP_PDS  && methodName <= COLINY_SOLIS_WETS )  ||
	 methodName == NCSU_DIRECT  || methodName == MESH_ADAPTIVE_SEARCH ||
	 methodName == GENIE_DIRECT || methodName == GENIE_OPT_DARTS      ||
         methodName == DL_SOLVER    || methodName == EFFICIENT_GLOBAL ) ) {
    Cerr << "\nError: linear constraints not currently supported by "
	 << method_enum_to_string(methodName) << ".\n       Please select a "
	 << "different method for generally constrained problems." << std::endl;
    err_flag = true;
  }
  // Check for nonlinear constraint support in method selection.  Note that
  // CONMIN and DOT swap method selections as needed for constraint support.
  if ( ( numNonlinearIneqConstraints || numNonlinearEqConstraints ) &&
       ( methodName == NL2SOL        || methodName == OPTPP_CG    ||
	 methodName == NONLINEAR_CG  || methodName == OPTPP_PDS   ||
	 methodName == NCSU_DIRECT   || methodName == GENIE_DIRECT ||
         methodName == GENIE_OPT_DARTS )) {
    Cerr << "\nError: nonlinear constraints not currently supported by "
	 << method_enum_to_string(methodName) << ".\n       Please select a "
	 << "different method for generally constrained problems." << std::endl;
    err_flag = true;
  }

  if (err_flag)
    abort_handler(-1);

  // set boundConstraintFlag
  size_t i;
  const RealVector& c_l_bnds = model.continuous_lower_bounds();
  const RealVector& c_u_bnds = model.continuous_upper_bounds();
  for (i=0; i<numContinuousVars; ++i)
    if (c_l_bnds[i] > -bigRealBoundSize || c_u_bnds[i] < bigRealBoundSize) {
      boundConstraintFlag = true;
      break;
    }
  bool discrete_bounds = (methodName == MOGA || methodName == SOGA ||
			  methodName == COLINY_EA);
  if (discrete_bounds) {
    const IntVector&  di_l_bnds = model.discrete_int_lower_bounds();
    const IntVector&  di_u_bnds = model.discrete_int_upper_bounds();
    const RealVector& dr_l_bnds = model.discrete_real_lower_bounds();
    const RealVector& dr_u_bnds = model.discrete_real_upper_bounds();
    for (i=0; i<numDiscreteIntVars; ++i)
      if (di_l_bnds[i] > -bigIntBoundSize || di_u_bnds[i] < bigIntBoundSize) {
	boundConstraintFlag = true;  
	break;
      }
    for (i=0; i<numDiscreteRealVars; ++i)
      if (dr_l_bnds[i] > -bigRealBoundSize || dr_u_bnds[i] < bigRealBoundSize) {
	boundConstraintFlag = true;  
	break;
      }
  }
}


void Minimizer::initialize_run()
{
  // Verify that iteratedModel is not null (default ctor and some
  // NoDBBaseConstructor ctors leave iteratedModel uninitialized).
  if (!iteratedModel.is_null()) {
    // update context data that is outside scope of local DB specifications.
    // This is needed for reused objects.
    //iteratedModel.db_scope_reset(); // TO DO: need better name?

    // Do not reset the evaluation reference for sub-iterators
    // (previously managed via presence/absence of ostream)
    //if (!subIteratorFlag)
    if (summaryOutputFlag)
      iteratedModel.set_evaluation_reference();
  }

  // Track any previous object instance in case of recursion.  Note that
  // optimizerInstance and minimizerInstance must be tracked separately since
  // the previous optimizer and previous minimizer could be different instances
  // (e.g., for MCUU with NL2SOL for NLS and NPSOL for MPP search, the previous
  // minimizer is NL2SOL and the previous optimizer is NULL).
  prevMinInstance   = minimizerInstance;
  minimizerInstance = this;

  if (subIteratorFlag) { 

    // Catch any updates to all inactive vars.  For now, do this in
    // initialize because derived classes may elect to reimplement
    // post_run.  Needs to happen before derived solvers update their
    // best points, so it doesn't trample their active variables data.

    // Dive into the originally passed model (could keep a shallow copy of it)
    // Don't use a reference here as want a shallow copy, not the instance
    Model usermodel(iteratedModel);
    for (unsigned short i=1; i<=minimizerRecasts; ++i) {
      usermodel = usermodel.subordinate_model();
    }
    
    // Could be lighter weight, but don't have a way to update only inactive
    bestVariablesArray.front().all_continuous_variables(
      usermodel.all_continuous_variables());
    bestVariablesArray.front().all_discrete_int_variables(
      usermodel.all_discrete_int_variables());
    bestVariablesArray.front().all_discrete_real_variables(
      usermodel.all_discrete_real_variables());
  }
}


void Minimizer::post_run(std::ostream& s)
{
  if (summaryOutputFlag) {
    // Print the function evaluation summary for all Iterators
    if (!iteratedModel.is_null())
      iteratedModel.print_evaluation_summary(s); // full hdr, relative counts

    // The remaining final results output varies by iterator branch
    print_results(s);
  }

  resultsDB.write_databases();
}


Model Minimizer::original_model(unsigned short recasts_left)
{
  // Dive into the originally passed model (could keep a shallow copy of it)
  // Don't use a reference here as want a shallow copy, not the instance
  Model usermodel(iteratedModel);
  for (unsigned short i=1; i<=minimizerRecasts - recasts_left; ++i) {
    usermodel = usermodel.subordinate_model();
  }

  return(usermodel);
}


/** Reads observation data to compute least squares residuals.  Does
    not change size of responses, and is the first wrapper, therefore
    sizes are based on iteratedModel.  */
void Minimizer::data_transform_model()
{
  if (outputLevel >= DEBUG_OUTPUT)
    Cout << "Initializing data transformation" << std::endl;
  
  // These may be promoted to members once we use state vars / sigma
  // TODO: need better validation of these sizes and data with error msgs
  numExperiments = probDescDB.get_sizet("responses.num_experiments");
  if (numExperiments < 1) {
      Cerr << "Error in number of experiments" << std::endl;
      abort_handler(-1);
  }

  // this option is currently ignored by ExperimentData; note that we
  // don't want to weight by missing sigma all = 1.0
  bool calc_sigma_from_data = true; // calculate sigma if not provided 
  expData.load_data("Least Squares", calc_sigma_from_data);

  // update sizes in Iterator view
  numTotalCalibTerms = expData.num_total_exppoints();
  numFunctions = numTotalCalibTerms + numNonlinearConstraints;
  numIterPrimaryFns = numTotalCalibTerms;

  if (outputLevel > NORMAL_OUTPUT)
    Cout << "Adjusted number of calibration terms: " << numTotalCalibTerms 
	 << std::endl;

  size_t num_config_vars_read = 
    probDescDB.get_sizet("responses.num_config_vars");
  if (num_config_vars_read > 0 && outputLevel >= QUIET_OUTPUT)
    Cout << "\nWarning (least squares): experimental_config_variables " 
	 << "will be read from file, but ignored." << std::endl;

  applyCovariance = false;
  matrixCovarianceActive = false;
  if (expData.variance_type_active(MATRIX_SIGMA)) {
    // can't apply matrix-valued errors due to possibly incomplete
    // dataset when active set vector is in use (missing residuals)
    applyCovariance = true;
    matrixCovarianceActive = true;
  }
  else if (expData.variance_type_active(SCALAR_SIGMA) || 
	   expData.variance_type_active(DIAGONAL_SIGMA))
    applyCovariance = true;

  // BMA TODO:
  // !!! The size of the variables map should be all active variables,
  // !!! not continuous!!!
  
  Sizet2DArray var_map_indices(numContinuousVars), 
    primary_resp_map_indices(numTotalCalibTerms), 
    secondary_resp_map_indices(numNonlinearConstraints);
  bool nonlinear_vars_map = false;
  size_t num_recast_fns = numTotalCalibTerms + numNonlinearConstraints;
  BoolDequeArray nonlinear_resp_map(num_recast_fns);
  // adjust active set vector to 1 + numNonlinearConstraints
  ShortArray asv(numTotalCalibTerms + numNonlinearConstraints, 1);
  activeSet.request_vector(asv);
  
  for (size_t i=0; i<numContinuousVars; i++) {
    var_map_indices[i].resize(1);
    var_map_indices[i][0] = i;
  }

  const SharedResponseData& srd = iteratedModel.current_response().shared_data();
  gen_primary_resp_map(srd, primary_resp_map_indices, nonlinear_resp_map);
  Cout << "prminds = \n" << primary_resp_map_indices << "\n";

  for (size_t i=0; i<numNonlinearConstraints; i++) {
    secondary_resp_map_indices[i].resize(1);
    // the recast constraints just depend on the simulation
    // constraints, which start at numUserPrimaryFns
    secondary_resp_map_indices[i][0] = numUserPrimaryFns + i;
    nonlinear_resp_map[numUserPrimaryFns+i].resize(1);
    nonlinear_resp_map[numUserPrimaryFns+i][0] = false;
  }

  void (*vars_recast) (const Variables&, Variables&) = NULL;
  // The default active set recasting should work for current use cases
  void (*set_recast)  (const Variables&, const ActiveSet&, ActiveSet&) = NULL;
  void (*pri_resp_recast) (const Variables&, const Variables&,
			   const Response&, Response&)
    = primary_resp_differencer;
  void (*sec_resp_recast) (const Variables&, const Variables&,
			   const Response&, Response&)
    = secondary_resp_copier;

  size_t recast_secondary_offset = numNonlinearIneqConstraints;
  SizetArray recast_vars_comps_total;  // default: empty; no change in size
  BitArray all_relax_di, all_relax_dr; // default: empty; no discrete relaxation
  iteratedModel.assign_rep(new
    RecastModel(iteratedModel, var_map_indices, recast_vars_comps_total,
		all_relax_di, all_relax_dr, nonlinear_vars_map, vars_recast,
		set_recast, primary_resp_map_indices,
		secondary_resp_map_indices, recast_secondary_offset,
		nonlinear_resp_map, pri_resp_recast, sec_resp_recast), false);
  ++minimizerRecasts;


  // TODO: review where should derivatives happen in data transform
  // case: where derivatives are computed should be tied to whether
  // interpolation is present

  // Need to correct a flaw in doing this: estimate derivatives uses a
  // mix of DB lookups and new evals, but the DB lookups aren't
  // transformed!

  // In the data transform case, perform numerical derivatives at the
  // RecastModel level (override the RecastModel default and the
  // subModel default)
  // BMA: Disabled until debugged...
  //  iteratedModel.supports_derivative_estimation(true);
  //  RecastModel* recast_model_rep = (RecastModel*) iteratedModel.model_rep();
  //  recast_model_rep->submodel_supports_derivative_estimation(false);

  // The following expansions are conservative.  Could be skipped when
  // only scalar data present and no replicates.

  // Preserve weights through data transformations
  const RealVector& submodel_weights = 
    iteratedModel.subordinate_model().primary_response_fn_weights();
  if (!submodel_weights.empty()) { 
    RealVector recast_weights(numTotalCalibTerms);
    expand_array(srd, submodel_weights, recast_weights);
    iteratedModel.primary_response_fn_weights(recast_weights);
  }
  // Preserve sense through data transformations
  const BoolDeque& submodel_sense = 
    iteratedModel.subordinate_model().primary_response_fn_sense();
  if (!submodel_sense.empty()) {
    BoolDeque recast_sense(numTotalCalibTerms);
    expand_array(srd, submodel_sense, recast_sense);
    iteratedModel.primary_response_fn_sense(recast_sense);
  }
}


void Minimizer::
gen_primary_resp_map(const SharedResponseData& srd,
		     Sizet2DArray& primary_resp_map_indices,
		     BoolDequeArray& nonlinear_resp_map) const
{
  size_t num_scalar = srd.num_scalar_responses();
  size_t num_field_groups = srd.num_field_response_groups();
  const IntVector& sim_field_lens = srd.field_lengths();
  size_t calib_term_ind = 0;
  for (size_t exp_ind = 0; exp_ind < numExperiments; ++exp_ind) {
    // field lengths can be different per experiment
    const IntVector& exp_field_lens = expData.field_lengths(exp_ind);
    for (size_t scalar_ind = 0; scalar_ind < num_scalar; ++scalar_ind) {
      // simulation scalars inform calibration terms 1 to 1 
      // (no correlation or interpolation allowed)
      primary_resp_map_indices[calib_term_ind].resize(1);
      primary_resp_map_indices[calib_term_ind][0] = scalar_ind; //=calib_term_ind
      nonlinear_resp_map[calib_term_ind].resize(1);
      nonlinear_resp_map[calib_term_ind][0] = false;
      ++calib_term_ind;
    }
    // base index for simulation in current field group
    size_t sim_ind_offset = num_scalar; 
    for (size_t fg_ind = 0; fg_ind < num_field_groups; ++fg_ind) {
      // each field calibration term depends on all simulation field
      // entries for this field, due to correlation or interpolation
      // if no matrix correlation, no interpolation, could skip
      for (size_t z=0; z<exp_field_lens[fg_ind]; ++z) {
	primary_resp_map_indices[calib_term_ind].resize(sim_field_lens[fg_ind]);
	nonlinear_resp_map[calib_term_ind].resize(sim_field_lens[fg_ind]);
	// this residual depends on all other simulation data for this field
	for (size_t sim_ind = 0; sim_ind<sim_field_lens[fg_ind]; ++sim_ind) {
	  primary_resp_map_indices[calib_term_ind][sim_ind] = 
	    sim_ind_offset + sim_ind;
	  nonlinear_resp_map[calib_term_ind][sim_ind] = false;
	}
	++calib_term_ind;
      }
      sim_ind_offset += sim_field_lens[fg_ind];
    }
  }
}


template<typename T>
void Minimizer::
expand_array(const SharedResponseData& srd, const T& submodel_array, 
	     T& recast_array) const 
{
  size_t num_scalar = srd.num_scalar_responses();
  size_t num_field_groups = srd.num_field_response_groups();
  size_t calib_term_ind = 0;
  for (size_t exp_ind=0; exp_ind<numExperiments; ++exp_ind) {
    const IntVector& exp_field_lens = expData.field_lengths(exp_ind);
    for (size_t sc_ind = 0; sc_ind < num_scalar; ++sc_ind)
      recast_array[calib_term_ind++] = submodel_array[sc_ind];
    for (size_t fg_ind = 0; fg_ind < num_field_groups; ++fg_ind)
      recast_array[calib_term_ind++] = submodel_array[num_scalar + fg_ind];
  }
}


/** Wrap the iteratedModel in a scaling transformation, such that
    iteratedModel now contains a scaling recast model. Potentially
    affects variables, primary, and secondary responses */
void Minimizer::scale_model()
{
  // BMA TODO: scale_model needs to be modular on number of incoming
  // response functions, not hard-wired to numUserPrimaryFns.

  if (outputLevel >= DEBUG_OUTPUT)
    Cout << "Initializing scaling transformation" << std::endl;

  // RecastModel is constructed, then later initialized because scaled
  // properties need to be set on the RecastModel, like bounds, but
  // the nonlinearity of the mapping is determined by the scales
  // themselves.

  // iteratedModel becomes the sub-model of a RecastModel:
  SizetArray recast_vars_comps_total;  // default: empty; no change in size
  BitArray all_relax_di, all_relax_dr; // default: empty; no discrete relaxation
  iteratedModel.assign_rep(new
      RecastModel(iteratedModel, recast_vars_comps_total, all_relax_di,
		  all_relax_dr, numUserPrimaryFns, numNonlinearConstraints,
		  numNonlinearIneqConstraints), false);
  ++minimizerRecasts;

  // initialize_scaling function needs to modify the iteratedModel
  initialize_scaling();

  // setup recast model mappings and flags
  // recast map is all one to one unless single objective transformation
  size_t i;
  size_t num_recast_fns = numUserPrimaryFns + numNonlinearConstraints;
  Sizet2DArray var_map_indices(numContinuousVars), 
    primary_resp_map_indices(numUserPrimaryFns), 
    secondary_resp_map_indices(numNonlinearConstraints);
  bool nonlinear_vars_map = false;
  BoolDequeArray nonlinear_resp_map(num_recast_fns);

  // the scaling transformation doesn't change any counts of variables
  // or responses, but may require a nonlinear transformation of
  // responses (grad, Hess) when variables are transformed
  for (i=0; i<numContinuousVars; i++) {
    var_map_indices[i].resize(1);
    var_map_indices[i][0] = i;
    if (varsScaleFlag && cvScaleTypes[i] & SCALE_LOG)
      nonlinear_vars_map = true;
  }
  for (i=0; i<numUserPrimaryFns; i++) {
    primary_resp_map_indices[i].resize(1);
    primary_resp_map_indices[i][0] = i;
    nonlinear_resp_map[i].resize(1);
    nonlinear_resp_map[i][0] = 
      (primaryRespScaleFlag && responseScaleTypes[i] & SCALE_LOG);
  }
  for (i=0; i<numNonlinearConstraints; i++) {
    secondary_resp_map_indices[i].resize(1);
    secondary_resp_map_indices[i][0] = numUserPrimaryFns + i;
    nonlinear_resp_map[numUserPrimaryFns+i].resize(1);
    nonlinear_resp_map[numUserPrimaryFns+i][0] = secondaryRespScaleFlag &&
      responseScaleTypes[numUserPrimaryFns + i] & SCALE_LOG;
  }

  void (*vars_recast) (const Variables&, Variables&) = variables_scaler;
  void (*set_recast) (const Variables&, const ActiveSet&, ActiveSet&) = NULL;

  // register primary response scaler if requested, or variables scaled
  void (*pri_resp_recast) (const Variables&, const Variables&,
			   const Response&, Response&)
    = (primaryRespScaleFlag || varsScaleFlag) ? primary_resp_scaler : NULL;

  // scale secondary response if requested, or variables scaled
  void (*sec_resp_recast) (const Variables&, const Variables&,
			   const Response&, Response&)
    = (secondaryRespScaleFlag || varsScaleFlag) ? secondary_resp_scaler : NULL;


  RecastModel* recast_model_rep = (RecastModel*)iteratedModel.model_rep();
  recast_model_rep->initialize(var_map_indices, nonlinear_vars_map,
			       vars_recast, set_recast,
			       primary_resp_map_indices,
			       secondary_resp_map_indices, nonlinear_resp_map,
			       pri_resp_recast, sec_resp_recast);

  // Preserve weights through scaling transformation
  bool recurse_flag = false;
  const RealVector& submodel_weights = 
    iteratedModel.subordinate_model().primary_response_fn_weights();
  iteratedModel.primary_response_fn_weights(submodel_weights);

  // Preserve sense through scaling transformation
  // Note: for a specification of negative scaling, we will assume that
  // the user's intent is to overlay the scaling and sense as specified,
  // such that we will not enforce a flip in sense for negative scaling. 
  const BoolDeque& submodel_sense = 
    iteratedModel.subordinate_model().primary_response_fn_sense();
  iteratedModel.primary_response_fn_sense(submodel_sense);

}


/** Initialize scaling types, multipliers, and offsets.  Update the
    iteratedModel appropriately */
void Minimizer::initialize_scaling()
{
  if (outputLevel > NORMAL_OUTPUT)
    Cout << "\nMinimizer: Scaling enabled (any scaling derived from 'auto' " 
	 << "reported as value)" << std::endl;
  else if (outputLevel == NORMAL_OUTPUT)
    Cout << "\nMinimizer: Scaling enabled" << std::endl;

  // in the scaled case, perform numerical derivatives at the RecastModel level
  // (override the RecastModel default and the subModel default)
  iteratedModel.supports_derivative_estimation(true);
  RecastModel* recast_model_rep = (RecastModel*) iteratedModel.model_rep();
  recast_model_rep->submodel_supports_derivative_estimation(false);

  // temporary arrays
  IntArray    tmp_types;
  RealVector  tmp_multipliers, tmp_offsets;
  RealVector lbs, ubs, targets;
  //RealMatrix linear_constraint_coeffs;

  // need to access the subModel of the RecastModel for bounds/constraints
  const Model& sub_model = iteratedModel.subordinate_model();

  // NOTE: When retrieving scaling vectors from database, excepting linear 
  //       constraints, they've already been checked at input to have length 0, 
  //       1, or number of vars, constraints, etc.

  // -----------------
  // CONTINUOUS DESIGN
  // -----------------
  const StringArray& cdv_scale_strings
    = probDescDB.get_sa("variables.continuous_design.scale_types");
  const RealVector& cdv_scales
    = probDescDB.get_rv("variables.continuous_design.scales");
  varsScaleFlag = !cdv_scale_strings.empty();

  copy_data(sub_model.continuous_lower_bounds(), lbs); // view->copy
  copy_data(sub_model.continuous_upper_bounds(), ubs); // view->copy

  compute_scaling(CDV, BOUNDS, numContinuousVars, lbs, ubs, targets,
		  cdv_scale_strings, cdv_scales, cvScaleTypes,
		  cvScaleMultipliers, cvScaleOffsets);

  iteratedModel.continuous_lower_bounds(lbs);
  iteratedModel.continuous_upper_bounds(ubs);
  iteratedModel.continuous_variables(
    modify_n2s(sub_model.continuous_variables(), cvScaleTypes,
	       cvScaleMultipliers, cvScaleOffsets) );

  if (outputLevel > NORMAL_OUTPUT && varsScaleFlag) {
    StringArray cv_labels;
    copy_data(iteratedModel.continuous_variable_labels(), cv_labels);
    print_scaling("Continuous design variable scales", cvScaleTypes,
		  cvScaleMultipliers, cvScaleOffsets, cv_labels);
  }

  // each responseScale* = [fnScale*, nonlinearIneqScale*, nonlinearEqScale*]
  // to make transformations faster at run time 
  // numFunctions should reflect size of user-space model
  responseScaleTypes.resize(numFunctions);
  responseScaleMultipliers.resize(numFunctions);
  responseScaleOffsets.resize(numFunctions);

  // -------------------------
  // OBJECTIVE FNS / LSQ TERMS
  // -------------------------
  const StringArray& primary_scale_strings
    = probDescDB.get_sa("responses.primary_response_fn_scale_types");
  const RealVector& primary_scales
    = probDescDB.get_rv("responses.primary_response_fn_scales");
  primaryRespScaleFlag = !primary_scale_strings.empty();

  lbs.size(0); ubs.size(0);
  compute_scaling(FN_LSQ, DISALLOW, numUserPrimaryFns, lbs, ubs, targets,
		  primary_scale_strings, primary_scales, tmp_types,
		  tmp_multipliers, tmp_offsets);

  for (int i=0; i<numUserPrimaryFns; ++i) {
    responseScaleTypes[i]       = tmp_types[i];
    responseScaleMultipliers[i] = tmp_multipliers[i];
    responseScaleOffsets[i]     = 0;
  }

  // --------------------
  // NONLINEAR INEQUALITY
  // --------------------
  const StringArray& nln_ineq_scale_strings =
    probDescDB.get_sa("responses.nonlinear_inequality_scale_types");
  const RealVector& nln_ineq_scales
    = probDescDB.get_rv("responses.nonlinear_inequality_scales"); 
  secondaryRespScaleFlag = !nln_ineq_scale_strings.empty();

  lbs = sub_model.nonlinear_ineq_constraint_lower_bounds();
  ubs = sub_model.nonlinear_ineq_constraint_upper_bounds();

  compute_scaling(NONLIN, BOUNDS, numNonlinearIneqConstraints, lbs, ubs,
		  targets, nln_ineq_scale_strings, nln_ineq_scales, tmp_types,
		  tmp_multipliers, tmp_offsets);

  for (int i=0; i<numNonlinearIneqConstraints; ++i) {
    responseScaleTypes[numUserPrimaryFns+i]       = tmp_types[i];
    responseScaleMultipliers[numUserPrimaryFns+i] = tmp_multipliers[i];
    responseScaleOffsets[numUserPrimaryFns+i]     = tmp_offsets[i];
  }

  iteratedModel.nonlinear_ineq_constraint_lower_bounds(lbs);
  iteratedModel.nonlinear_ineq_constraint_upper_bounds(ubs);

  // --------------------
  // NONLINEAR EQUALITY
  // --------------------
  const StringArray& nln_eq_scale_strings
    = probDescDB.get_sa("responses.nonlinear_equality_scale_types");
  const RealVector& nln_eq_scales
    = probDescDB.get_rv("responses.nonlinear_equality_scales");
  secondaryRespScaleFlag
    = (secondaryRespScaleFlag || !nln_eq_scale_strings.empty());

  lbs.size(0); ubs.size(0);
  targets = sub_model.nonlinear_eq_constraint_targets();
  compute_scaling(NONLIN, TARGET, numNonlinearEqConstraints,
		  lbs, ubs, targets, nln_eq_scale_strings, nln_eq_scales,
		  tmp_types, tmp_multipliers, tmp_offsets);

  for (int i=0; i<numNonlinearEqConstraints; ++i) {
    responseScaleTypes[numUserPrimaryFns+numNonlinearIneqConstraints+i] 
      = tmp_types[i];
    responseScaleMultipliers[numUserPrimaryFns+numNonlinearIneqConstraints+i] 
      = tmp_multipliers[i];
    responseScaleOffsets[numUserPrimaryFns+numNonlinearIneqConstraints+i] 
      = tmp_offsets[i];
  }

  iteratedModel.nonlinear_eq_constraint_targets(targets);

  if (outputLevel > NORMAL_OUTPUT && 
      (primaryRespScaleFlag || secondaryRespScaleFlag) )
    print_scaling("Response scales", responseScaleTypes,
		  responseScaleMultipliers, responseScaleOffsets,
		  sub_model.response_labels());

  // LINEAR CONSTRAINT SCALING:
  // computed scales account for scaling in CVs x
  // updating constraint coefficient matrices now handled in derived classes
  // NOTE: cannot use offsets since would be an affine scale
  // ScaleOffsets in this case are only for applying to bounds

  // -----------------
  // LINEAR INEQUALITY
  // -----------------
  const StringArray& lin_ineq_scale_strings
    = probDescDB.get_sa("method.linear_inequality_scale_types");
  const RealVector& lin_ineq_scales
    = probDescDB.get_rv("method.linear_inequality_scales");

  if ( ( lin_ineq_scale_strings.size() != 0 &&
	 lin_ineq_scale_strings.size() != 1 && 
	 lin_ineq_scale_strings.size() != numLinearIneqConstraints  ) ||
       ( lin_ineq_scales.length() != 0 && lin_ineq_scales.length() != 1 && 
	 lin_ineq_scales.length() != numLinearIneqConstraints ) ) {
    Cerr << "Error: linear_inequality_scale specifications must have length 0, "
	 << "1, or " << numLinearIneqConstraints << ".\n";
    abort_handler(-1);
  }

  linearIneqScaleOffsets.resize(numLinearIneqConstraints);

  lbs = sub_model.linear_ineq_constraint_lower_bounds();
  ubs = sub_model.linear_ineq_constraint_upper_bounds();
  targets.size(0);

  const RealMatrix& lin_ineq_coeffs
    = sub_model.linear_ineq_constraint_coeffs();
  for (int i=0; i<numLinearIneqConstraints; ++i) {

    // compute A_i*cvScaleOffset for current constraint -- discrete variables
    // aren't scaled so don't contribute
    linearIneqScaleOffsets[i] = 0.0;
    for (int j=0; j<numContinuousVars; ++j)
      linearIneqScaleOffsets[i] += lin_ineq_coeffs(i,j)*cvScaleOffsets[j];
    
    lbs[i] -= linearIneqScaleOffsets[i];
    ubs[i] -= linearIneqScaleOffsets[i];

  }
  compute_scaling(LINEAR, BOUNDS, numLinearIneqConstraints,
		  lbs, ubs, targets, lin_ineq_scale_strings, lin_ineq_scales,
		  linearIneqScaleTypes, linearIneqScaleMultipliers, 
		  tmp_offsets);

  iteratedModel.linear_ineq_constraint_lower_bounds(lbs);
  iteratedModel.linear_ineq_constraint_upper_bounds(ubs);
  iteratedModel.linear_ineq_constraint_coeffs(
    lin_coeffs_modify_n2s(lin_ineq_coeffs, cvScaleMultipliers, 
			  linearIneqScaleMultipliers) );

  if (outputLevel > NORMAL_OUTPUT && numLinearIneqConstraints > 0)
    print_scaling("Linear inequality scales (incl. any variable scaling)",
		  linearIneqScaleTypes, linearIneqScaleMultipliers,
		  linearIneqScaleOffsets, StringArray());

  // ---------------
  // LINEAR EQUALITY
  // ---------------
  const StringArray& lin_eq_scale_strings
    = probDescDB.get_sa("method.linear_equality_scale_types");
  const RealVector& lin_eq_scales
    = probDescDB.get_rv("method.linear_equality_scales");

  if ( ( lin_eq_scale_strings.size() != 0 && lin_eq_scale_strings.size() != 1 &&
	 lin_eq_scale_strings.size() != numLinearEqConstraints ) ||
       ( lin_eq_scales.length() != 0 && lin_eq_scales.length() != 1 && 
	 lin_eq_scales.length() != numLinearEqConstraints ) ) {
    Cerr << "Error: linear_equality_scale specifications must have length 0, "
	 << "1, or " << numLinearEqConstraints << ".\n";
    abort_handler(-1);
  }

  linearEqScaleOffsets.resize(numLinearEqConstraints);

  lbs.size(0); ubs.size(0);
  targets = sub_model.linear_eq_constraint_targets();

  const RealMatrix& lin_eq_coeffs
    = sub_model.linear_eq_constraint_coeffs();
  for (int i=0; i<numLinearEqConstraints; ++i) {
    // compute A_i*cvScaleOffset for current constraint
    linearEqScaleOffsets[i] = 0.0;
    for (int j=0; j<numContinuousVars; ++j)
      linearEqScaleOffsets[i] += lin_eq_coeffs(i,j)*cvScaleOffsets[j];
   
    targets[i] -= linearEqScaleOffsets[i];
  }
  compute_scaling(LINEAR, TARGET, numLinearEqConstraints,
		  lbs, ubs, targets, lin_eq_scale_strings, lin_eq_scales,
		  linearEqScaleTypes, linearEqScaleMultipliers, 
		  tmp_offsets);

  iteratedModel.linear_eq_constraint_targets(targets);
  iteratedModel.linear_eq_constraint_coeffs(
    lin_coeffs_modify_n2s(lin_eq_coeffs, cvScaleMultipliers, 
			  linearEqScaleMultipliers) );

  if (outputLevel > NORMAL_OUTPUT && numLinearEqConstraints > 0)
    print_scaling("Linear equality scales (incl. any variable scaling)",
		  linearEqScaleTypes, linearEqScaleMultipliers,
		  linearEqScaleOffsets, StringArray());

  if (outputLevel > NORMAL_OUTPUT)
    Cout << std::endl;
}


// compute_scaling will potentially modify lbs, ubs, and targets; will resize
// and set class data referenced by scale_types, scale_mults, and scale_offsets
void Minimizer::
compute_scaling(int object_type, // type of object being scaled 
		int auto_type,   // option for auto scaling type
		int num_vars,    // length of object being scaled
		RealVector& lbs,     RealVector& ubs,
		RealVector& targets, const StringArray& scale_strings,
		const RealVector& scales, IntArray& scale_types,
		RealVector& scale_mults,  RealVector& scale_offsets)
{
  // temporary arrays
  String tmp_scl_str; 
  Real tmp_bound, tmp_mult, tmp_offset;
  //RealMatrix linear_constraint_coeffs;

  const int num_scale_strings = scale_strings.size();
  const int num_scales        = scales.length();
  
  scale_types.resize(num_vars);
  scale_mults.resize(num_vars);
  scale_offsets.resize(num_vars);

  for (int i=0; i<num_vars; ++i) {

    //set defaults
    scale_types[i]   = SCALE_NONE;
    scale_mults[i]   = 1.0;
    scale_offsets[i] = 0.0;

    // set the string for scale_type, depending on whether user sent
    // 0, 1, or N scale_strings
    tmp_scl_str = "none";
    if (num_scale_strings == 1)
      tmp_scl_str = scale_strings[0];
    else if (num_scale_strings > 1)
      tmp_scl_str = scale_strings[i];

    if (tmp_scl_str != "none") {

      if (  object_type == CDV && numLinearConstraints > 0 &&
	    tmp_scl_str == "log" ) {
	Cerr << "Error: Continuous design variables cannot be logarithmically "
	     << "scaled when linear\nconstraints are present.\n";
	abort_handler(-1);
      } 
      else if ( num_scales > 0 ) {
	
	// process scale_values for all types of scaling 
	// indicate that scale values are active, update bounds, poss. negating
	scale_types[i] |= SCALE_VALUE;
	scale_mults[i] = (num_scales == 1) ? scales[0] : scales[i];
	if (std::fabs(scale_mults[i]) < SCALING_MIN_SCALE)
	  Cout << "Warning: abs(scale) < " << SCALING_MIN_SCALE
	       << " provided; carefully verify results.\n";
	// adjust bounds or targets
	if (!lbs.empty()) {
	  // don't scale bounds if the user intended no bound
	  if (-bigRealBoundSize < lbs[i])
	    lbs[i] /= scale_mults[i];
	  if (ubs[i] < bigRealBoundSize)
	    ubs[i] /= scale_mults[i];
	  if (scale_mults[i] < 0) {
	    tmp_bound = lbs[i];
	    lbs[i] = ubs[i];
	    ubs[i] = tmp_bound;
	  }
	} 
	else if (!targets.empty())
	  targets[i] /= scale_mults[i];
      }

    } // endif for generic scaling preprocessing

    // At this point bounds/targets are scaled with user-provided values and
    // scale_mults are set to user-provided values.
    // Now auto or log scale as relevant and allowed:
    if ( tmp_scl_str == "auto" && auto_type > DISALLOW ) {
      bool scale_flag = false; // will be true for valid auto-scaling
      if ( auto_type == TARGET ) {
	scale_flag = compute_scale_factor(targets[i], &tmp_mult);
	tmp_offset = 0.0;
      }
      else if (auto_type == BOUNDS )
	scale_flag = compute_scale_factor(lbs[i], ubs[i], 
					  &tmp_mult, &tmp_offset);
      if (scale_flag) {

	scale_types[i] |= SCALE_VALUE;
	// tmp_offset was calculated based on scaled bounds, so
	// includes the effect of user scale values, so in computing
	// the offset, need to include the effect of any user-supplied
	// characteristic value scaling, then update multipliers
	scale_offsets[i] += tmp_offset*scale_mults[i];
	scale_mults[i] *= tmp_mult;  
      
	// necessary since the initial values may have already been value scaled
	if (auto_type == BOUNDS) {
	  // don't scale bounds if the user intended no bound
	  if (-bigRealBoundSize < lbs[i])
	    lbs[i] = (lbs[i] - tmp_offset)/tmp_mult;
	  if (ubs[i] < bigRealBoundSize)
	    ubs[i] = (ubs[i] - tmp_offset)/tmp_mult;
	}
	else if (auto_type == TARGET)
	  targets[i] /= tmp_mult;

      }
    }
    else if ( tmp_scl_str == "log" ) {

      scale_types[i] |= SCALE_LOG;
      if (auto_type == BOUNDS) {
	if (-bigRealBoundSize < lbs[i]) {
	  if ( lbs[i] < SCALING_MIN_LOG )
	    Cout << "Warning: scale_type 'log' used without positive lower "
		 << "bound.\n";
	  lbs[i] = std::log(lbs[i])/SCALING_LN_LOGBASE;
	}
	if (ubs[i] < bigRealBoundSize) {
	  if ( ubs[i] < SCALING_MIN_LOG )
	    Cout << "Warning: scale_type 'log' used without positive upper "
		 << "bound.\n";
	  ubs[i] = std::log(ubs[i])/SCALING_LN_LOGBASE;
	}
      }
      else if (auto_type == TARGET) {
	targets[i] = std::log(targets[i])/SCALING_LN_LOGBASE;
	if ( targets[i] < SCALING_MIN_LOG )
	  Cout << "Warning: scale_type 'log' used without positive target.\n";
      }
    }

  } // end for each variable
}


/** Difference the primary responses with observed data */
void Minimizer::
primary_resp_differencer(const Variables& raw_vars, 
			 const Variables& residual_vars,
			 const Response& raw_response, 
			 Response& residual_response)
{
  // data differencing doesn't affect gradients and Hessians, as long
  // as they use the updated residual in their computation.  They probably don't!

  if (minimizerInstance->outputLevel > NORMAL_OUTPUT) {
    Cout << "\n-----------------------------------------------------------";
    Cout << "\nPost-processing Function Evaluation: Data Transformation";
    Cout << "\n-----------------------------------------------------------" 
	 << std::endl;
  }

  minimizerInstance->data_difference_core(raw_response, residual_response);

  if (minimizerInstance->outputLevel > NORMAL_OUTPUT && 
      minimizerInstance->numUserPrimaryFns > 0) {
    Cout << "Least squares data transformation:\n";
    write_data(Cout, residual_response.function_values(),
	       residual_response.function_labels());
    Cout << std::endl;
  }
  if (minimizerInstance->outputLevel == DEBUG_OUTPUT && 
      minimizerInstance->numUserPrimaryFns > 0) {
    size_t numTotalCalibTerms = 
      minimizerInstance-> expData.num_total_exppoints();
    const ShortArray& asv = residual_response.active_set_request_vector();
    for (size_t i=0; i < numTotalCalibTerms; i++) {
      if (asv[i] & 1) 
        Cout << " residual_response function " << i << ' ' 
	     << residual_response.function_value(i) << '\n';
      if (asv[i] & 2) 
        Cout << " residual_response gradient " << i << ' ' 
	     << residual_response.function_gradient_view(i) << '\n';
      if (asv[i] & 4) 
        Cout << " residual_response hessian " << i << ' ' 
	     << residual_response.function_hessian(i) << '\n';
    }
  }
}

void Minimizer::
data_difference_core(const Response& raw_response, Response& residual_response) 
{
  ShortArray total_asv;
  bool interrogate_field_data = 
    ( ( matrixCovarianceActive ) || ( expData.interpolate_flag() ) );
  total_asv = expData.determine_active_request(residual_response, 
					       interrogate_field_data);
  expData.form_residuals(raw_response, total_asv, residual_response);
  if (applyCovariance) 
    expData.scale_residuals(residual_response, total_asv);
}

/** Variables map from iterator/scaled space to user/native space
    using a RecastModel. */
void Minimizer::
variables_scaler(const Variables& scaled_vars, Variables& native_vars)
{
  if (minimizerInstance->outputLevel > NORMAL_OUTPUT) {
    Cout << "\n----------------------------------";
    Cout << "\nPre-processing Function Evaluation";
    Cout << "\n----------------------------------";
    Cout << "\nVariables before scaling transformation:\n";
    write_data(Cout, scaled_vars.continuous_variables(),
      scaled_vars.continuous_variable_labels());
    Cout << std::endl;
  }
  native_vars.continuous_variables(minimizerInstance->modify_s2n(
    scaled_vars.continuous_variables(),    minimizerInstance->cvScaleTypes,
    minimizerInstance->cvScaleMultipliers, minimizerInstance->cvScaleOffsets));
}


/** For Gauss-Newton Hessian requests, activate the 2 bit and mask the 4 bit. */
void Minimizer::
gnewton_set_recast(const Variables& recast_vars, const ActiveSet& recast_set,
		   ActiveSet& sub_model_set)
{
  // AUGMENT standard mappings in RecastModel::set_mapping()
  const ShortArray& sub_model_asv = sub_model_set.request_vector();
  size_t i, num_sm_fns = sub_model_asv.size();
  for (i=0; i<num_sm_fns; ++i)
    if (sub_model_asv[i] & 4) { // add 2 bit and remove 4 bit
      short sm_asv_val = ( (sub_model_asv[i] | 2) & 3);
      sub_model_set.request_value(sm_asv_val, i);
    }
}

void Minimizer::
replicate_set_recast(const Variables& recast_vars, const ActiveSet& recast_set,
		   ActiveSet& sub_model_set)
{
  // BMA: IS this actually needed as an override?!?
  // AUGMENT standard mappings in RecastModel::set_mapping()
  const ShortArray& sub_model_asv = sub_model_set.request_vector();
  size_t i,j, num_sm_fns = sub_model_asv.size();
  size_t inner_size = recast_set.request_vector().size()/num_sm_fns;
  ShortArray new_asv(num_sm_fns,0);
  for (i=0; i<num_sm_fns; ++i) {
    for (j=0; j<inner_size; ++j)  
      new_asv[i] |= recast_set.request_vector()[i*inner_size+j];
    sub_model_set.request_value(new_asv[i],i);
  }
  sub_model_set.derivative_vector(recast_set.derivative_vector());
}

void Minimizer::
primary_resp_scaler(const Variables& native_vars, const Variables& scaled_vars,
		    const Response& native_response, Response& iterator_response)
{
  if (minimizerInstance->outputLevel > NORMAL_OUTPUT) {
    Cout << "\n-----------------------------------------------------------";
    Cout << "\nPost-processing Function Evaluation: Scaling Transformation";
    Cout << "\n-----------------------------------------------------------" 
	 << std::endl;
  }

  // scaling is always applied on a model with user's original size
  minimizerInstance->
    response_scaler_core(native_vars, scaled_vars, native_response, 
			 iterator_response, 0, 
			 minimizerInstance->numUserPrimaryFns);
}




/** Constraint function map from user/native space to iterator/scaled/combined
    space using a RecastModel. */
void Minimizer::
secondary_resp_scaler(const Variables& native_vars,
		      const Variables& scaled_vars,
		      const Response& native_response,
		      Response& iterator_response)
{
  // need to scale if secondary responses are scaled or (variables are
  // scaled and grad or hess requested)
  // scaling is always applied on a model with user's original size
  minimizerInstance->
    response_scaler_core(native_vars, scaled_vars, native_response, 
			 iterator_response, 
			 minimizerInstance->numUserPrimaryFns,
			 minimizerInstance->numNonlinearConstraints);
}

void Minimizer::
response_scaler_core(const Variables& native_vars,
		     const Variables& scaled_vars,
		     const Response& native_response,
		     Response& iterator_response,
		     size_t start_offset, size_t num_responses)
{
  // need to scale if primary responses are scaled or (variables are
  // scaled and grad or hess requested)
  bool scale_transform_needed = minimizerInstance->primaryRespScaleFlag ||
    minimizerInstance->need_resp_trans_byvars(
      native_response.active_set_request_vector(), start_offset,
      num_responses);

  if (scale_transform_needed)
    minimizerInstance->response_modify_n2s(native_vars, native_response,
      iterator_response, start_offset, num_responses);
  else
    // could reach this if variables are scaled and only functions are requested
    iterator_response.update_partial(start_offset, num_responses,
				     native_response, start_offset);
}


/** Constraint function map from user/native space to iterator/scaled/combined
    space using a RecastModel. */
void Minimizer::
secondary_resp_copier(const Variables& input_vars,
		      const Variables& output_vars,
		      const Response& input_response,
		      Response& output_response)
{
  // in cases where we reduce or data transform, only need to topu
  // TODO: fix use of numIter here!!!

  output_response.update_partial(minimizerInstance->numIterPrimaryFns,
    minimizerInstance->numNonlinearConstraints, input_response,
    minimizerInstance->numUserPrimaryFns);
}


/** Determine if variable transformations present and derivatives
    requested, which implies a response transformation is necessay */ 
bool Minimizer::
need_resp_trans_byvars(const ShortArray& asv, int start_index, int num_resp) 
{
  if (varsScaleFlag)
    for (size_t i=start_index; i<start_index+num_resp; ++i)
      if (asv[i] & 2 || asv[i] & 4)
	return(true);
  
  return(false);
}

/** general RealVector mapping from native to scaled variables; loosely,
    in greatest generality:
    scaled_var = log( (native_var - offset) / multiplier ) */
RealVector Minimizer::
modify_n2s(const RealVector& native_vars, const IntArray& scale_types,
	   const RealVector& multipliers, const RealVector& offsets) const
{
  RealVector scaled_vars(native_vars.length(), false);
  for (int i=0; i<native_vars.length(); ++i) {

    if (scale_types[i] & SCALE_VALUE)
      scaled_vars[i] = (native_vars[i] - offsets[i]) / multipliers[i];
    else 
      scaled_vars[i] = native_vars[i];

    if (scale_types[i] & SCALE_LOG)
      scaled_vars[i] = std::log(scaled_vars[i])/SCALING_LN_LOGBASE;

  }

  return(scaled_vars);
}

/** general RealVector mapping from scaled to native variables and/or vals;
    loosely, in greatest generality:
    scaled_var = (LOG_BASE^scaled_var) * multiplier + offset */
RealVector Minimizer::
modify_s2n(const RealVector& scaled_vars, const IntArray& scale_types,
	   const RealVector& multipliers, const RealVector& offsets) const
{
  RealVector native_vars(scaled_vars.length(), false);
  for (RealVector::ordinalType i=0; i<scaled_vars.length(); ++i) {

    if (scale_types[i] & SCALE_LOG)
      native_vars[i] = std::pow(SCALING_LOGBASE, scaled_vars[i]);
    else 
      native_vars[i] = scaled_vars[i];

    if (scale_types[i] & SCALE_VALUE)
      native_vars[i] = native_vars[i]*multipliers[i] + offsets[i];
    
  }

  return(native_vars);
}


/** Scaling response mapping: modifies response from a model
    (user/native) for use in iterators (scaled). Maps num_responses
    starting at response_offset */
void Minimizer::response_modify_n2s(const Variables& native_vars,
				    const Response& native_response,
				    Response& recast_response,
				    int start_offset,
				    int num_responses) const
{
  int i, j, k;
  int end_offset = start_offset + num_responses;
  SizetMultiArray var_ids;
  RealVector cdv;

  // unroll the unscaled (native/user-space) response
  const ActiveSet&  set = native_response.active_set();
  const ShortArray& asv = set.request_vector();
  const SizetArray& dvv = set.derivative_vector();
  size_t num_deriv_vars = dvv.size(); 
 
  if (dvv == native_vars.continuous_variable_ids()) {
    var_ids.resize(boost::extents[native_vars.cv()]);
    var_ids = native_vars.continuous_variable_ids();
    cdv = native_vars.continuous_variables(); // view OK
  }
  else if (dvv == native_vars.inactive_continuous_variable_ids()) {
    var_ids.resize(boost::extents[native_vars.icv()]);
    var_ids = native_vars.inactive_continuous_variable_ids();
    cdv = native_vars.inactive_continuous_variables(); // view OK
  }
  else { // general derivatives
    var_ids.resize(boost::extents[native_vars.acv()]);
    var_ids = native_vars.all_continuous_variable_ids();
    cdv = native_vars.all_continuous_variables(); // view OK
  }

  if (outputLevel > NORMAL_OUTPUT)
    if (start_offset < numUserPrimaryFns)
      Cout << "Primary response scaling transformation:\n";
    else
      Cout << "Secondary response scaling transformation:\n";

  // scale functions and constraints
  // assumes Multipliers and Offsets are 1 and 0 when not in use
  // there's a tradeoff here between flops and logic simplicity
  // (responseScaleOffsets may be nonzero for constraints)
  // iterate over components of ASV-requested functions and scale when necessary
  Real recast_val;
  const RealVector&  native_vals   = native_response.function_values();
  const StringArray& recast_labels = recast_response.function_labels();
  for (i = start_offset; i < end_offset; ++i)
    if (asv[i] & 1) {
      // SCALE_LOG case here includes case of SCALE_LOG && SCALE_VALUE
      if (responseScaleTypes[i] & SCALE_LOG)
	recast_val = std::log( (native_vals[i] - responseScaleOffsets[i]) / 
	  responseScaleMultipliers[i] )/SCALING_LN_LOGBASE; 
      else if (responseScaleTypes[i] & SCALE_VALUE)
	recast_val = (native_vals[i] - responseScaleOffsets[i]) / 
	  responseScaleMultipliers[i]; 
      else
	recast_val = native_vals[i];
      recast_response.function_value(recast_val, i);
      if (outputLevel > NORMAL_OUTPUT)
	Cout << "                     " << std::setw(write_precision+7) 
	     << recast_val << ' ' << recast_labels[i] << '\n';
    }

  // scale gradients
  const RealMatrix& native_grads = native_response.function_gradients();
  for (i = start_offset; i < end_offset; ++i)
    if (asv[i] & 2) {

      Real fn_divisor;
      if (responseScaleTypes[i] & SCALE_LOG)
	fn_divisor = (native_vals[i] - responseScaleOffsets[i]) *
	  SCALING_LN_LOGBASE;
      else if (responseScaleTypes[i] & SCALE_VALUE)
	fn_divisor = responseScaleMultipliers[i];
      else
	fn_divisor = 1.;

      RealVector recast_grad
	= recast_response.function_gradient_view(i);
      copy_data(native_grads[i], (int)num_deriv_vars, recast_grad);
      for (j=0; j<num_deriv_vars; ++j) {
	size_t xj_index = find_index(var_ids, dvv[j]);

	// first multiply by d(f_scaled)/d(f) based on scaling type
	recast_grad[xj_index] /= fn_divisor;

	// now multiply by d(x)/d(x_scaled)
	if (cvScaleTypes[xj_index] & SCALE_LOG)
	  recast_grad[xj_index] *= 
	    (cdv[xj_index] - cvScaleOffsets[xj_index]) * SCALING_LN_LOGBASE;
	else if (cvScaleTypes[xj_index] & SCALE_VALUE)
	  recast_grad[xj_index] *= cvScaleMultipliers[xj_index];
      }
      if (outputLevel > NORMAL_OUTPUT) {
	write_col_vector_trans(Cout, i, true, true, false, 
			       recast_response.function_gradients());
	Cout << recast_labels[i] << " gradient\n";
      }
    }
  
  // scale hessians
  const RealSymMatrixArray& native_hessians
    = native_response.function_hessians();
  for (i = start_offset; i < end_offset; ++i)
    if (asv[i] & 4) {
      RealSymMatrix recast_hess
	= recast_response.function_hessian_view(i);
      recast_hess.assign(native_hessians[i]);

      Real offset_fn = 1.;
      if (responseScaleTypes[i] & SCALE_LOG)
	offset_fn = native_vals[i] - responseScaleOffsets[i];
      for (j=0; j<num_deriv_vars; ++j) {
	size_t xj_index = find_index(var_ids, dvv[j]);
	for (k=0; k<=j; ++k) {
	  size_t xk_index = find_index(var_ids, dvv[k]);

	  // first multiply by d(f_scaled)/d(f) based on scaling type
	  if (responseScaleTypes[i] & SCALE_LOG) {

	    recast_hess(xj_index,xk_index) -= 
	      native_grads(xj_index,i)*native_grads(xk_index,i) / offset_fn;

	    recast_hess(xj_index,xk_index) /= offset_fn*SCALING_LN_LOGBASE;

	  }
	  else if (responseScaleTypes[i] & SCALE_VALUE)
	    recast_hess(xj_index,xk_index) /= responseScaleMultipliers[i];

	  // now multiply by d(x)/d(x_scaled) for j,k
	  if (cvScaleTypes[xj_index] & SCALE_LOG)
	    recast_hess(xj_index,xk_index) *= 
	      (cdv[xj_index] - cvScaleOffsets[xj_index]) * SCALING_LN_LOGBASE;
	  else if (cvScaleTypes[xj_index] & SCALE_VALUE)
	    recast_hess(xj_index,xk_index) *= cvScaleMultipliers[xj_index];

	  if (cvScaleTypes[xk_index] & SCALE_LOG)
	    recast_hess(xj_index,xk_index) *= 
	      (cdv[xk_index] - cvScaleOffsets[xk_index]) * SCALING_LN_LOGBASE;
	  else if (cvScaleTypes[xk_index] & SCALE_VALUE)
	    recast_hess(xj_index,xk_index) *= cvScaleMultipliers[xk_index];

	  // need gradient term only for diagonal entries
	  if (xj_index == xk_index && cvScaleTypes[xj_index] & SCALE_LOG)
	    if (responseScaleTypes[i] & SCALE_LOG)
	      recast_hess(xj_index,xk_index) += native_grads(xj_index,i)*
		(cdv[xj_index] - cvScaleOffsets[xj_index]) *
		SCALING_LN_LOGBASE / offset_fn;
	    else
	      recast_hess(xj_index,xk_index) += native_grads(xj_index,i)*
		(cdv[xj_index] - cvScaleOffsets[xj_index]) * SCALING_LN_LOGBASE
		* SCALING_LN_LOGBASE / responseScaleMultipliers[i];

	}
      }
      if (outputLevel > NORMAL_OUTPUT) {
	write_data(Cout, recast_hess, true, true, false);
	Cout << recast_labels[i] << " Hessian\n";
      }
    }
  
  if (outputLevel > NORMAL_OUTPUT)
    Cout << std::endl;
}

/** Unscaling response mapping: modifies response from scaled
    (iterator) to native (user) space.  Maps num_responses starting at
    response_offset */
void Minimizer::response_modify_s2n(const Variables& native_vars,
				    const Response& scaled_response,
				    Response& native_response,
				    int start_offset,
				    int num_responses) const
{
  using std::pow;

  int i, j, k;
  int end_offset = start_offset + num_responses;
  SizetMultiArray var_ids;
  RealVector cdv;

  // unroll the unscaled (native/user-space) response
  const ActiveSet&  set = scaled_response.active_set();
  const ShortArray& asv = set.request_vector();
  const SizetArray& dvv = set.derivative_vector();
  size_t num_deriv_vars = dvv.size(); 
 
  if (dvv == native_vars.continuous_variable_ids()) {
    var_ids.resize(boost::extents[native_vars.cv()]);
    var_ids = native_vars.continuous_variable_ids();
    cdv = native_vars.continuous_variables(); // view OK
  }
  else if (dvv == native_vars.inactive_continuous_variable_ids()) {
    var_ids.resize(boost::extents[native_vars.icv()]);
    var_ids = native_vars.inactive_continuous_variable_ids();
    cdv = native_vars.inactive_continuous_variables(); // view OK
  }
  else {    // general derivatives
    var_ids.resize(boost::extents[native_vars.acv()]);
    var_ids = native_vars.all_continuous_variable_ids();
    cdv = native_vars.all_continuous_variables(); // view OK
  }

  if (outputLevel > NORMAL_OUTPUT)
    if (start_offset < numUserPrimaryFns)
      Cout << "Primary response unscaling transformation:\n";
    else
      Cout << "Secondary response unscaling transformation:\n";

  // scale functions and constraints
  // assumes Multipliers and Offsets are 1 and 0 when not in use
  // there's a tradeoff here between flops and logic simplicity
  // (responseScaleOffsets may be nonzero for constraints)
  // iterate over components of ASV-requested functions and scale when necessary
  Real native_val;
  const RealVector&  scaled_vals   = scaled_response.function_values();
  const StringArray& native_labels = native_response.function_labels();
  for (i = start_offset; i < end_offset; ++i)
    if (asv[i] & 1) {
      // SCALE_LOG case here includes case of SCALE_LOG && SCALE_VALUE
      if (responseScaleTypes[i] & SCALE_LOG)
	native_val = pow(SCALING_LOGBASE, scaled_vals[i]) *
	  responseScaleMultipliers[i] + responseScaleOffsets[i];
 
      else if (responseScaleTypes[i] & SCALE_VALUE)
	native_val = scaled_vals[i]*responseScaleMultipliers[i] + 
	  responseScaleOffsets[i];
      else
	native_val = scaled_vals[i];
      native_response.function_value(native_val, i);
      if (outputLevel > NORMAL_OUTPUT)
	Cout << "                     " << std::setw(write_precision+7) 
	     << native_val << ' ' << native_labels[i] << '\n';
    }

  // scale gradients
  Real df_dfscl;
  const RealMatrix& scaled_grads = scaled_response.function_gradients();
  for (i = start_offset; i < end_offset; ++i)
    if (asv[i] & 2) {

      if (responseScaleTypes[i] & SCALE_LOG)
	df_dfscl = pow(SCALING_LOGBASE, scaled_vals[i]) * SCALING_LN_LOGBASE *
	  responseScaleMultipliers[i];	 
      else if (responseScaleTypes[i] & SCALE_VALUE)
	df_dfscl = responseScaleMultipliers[i];
      else
	df_dfscl = 1.;

      RealVector native_grad
	= native_response.function_gradient_view(i);
      copy_data(scaled_grads[i], (int)num_deriv_vars, native_grad);
      for (j=0; j<num_deriv_vars; ++j) {
	size_t xj_index = find_index(var_ids, dvv[j]);

	// first multiply by d(f)/d(f_scaled) based on scaling type
	native_grad[xj_index] *= df_dfscl; 

	// now multiply by d(x_scaled)/d(x)
	if (cvScaleTypes[xj_index] & SCALE_LOG)
	  native_grad[xj_index] /= (cdv[xj_index] - cvScaleOffsets[xj_index]) *
	    SCALING_LN_LOGBASE;
	else if (cvScaleTypes[xj_index] & SCALE_VALUE)
	  native_grad[xj_index] /= cvScaleMultipliers[xj_index];
      }
      if (outputLevel > NORMAL_OUTPUT) {
	write_col_vector_trans(Cout, i, true, true, false, 
			       native_response.function_gradients());
	Cout << native_labels[i] << " gradient\n";
      }
    }
  
  // scale hessians
  const RealSymMatrixArray& scaled_hessians
    = scaled_response.function_hessians();
  for (i = start_offset; i < end_offset; ++i)
    if (asv[i] & 4) {
 
     if (responseScaleTypes[i] & SCALE_LOG)
	df_dfscl = pow(SCALING_LOGBASE, scaled_vals[i]) * SCALING_LN_LOGBASE *
	  responseScaleMultipliers[i];
      else if (responseScaleTypes[i] & SCALE_VALUE)
	df_dfscl = responseScaleMultipliers[i];
      else
	df_dfscl = 1.;

      RealSymMatrix native_hess
	= native_response.function_hessian_view(i);
      native_hess.assign(scaled_hessians[i]);
      for (j=0; j<num_deriv_vars; ++j) {
	size_t xj_index = find_index(var_ids, dvv[j]);
	for (k=0; k<=j; ++k) {
	  size_t xk_index = find_index(var_ids, dvv[k]);
	  
	  // first multiply by d(f_scaled)/d(f) based on scaling type

	  if (responseScaleTypes[i] & SCALE_LOG) {

	    native_hess(xj_index,xk_index) += scaled_grads(xj_index,i) *
	      scaled_grads(xk_index,i) * SCALING_LN_LOGBASE; 

	    native_hess(xj_index,xk_index) *= df_dfscl; 

	  }
	  else if (responseScaleTypes[i] & SCALE_VALUE)
	    native_hess(xj_index,xk_index) *= df_dfscl;

	  // now multiply by d(x_scaled)/d(x) for j,k
	  if (cvScaleTypes[xj_index] & SCALE_LOG)
	    native_hess(xj_index,xk_index) /= 
	      (cdv[xj_index] - cvScaleOffsets[xj_index]) * SCALING_LN_LOGBASE;
	  else if (cvScaleTypes[xj_index] & SCALE_VALUE)
	    native_hess(xj_index,xk_index) /= cvScaleMultipliers[xj_index];

	  if (cvScaleTypes[xk_index] & SCALE_LOG)
	    native_hess(xj_index,xk_index) /= 
	      (cdv[xk_index] - cvScaleOffsets[xk_index]) * SCALING_LN_LOGBASE;
	  else if (cvScaleTypes[xk_index] & SCALE_VALUE)
	    native_hess(xj_index,xk_index) /= cvScaleMultipliers[xk_index];

	  // need gradient term only for diagonal entries
	  if (xj_index == xk_index && cvScaleTypes[xj_index] & SCALE_LOG)
	    native_hess(xj_index,xk_index) -=
	      df_dfscl * scaled_grads(xj_index,i) /
	      (cdv[xj_index] - cvScaleOffsets[xj_index]) * 
	      (cdv[xj_index] - cvScaleOffsets[xj_index]) * SCALING_LN_LOGBASE;

	}
      }
      if (outputLevel > NORMAL_OUTPUT) {
	write_data(Cout, native_hess, true, true, false);
	Cout << native_labels[i] << " Hessian\n";
      }
    }
  
  if (outputLevel > NORMAL_OUTPUT)
    Cout << std::endl;
}


// automatically compute scaling factor
// bounds case allows for negative multipliers
// returns true if a valid scaling factor was computed
bool Minimizer::compute_scale_factor(const Real lower_bound,
				     const Real upper_bound,
				     Real *multiplier, Real *offset)
{
  /*  Compute scaleMultipliers for each design var, fn, constr, etc.
      1. user-specified scaling was already detected at higher level
      2. check for two-sided bounds
      3. then check for one-sided bounds
      4. else resort to no scaling

      Auto-scaling is to [0,1] (affine scaling) in two sided case 
      or value of bound in one-sided case
  */

  bool lb_flag = false;
  bool ub_flag = false;

  if (-bigRealBoundSize < lower_bound)
    lb_flag = true;
  if (upper_bound < bigRealBoundSize)
    ub_flag = true;

  // process two-sided, then single-sided bounds
  if ( lb_flag && ub_flag ) {
    *multiplier = upper_bound - lower_bound;
    *offset = lower_bound;
  } 
  else if (lb_flag) {
    *multiplier = lower_bound;
    *offset = 0.0;
  } 
  else if (ub_flag) {
    *multiplier = upper_bound;
    *offset = 0.0;
  } 
  else {
    Cout << "Warning: abs(bounds) > bigRealBoundSize. Not auto-scaling "
	 << "component." << std::endl;
    *multiplier = 1.0;
    *offset = 0.0;
    return(false);
  }

  if (std::fabs(*multiplier) < SCALING_MIN_SCALE) {
    *multiplier = (*multiplier >= 0.0) ? SCALING_MIN_SCALE :
      -(SCALING_MIN_SCALE); 
    Cout << "Warning: in auto-scaling abs(computed scale) < " 
	 << SCALING_MIN_SCALE << "; resetting scale = " 
	 << *multiplier << ".\n";
  }

  return(true);
}


// automatically compute scaling factor
// target case allows for negative multipliers
// returns true if a valid scaling factor was computed
bool Minimizer::compute_scale_factor(const Real target, Real *multiplier)
{
  if ( std::fabs(target) < bigRealBoundSize ) 
    *multiplier = target;
  else {
    Cout << "Automatic Scaling Warning: abs(target) > bigRealBoundSize. "
	 << "Not scaling this component." << std::endl;
    *multiplier = 1.0;
    return(false);
  }

  if (std::fabs(*multiplier) < SCALING_MIN_SCALE) {
    *multiplier = (*multiplier >= 0.0) ? SCALING_MIN_SCALE :
      -(SCALING_MIN_SCALE); 
    Cout << "Warning: in auto-scaling abs(computed scale) < " 
	 << SCALING_MIN_SCALE << "; resetting scale = " 
	 << *multiplier << ".\n";
  }

  return(true);
}


/** compute scaled linear constraint matrix given design variable 
    multipliers and linear scaling multipliers.  Only scales components 
    corresponding to continuous variables so for src_coeffs of size MxN, 
    lin_multipliers.size() <= M, cv_multipliers.size() <= N */
RealMatrix Minimizer::
lin_coeffs_modify_n2s(const RealMatrix& src_coeffs,
		      const RealVector& cv_multipliers,
		      const RealVector& lin_multipliers) const
{
  RealMatrix dest_coeffs(src_coeffs);
  for (int i=0; i<lin_multipliers.length(); ++i)
    for (int j=0; j<cv_multipliers.length(); ++j)
      dest_coeffs(i,j) *= cv_multipliers[j] / lin_multipliers[i];

  return(dest_coeffs);
}


void Minimizer::print_scaling(const String& info, const IntArray& scale_types,
			      const RealVector& scale_mults,
			      const RealVector& scale_offsets,
			      const StringArray& labels)
{
  // labels will be empty for linear constraints
  Cout << "\n" << info << ":\n";
  Cout << "scale type " << std::setw(write_precision+7) << "multiplier" << " "
       << std::setw(write_precision+7) << "offset"
       << (labels.empty() ? " constraint number" : " label") << std::endl; 
  for (size_t i=0; i<scale_types.size(); ++i) {
    switch (scale_types[i]) {
    case SCALE_NONE: 
      Cout << "none       ";
      break;
    case SCALE_VALUE: 
      Cout << "value      ";
      break;
    case SCALE_LOG:
      Cout << "log        ";
      break;
    case (SCALE_VALUE | SCALE_LOG): 
      Cout << "value+log  ";
      break;
    }
    Cout << std::setw(write_precision+7) << scale_mults[i]   << " " 
	 << std::setw(write_precision+7) << scale_offsets[i] << " ";
    if (labels.empty())
      Cout << i << std::endl;
    else
      Cout << labels[i] << std::endl;
  }
}


/** The composite objective computation sums up the contributions from
    one of more primary functions using the primary response fn weights. */
Real Minimizer::
objective(const RealVector& fn_vals, const BoolDeque& max_sense,
	  const RealVector& primary_wts) const
{
  return objective(fn_vals, numUserPrimaryFns, max_sense, primary_wts);
}

/** This "composite" objective is a more general case of the previous
    objective(), but doesn't presume a reduction map from user to
    iterated space.  Used to apply weights and sense in COLIN results
    sorting.  Leaving as a duplicate implementation pending resolution
    of COLIN lookups. */
Real Minimizer::
objective(const RealVector& fn_vals, size_t num_fns,
	  const BoolDeque& max_sense,
	  const RealVector& primary_wts) const
{
  Real obj_fn = 0.0;
  if (optimizationFlag) { // MOO
    bool use_sense = !max_sense.empty();
    if (primary_wts.empty()) {
      for (size_t i=0; i<num_fns; ++i)
	if (use_sense && max_sense[i]) obj_fn -= fn_vals[i];
	else                           obj_fn += fn_vals[i];
      if (num_fns > 1)
	obj_fn /= (Real)num_fns; // default weight = 1/n
    }
    else {
      for (size_t i=0; i<num_fns; ++i)
	if (use_sense && max_sense[i]) obj_fn -= primary_wts[i] * fn_vals[i];
	else                           obj_fn += primary_wts[i] * fn_vals[i];
    }
  }
  else { // NLS
    if (primary_wts.empty())
      for (size_t i=0; i<num_fns; ++i)
	obj_fn += std::pow(fn_vals[i], 2); // default weight = 1
    else
      for (size_t i=0; i<num_fns; ++i)
    	obj_fn += primary_wts[i] * std::pow(fn_vals[i], 2);
  }
  return obj_fn;
}

void Minimizer::
objective_gradient(const RealVector& fn_vals, const RealMatrix& fn_grads, 
		   const BoolDeque& max_sense, const RealVector& primary_wts,
		   RealVector& obj_grad) const
{
  objective_gradient(fn_vals, numUserPrimaryFns, fn_grads, max_sense,
		     primary_wts, obj_grad);
}

/** The composite objective gradient computation combines the
    contributions from one of more primary function gradients,
    including the effect of any primary function weights.  In the case
    of a linear mapping (MOO), only the primary function gradients are
    required, but in the case of a nonlinear mapping (NLS), primary
    function values are also needed.  Within RecastModel::set_mapping(),
    the active set requests are automatically augmented to make values
    available when needed, based on nonlinearRespMapping settings. */
void Minimizer::
objective_gradient(const RealVector& fn_vals, size_t num_fns,
		   const RealMatrix& fn_grads, const BoolDeque& max_sense, 
		   const RealVector& primary_wts, RealVector& obj_grad) const
{
  if (obj_grad.length() != numContinuousVars)
    obj_grad.sizeUninitialized(numContinuousVars);
  obj_grad = 0.;
  if (optimizationFlag) { // MOO
    bool use_sense = !max_sense.empty();
    if (primary_wts.empty()) {
      for (size_t i=0; i<num_fns; ++i) {
	const Real* fn_grad_i = fn_grads[i];
	if (use_sense && max_sense[i])
	  for (size_t j=0; j<numContinuousVars; ++j)
	    obj_grad[j] -= fn_grad_i[j];
	else
	  for (size_t j=0; j<numContinuousVars; ++j)
	    obj_grad[j] += fn_grad_i[j];
      }
      if (num_fns > 1)
	obj_grad.scale(1./(Real)num_fns); // default weight = 1/n
    }
    else {
      for (size_t i=0; i<num_fns; ++i) {
	const Real& wt_i      = primary_wts[i];
	const Real* fn_grad_i = fn_grads[i];
	if (use_sense && max_sense[i])
	  for (size_t j=0; j<numContinuousVars; ++j)
	    obj_grad[j] -= wt_i * fn_grad_i[j];
	else
	  for (size_t j=0; j<numContinuousVars; ++j)
	    obj_grad[j] += wt_i * fn_grad_i[j];
      }
    }
  }
  else { // NLS
    for (size_t i=0; i<num_fns; ++i) {
      Real wt_2_fn_val = 2. * fn_vals[i]; // default weight = 1
      if (!primary_wts.empty()) wt_2_fn_val *= primary_wts[i];
      const Real* fn_grad_i = fn_grads[i];
      for (size_t j=0; j<numContinuousVars; ++j)
	obj_grad[j] += wt_2_fn_val * fn_grad_i[j];
    }
  }
}

void Minimizer::
objective_hessian(const RealVector& fn_vals, const RealMatrix& fn_grads, 
		  const RealSymMatrixArray& fn_hessians, 
		  const BoolDeque& max_sense, const RealVector& primary_wts,
		  RealSymMatrix& obj_hess) const
{
  objective_hessian(fn_vals, numUserPrimaryFns, fn_grads, fn_hessians, 
		    max_sense, primary_wts, obj_hess);
}


/** The composite objective Hessian computation combines the
    contributions from one of more primary function Hessians,
    including the effect of any primary function weights.  In the case
    of a linear mapping (MOO), only the primary function Hessians are
    required, but in the case of a nonlinear mapping (NLS), primary
    function values and gradients are also needed in general
    (gradients only in the case of a Gauss-Newton approximation).
    Within the default RecastModel::set_mapping(), the active set
    requests are automatically augmented to make values and gradients
    available when needed, based on nonlinearRespMapping settings. */
void Minimizer::
objective_hessian(const RealVector& fn_vals, size_t num_fns,
		  const RealMatrix& fn_grads, 
		  const RealSymMatrixArray& fn_hessians, 
		  const BoolDeque& max_sense, const RealVector& primary_wts,
		  RealSymMatrix& obj_hess) const
{
  if (obj_hess.numRows() != numContinuousVars)
    obj_hess.shapeUninitialized(numContinuousVars);
  obj_hess = 0.;
  size_t i, j, k;
  if (optimizationFlag) { // MOO
    bool use_sense = !max_sense.empty();
    if (primary_wts.empty()) {
      for (i=0; i<num_fns; ++i) {
	const RealSymMatrix& fn_hess_i = fn_hessians[i];
	if (use_sense && max_sense[i])
	  for (j=0; j<numContinuousVars; ++j)
	    for (k=0; k<=j; ++k)
	      obj_hess(j,k) -= fn_hess_i(j,k);
	else
	  for (j=0; j<numContinuousVars; ++j)
	    for (k=0; k<=j; ++k)
	      obj_hess(j,k) += fn_hess_i(j,k);
      }
      if (num_fns > 1)
	obj_hess *= 1./(Real)num_fns; // default weight = 1/n
    }
    else
      for (i=0; i<num_fns; ++i) {
	const RealSymMatrix& fn_hess_i = fn_hessians[i];
	const Real&               wt_i = primary_wts[i];
	if (use_sense && max_sense[i])
	  for (j=0; j<numContinuousVars; ++j)
	    for (k=0; k<=j; ++k)
	      obj_hess(j,k) -= wt_i * fn_hess_i(j,k);
	else
	  for (j=0; j<numContinuousVars; ++j)
	    for (k=0; k<=j; ++k)
	      obj_hess(j,k) += wt_i * fn_hess_i(j,k);
      }
  }
  else { // NLS
    if (fn_grads.empty()) {
      Cerr << "Error: Hessian reduction for NLS requires a minimum of least "
	   << "squares gradients (for Gauss-Newton)." << std::endl;
      abort_handler(-1);
    }
    // Gauss_Newton approx Hessian = J^T J (neglect f*H term)
    if (fn_hessians.empty() || fn_vals.empty()) {
      if (primary_wts.empty())
	for (j=0; j<numContinuousVars; ++j)
	  for (k=0; k<=j; ++k) {
	    Real& sum = obj_hess(j,k); sum = 0.;
	    for (i=0; i<num_fns; ++i)
	      sum += fn_grads(j,i) * fn_grads(k,i); // default weight = 1
	    sum *= 2.;
	  }
      else
	for (j=0; j<numContinuousVars; ++j)
	  for (k=0; k<=j; ++k) {
	    Real& sum = obj_hess(j,k); sum = 0.;
	    for (i=0; i<num_fns; ++i)
	      sum += primary_wts[i] * fn_grads(j,i) * fn_grads(k,i);
	    sum *= 2.;
	  }
    }
    // full Hessian = f H + J^T J
    else {
      if (primary_wts.empty())
	for (j=0; j<numContinuousVars; ++j)
	  for (k=0; k<=j; ++k) {
	    Real& sum = obj_hess(j,k); sum = 0.;
	    for (i=0; i<num_fns; ++i)
	      sum += fn_grads(j,i) * fn_grads(k,i) +
		fn_vals[i] * fn_hessians[i](j,k);
	    sum *= 2.;
	  }
      else
	for (j=0; j<numContinuousVars; ++j)
	  for (k=0; k<=j; ++k) {
	    Real& sum = obj_hess(j,k); sum = 0.;
	    for (i=0; i<num_fns; ++i)
	      sum += primary_wts[i] * (fn_grads(j,i) * fn_grads(k,i) +
				       fn_vals[i] * fn_hessians[i](j,k));
	    sum *= 2.;
	  }
    }
  }
}


void Minimizer::archive_allocate_best(size_t num_points)
{
  // allocate arrays for best data (stored as a set even if only one best set)
  if (numContinuousVars) {
    // labels
    resultsDB.insert
      (run_identifier(), resultsNames.cv_labels, 
       variables_results().continuous_variable_labels());
    // best variables, with labels in metadata
    MetaDataType md;
    md["Array Spans"] = make_metadatavalue("Best Sets");
    md["Row Labels"] = 
      make_metadatavalue(variables_results().continuous_variable_labels()); 
    resultsDB.array_allocate<RealVector>
      (run_identifier(), resultsNames.best_cv, num_points, md);
  }
  if (numDiscreteIntVars) {
    // labels
    resultsDB.insert
      (run_identifier(), resultsNames.div_labels, 
       variables_results().discrete_int_variable_labels());
    // best variables, with labels in metadata
    MetaDataType md;
    md["Array Spans"] = make_metadatavalue("Best Sets");
    md["Row Labels"] = 
      make_metadatavalue(variables_results().discrete_int_variable_labels()); 
    resultsDB.array_allocate<IntVector>
      (run_identifier(), resultsNames.best_div, num_points, md);
  }
  if (numDiscreteStringVars) {
    // labels
    resultsDB.insert
      (run_identifier(), resultsNames.dsv_labels, 
       variables_results().discrete_string_variable_labels());
    // best variables, with labels in metadata
    MetaDataType md;
    md["Array Spans"] = make_metadatavalue("Best Sets");
    md["Row Labels"] = 
      make_metadatavalue(variables_results().discrete_string_variable_labels()); 
    resultsDB.array_allocate<StringArray>
      (run_identifier(), resultsNames.best_dsv, num_points, md);
  }
  if (numDiscreteRealVars) {
    // labels
    resultsDB.insert
      (run_identifier(), resultsNames.drv_labels, 
       variables_results().discrete_real_variable_labels());
    // best variables, with labels in metadata
    MetaDataType md;
    md["Array Spans"] = make_metadatavalue("Best Sets");
    md["Row Labels"] = 
      make_metadatavalue(variables_results().discrete_real_variable_labels()); 
    resultsDB.array_allocate<RealVector>
      (run_identifier(), resultsNames.best_drv, num_points, md);
  }
  // labels
  resultsDB.insert
    (run_identifier(), resultsNames.fn_labels,
     response_results().function_labels());
  // best functions, with labels in metadata
  MetaDataType md;
  md["Array Spans"] = make_metadatavalue("Best Sets");
  md["Row Labels"] = 
    make_metadatavalue(response_results().function_labels());
  resultsDB.array_allocate<RealVector>
    (run_identifier(), resultsNames.best_fns, num_points, md);
}

void Minimizer::
archive_best(size_t point_index, 
	     const Variables& best_vars, const Response& best_resp)
{
  // archive the best point in the iterator database
  if (numContinuousVars)
    resultsDB.array_insert<RealVector>
      (run_identifier(), resultsNames.best_cv, point_index,
       best_vars.continuous_variables());
  if (numDiscreteIntVars)
    resultsDB.array_insert<IntVector>
      (run_identifier(), resultsNames.best_div, point_index,
       best_vars.discrete_int_variables());
  if (numDiscreteStringVars) {
    resultsDB.array_insert<StringArray>
      (run_identifier(), resultsNames.best_dsv, point_index,
       best_vars.discrete_string_variables());
  }
  if (numDiscreteRealVars)
    resultsDB.array_insert<RealVector>
      (run_identifier(), resultsNames.best_drv, point_index,
       best_vars.discrete_real_variables());
  resultsDB.array_insert<RealVector>
    (run_identifier(), resultsNames.best_fns, point_index,
     best_resp.function_values());
} 



/** Uses data from the innermost model, should any Minimizer recasts be active.
    Called by multipoint return solvers. Do not directly call resize on the 
    bestVariablesArray object unless you intend to share the internal content 
    (letter) with other objects after assignment. */
void Minimizer::resize_best_vars_array(size_t newsize) 
{
  size_t curr_size = bestVariablesArray.size();

  if(newsize < curr_size) {
    // If reduction in size, use the standard resize
    bestVariablesArray.resize(newsize);
  }
  else if(newsize > curr_size) {

    // Otherwise, we have to do the iteration ourselves so that we make use
    // of the model's current variables for envelope-letter requirements.

    // Best point arrays have be sized and scaled in the original user space.  
    Model usermodel = original_model();
    bestVariablesArray.reserve(newsize);
    for(size_t i=curr_size; i<newsize; ++i)
      bestVariablesArray.push_back(usermodel.current_variables().copy());
  }
  // else no size change

}

/** Uses data from the innermost model, should any Minimizer recasts be active.
    Called by multipoint return solvers. Do not directly call resize on the 
    bestResponseArray object unless you intend to share the internal content 
    (letter) with other objects after assignment. */
void Minimizer::resize_best_resp_array(size_t newsize) 
{
  size_t curr_size = bestResponseArray.size();

  if (newsize < curr_size) {
    // If reduction in size, use the standard resize
    bestResponseArray.resize(newsize);
  }
  else if (newsize > curr_size) {

    // Otherwise, we have to do the iteration ourselves so that we make use
    // of the model's current response for envelope-letter requirements.

    // Best point arrays have be sized and scaled in the original user space.  
    Model usermodel = original_model();
    bestResponseArray.reserve(newsize);
    for(size_t i=curr_size; i<newsize; ++i)
      bestResponseArray.push_back(usermodel.current_response().copy());
  }
  // else no size change
}


Real Minimizer::
sum_squared_residuals(size_t num_pri_fns, const RealVector& residuals, 
		      const RealVector& weights)
{
  if (!weights.empty() && num_pri_fns != weights.length()) {
    Cerr << "\nError (sum_squared_residuals): incompatible residual and weight "
	 << "lengths." << std::endl;
    abort_handler(-1);
  }

  // TODO: Just call reduce/objective on the data?
  Real t = 0.;
  for(size_t j=0; j<num_pri_fns; ++j) {
    const Real& t1 = residuals[j];
    if (weights.empty())
      t += t1*t1;
    else
      t += t1*t1*weights[j];
  }

  return t;
}


void Minimizer::print_model_resp(size_t num_pri_fns, const RealVector& best_fns,
				 size_t num_best, size_t best_index,
				 std::ostream& s)

{
  if (num_pri_fns > 1) s << "<<<<< Best model responses "; 
  else                 s << "<<<<< Best model response "; 
  if (num_best > 1) s << "(set " << best_index+1 << ") "; s << "=\n"; 
  write_data_partial(s, (size_t)0, num_pri_fns, best_fns); 
}


void Minimizer::print_residuals(size_t num_terms, const RealVector& best_terms, 
				const RealVector& weights, 
				size_t num_best, size_t best_index,
				std::ostream& s)
{
  // BMA TODO: if data and scaling are present, this won't print
  // correct weighted residual norms

  Real wssr = sum_squared_residuals(num_terms, best_terms, weights);

  s << "<<<<< Best residual norm ";
  if (num_best > 1) s << "(set " << best_index+1 << ") ";
  s << "= " << std::setw(write_precision+7)
    << std::sqrt(wssr) << "; 0.5 * norm^2 = " 
    << std::setw(write_precision+7) << 0.5*wssr << '\n';

  // Print best response functions
  if (num_terms > 1) s << "<<<<< Best residual terms "; 
  else               s << "<<<<< Best residual term  "; 
  if (num_best > 1) s << "(set " << best_index+1 << ") "; s << "=\n"; 
  write_data_partial(s, (size_t)0, num_terms, best_terms); 
}


/** Retrieve a MOO/NLS response based on the data returned by a single
    objective optimizer by performing a data_pairs search. This may
    get called even for a single user-specified function, since we may
    be recasting a single NLS residual into a squared
    objective. Always returns best data in the space of the original
    inbound Model. */
void Minimizer::
local_recast_retrieve(const Variables& vars, Response& response) const
{
  // TODO: could omit constraints for solvers populating them (there
  // may not exist a single DB eval with both functions, constraints)
  ActiveSet lookup_set(response.active_set());
  PRPCacheHIter cache_it
    = lookup_by_val(data_pairs, iteratedModel.interface_id(), vars, lookup_set);
  if (cache_it == data_pairs.get<hashed>().end())
    Cerr << "Warning: failure in recovery of final values for locally recast "
	 << "optimization." << std::endl;
  else
    response.update(cache_it->response());
}


} // namespace Dakota
