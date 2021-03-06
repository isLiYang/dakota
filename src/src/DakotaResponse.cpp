/*  _______________________________________________________________________

    DAKOTA: Design Analysis Kit for Optimization and Terascale Applications
    Copyright 2014 Sandia Corporation.
    This software is distributed under the GNU Lesser General Public License.
    For more information, see the README file in the top Dakota directory.
    _______________________________________________________________________ */

//- Class:        Response
//- Description:  Class implementation
//- Owner:        Mike Eldred

//#define REFCOUNT_DEBUG 1

#include "DakotaResponse.hpp"
#include "SimulationResponse.hpp"
#include "ExperimentResponse.hpp"
#include "DakotaVariables.hpp"
#include "ProblemDescDB.hpp"
#include "dakota_data_io.hpp"
#include <algorithm>
#include <boost/regex.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/export.hpp>

static const char rcsId[]="@(#) $Id: DakotaResponse.cpp 7029 2010-10-22 00:17:02Z mseldre $";

BOOST_CLASS_EXPORT(Dakota::Response)

namespace Dakota {


/** This constructor is the one which must build the base class data for all
    derived classes.  get_response() instantiates a derived class letter
    and the derived constructor selects this base class constructor in its 
    initialization list (to avoid the recursion of the base class constructor
    calling get_response() again).  Since the letter IS the representation, 
    its representation pointer is set to NULL (an uninitialized pointer causes
    problems in ~Response). */
Response::
Response(BaseConstructor, const Variables& vars,
	 const ProblemDescDB& problem_db):
  sharedRespData(problem_db), responseRep(NULL), referenceCount(1)
{
  // the derivative arrays must accomodate either active or inactive variables,
  // but the default is active variables.  Derivative arrays are resized if a
  // DVV of different length is set within responseActiveSet.
  size_t num_params = vars.cv(), num_fns = sharedRespData.num_functions();

  // Resize & initialize response data
  // Conserve memory by checking DB info prior to sizing grad/hessian arrays
  bool grad_flag = (problem_db.get_string("responses.gradient_type") != "none");
  bool hess_flag = (problem_db.get_string("responses.hessian_type")  != "none");
  functionValues.size(num_fns); // init to 0
  short asv_value = 1;
  if (grad_flag) {
    asv_value |= 2;
    functionGradients.reshape(num_params, num_fns);
    functionGradients = 0.;
  }
  if (hess_flag) {
    asv_value |= 4;
    functionHessians.resize(num_fns);
    for (size_t i=0; i<num_fns; i++) {
      functionHessians[i].reshape(num_params);
      functionHessians[i] = 0.;
    }
  }

  // set up the response ActiveSet.  This object is copied by Iterator for its
  // initial activeSet and used by Model for MPI buffer size estimation.
  //responseActiveSet.reshape(num_fns, num_params);
  ShortArray asv(num_fns, asv_value);
  responseActiveSet.request_vector(asv);
  responseActiveSet.derivative_vector(vars.continuous_variable_ids());
  
  const String& coord_file
    = problem_db.get_string("responses.coord_data_filename");
  if (!coord_file.empty() ) {
    RealMatrix coord_values;
    read_coord_values(coord_file,coord_values);
    //Cout << "coord_file " << coord_file << " coord_values:" << coord_values;
    field_coords(coord_values,0);
  } 

#ifdef REFCOUNT_DEBUG
  Cout << "Response::Response(BaseConstructor) called to build base class "
       << "data for letter object." << std::endl;
#endif
}


/** BaseConstructor must build the base class data for all derived
    classes.  This version is used for building a response object of
    the correct size on the fly (e.g., by slave analysis servers
    performing execute() on a local_response).
    SharedResponseData::functionLabels is not needed for MPI send/recv
    buffers, but is needed for NPSOLOptimizer's user-defined functions
    option to support I/O in bestResponseArray.front(). */
Response::
Response(BaseConstructor, const SharedResponseData& srd, const ActiveSet& set):
  sharedRespData(srd), responseActiveSet(set), responseRep(NULL),
  referenceCount(1)
{
  shape_rep(set);

#ifdef REFCOUNT_DEBUG
  Cout << "Response::Response(BaseConstructor) called to build base class "
       << "data for letter object." << std::endl;
#endif
}


/** BaseConstructor must build the base class data for all derived
    classes.  This version is used for building a response object of
    the correct size on the fly (e.g., by slave analysis servers
    performing execute() on a local_response).
    SharedResponseData::functionLabels is not needed for MPI send/recv
    buffers, but is needed for NPSOLOptimizer's user-defined functions
    option to support I/O in bestResponseArray.front(). */
Response::Response(BaseConstructor, const ActiveSet& set):
  sharedRespData(set), // minimal unshared definition
  responseActiveSet(set), responseRep(NULL), referenceCount(1)
{
  shape_rep(set);

#ifdef REFCOUNT_DEBUG
  Cout << "Response::Response(BaseConstructor) called to build base class "
       << "data for letter object." << std::endl;
#endif
}


/** BaseConstructor must build the base class data for all derived
    classes.  This version is used for building a response object of
    the correct size on the fly and sharing an instance of
    SharedResponseData. */
Response::Response(BaseConstructor, const SharedResponseData& srd):
  sharedRespData(srd), functionValues(srd.num_functions()),
  responseActiveSet(functionValues.length()), responseRep(NULL),
  referenceCount(1)
{
#ifdef REFCOUNT_DEBUG
  Cout << "Response::Response(BaseConstructor) called to build base class "
       << "data for letter object." << std::endl;
#endif
}


/** The default constructor: responseRep is NULL in this case (a populated
    problem_db is needed to build a meaningful Response object).  This
    makes it necessary to check for NULL in the copy constructor, assignment
    operator, and destructor. */
Response::Response(): responseRep(NULL), referenceCount(1)
{
#ifdef REFCOUNT_DEBUG
  Cout << "Response::Response() called to build empty response object."
       << std::endl;
#endif
}


/** This is the primary envelope constructor which uses problem_db to
    build a fully populated response object.  It only needs to
    extract enough data to properly execute get_response(problem_db),
    since the constructor overloaded with BaseConstructor builds the
    actual base class data inherited by the derived classes. */
Response::
Response(short type, const Variables& vars, const ProblemDescDB& problem_db):
  referenceCount(1) // not used since this is the envelope, not the letter
{
#ifdef REFCOUNT_DEBUG
  Cout << "Response::Response(short, Variables&, ProblemDescDB&) called to "
       << "instantiate envelope." << std::endl;
#endif

  // Set the rep pointer to the appropriate derived response class
  responseRep = get_response(type, vars, problem_db);
  if (!responseRep) // bad type or insufficient memory
    abort_handler(-1);
}


/** This is an alternate envelope constructor for instantiations on
    the fly.  This constructor executes get_response(type, set), which
    invokes the derived constructor corresponding to type. */
Response::Response(const SharedResponseData& srd, const ActiveSet& set):
  referenceCount(1) // not used since this is the envelope, not the letter
{
#ifdef REFCOUNT_DEBUG
  Cout << "Response::Response(SharedResponseData&, ActiveSet&) called to "
       << "instantiate envelope." << std::endl;
#endif

  // for responseRep, instantiate the appropriate derived response class
  responseRep = get_response(srd, set);
  if (!responseRep) // bad type or insufficient memory
    abort_handler(-1);
}


/** This is an alternate envelope constructor for instantiations on
    the fly.  This constructor executes get_response(type, set), which
    invokes the derived constructor corresponding to type. */
Response::Response(short type, const ActiveSet& set):
  referenceCount(1) // not used since this is the envelope, not the letter
{
#ifdef REFCOUNT_DEBUG
  Cout << "Response::Response(short, ActiveSet&) called to instantiate "
       << "envelope." << std::endl;
#endif

  // for responseRep, instantiate the appropriate derived response class
  responseRep = get_response(type, set);
  if (!responseRep) // bad type or insufficient memory
    abort_handler(-1);
}


/** This is an alternate envelope constructor for instantiations on
    the fly.  This constructor executes get_response(type, set), which
    invokes the derived constructor corresponding to type. */
Response::Response(const SharedResponseData& srd):
  referenceCount(1) // not used since this is the envelope, not the letter
{
#ifdef REFCOUNT_DEBUG
  Cout << "Response::Response(SharedResponseData&) called to instantiate "
       << "envelope." << std::endl;
#endif

  // for responseRep, instantiate the appropriate derived response class
  responseRep = get_response(srd);
  if (!responseRep) // bad type or insufficient memory
    abort_handler(-1);
}


/** Copy constructor manages sharing of responseRep and incrementing
    of referenceCount. */
Response::Response(const Response& resp)
{
  responseRep = resp.responseRep;
  // Increment new (no old to decrement)
  if (responseRep) // Check for assignment of NULL
    ++responseRep->referenceCount;

#ifdef REFCOUNT_DEBUG
  Cout << "Response::Response(Response&)" << std::endl;
  if (responseRep)
    Cout << "responseRep referenceCount = " << responseRep->referenceCount
	 << std::endl;
#endif
}


/** Assignment operator decrements referenceCount for old responseRep, assigns
    new responseRep, and increments referenceCount for new responseRep. */
Response Response::operator=(const Response& resp)
{
  if (responseRep != resp.responseRep) { // normal case: old != new
    // Decrement old
    if (responseRep) // Check for NULL
      if ( --responseRep->referenceCount == 0 ) 
	delete responseRep;
    // Assign and increment new
    responseRep = resp.responseRep;
    if (responseRep) // Check for NULL
      ++responseRep->referenceCount;
  }
  // else if assigning same rep, then do nothing since referenceCount
  // should already be correct

#ifdef REFCOUNT_DEBUG
  Cout << "Response::operator=(Response&)" << std::endl;
  if (responseRep)
    Cout << "responseRep referenceCount = " << responseRep->referenceCount
	 << std::endl;
#endif

  return *this; // calls copy constructor since returned by value
}


/** Destructor decrements referenceCount and only deletes responseRep
    when referenceCount reaches zero. */
Response::~Response()
{ 
  if (responseRep) { // Check for NULL pointer
    // envelope only: decrement letter reference count and delete if 0
    --responseRep->referenceCount;
#ifdef REFCOUNT_DEBUG
    Cout << "responseRep referenceCount decremented to " 
         << responseRep->referenceCount << std::endl;
#endif
    if (responseRep->referenceCount == 0) {
#ifdef REFCOUNT_DEBUG
      Cout << "deleting responseRep" << std::endl;
#endif
      delete responseRep;
    }
  }
}


/** Initializes responseRep to the appropriate derived type, as given
    by problem_db attributes.  The standard derived class constructors
    are invoked.  */
Response* Response::
get_response(short type, const Variables& vars,
	     const ProblemDescDB& problem_db) const
{
#ifdef REFCOUNT_DEBUG
  Cout << "Envelope instantiating letter in get_response(short, Variables&, "
       << "ProblemDescDB&)." << std::endl;
#endif

  // This get_response version invokes the standard constructor.
  switch (type) {
  case SIMULATION_RESPONSE:
    return new SimulationResponse(vars, problem_db); break;
  case EXPERIMENT_RESPONSE:
    return new ExperimentResponse(vars, problem_db); break;
  case BASE_RESPONSE:
    return new Response(BaseConstructor(), vars, problem_db); break;
  default:
    Cerr << "Response type " << type << " not currently supported in derived "
	 << "Response classes." << std::endl;
    return NULL; break;
  }
}


/** Initializes responseRep to the appropriate derived type, as given
    by SharedResponseData::responseType. */
Response* Response::
get_response(const SharedResponseData& srd, const ActiveSet& set) const
{
#ifdef REFCOUNT_DEBUG
  Cout << "Envelope instantiating letter in get_response()." << std::endl;
#endif

  switch (srd.response_type()) {
  case SIMULATION_RESPONSE:
    return new SimulationResponse(srd, set); break;
  case EXPERIMENT_RESPONSE:
    return new ExperimentResponse(srd, set); break;
  case BASE_RESPONSE:
    return new Response(BaseConstructor(), srd, set); break;
  default:
    Cerr << "Response type " << srd.response_type() << " not currently "
	 << "supported in derived Response classes." << std::endl;
    return NULL; break;
  }
}


/** Initializes responseRep to the appropriate derived class, as given
    by type. */
Response* Response::get_response(short type, const ActiveSet& set) const
{
#ifdef REFCOUNT_DEBUG
  Cout << "Envelope instantiating letter in get_response(short, ActiveSet&)."
       << std::endl;
#endif

  switch (type) {
  case SIMULATION_RESPONSE:
    return new SimulationResponse(set); break;
  case EXPERIMENT_RESPONSE:
    return new ExperimentResponse(set); break;
  case BASE_RESPONSE:
    return new Response(BaseConstructor(), set); break;
  default:
    Cerr << "Response type " << type << " not currently supported in derived "
	 << "Response classes." << std::endl;
    return NULL; break;
  }
}


/** Initializes responseRep to the appropriate derived type, as given
    by SharedResponseData::responseType. */
Response* Response::get_response(const SharedResponseData& srd) const
{
#ifdef REFCOUNT_DEBUG
  Cout << "Envelope instantiating letter in get_response()." << std::endl;
#endif

  switch (srd.response_type()) {
  case SIMULATION_RESPONSE:
    return new SimulationResponse(srd); break;
  case EXPERIMENT_RESPONSE:
    return new ExperimentResponse(srd); break;
  case BASE_RESPONSE:
    return new Response(BaseConstructor(), srd); break;
  default:
    Cerr << "Response type " << srd.response_type() << " not currently "
	 << "supported in derived Response classes." << std::endl;
    return NULL; break;
  }
}


/** Initializes responseRep to the appropriate derived type, as given
    by type. */
Response* Response::get_response(short type) const
{
#ifdef REFCOUNT_DEBUG
  Cout << "Envelope instantiating letter in get_response()." << std::endl;
#endif

  switch (type) {
  case SIMULATION_RESPONSE:
    return new SimulationResponse(); break;
  case EXPERIMENT_RESPONSE:
    return new ExperimentResponse(); break;
  case BASE_RESPONSE:
    return new Response();           break;
  default:
    Cerr << "Response type " << type << " not currently supported in "
	 << "derived Response classes." << std::endl;
    return NULL; break;
  }
}


Response Response::copy(bool deep_srd) const
{
  Response response; // empty envelope: responseRep=NULL

  if (responseRep) {
    // allocate a responseRep letter, copy data attributes, share the srd
    response.responseRep = (deep_srd) ?
      get_response(responseRep->sharedRespData.copy()) : // deep SRD copy
      get_response(responseRep->sharedRespData);      // shallow SRD copy
    // allow derived classes to specialize copy_rep if they augment
    // the base class data
    response.responseRep->copy_rep(responseRep);
  }

  return response;
}


void Response::copy_rep(Response* source_resp_rep)
{
  functionValues    = source_resp_rep->functionValues;
  fieldCoords       = source_resp_rep->fieldCoords;
  functionGradients = source_resp_rep->functionGradients;
  functionHessians  = source_resp_rep->functionHessians;
  responseActiveSet = source_resp_rep->responseActiveSet;
}


// I/O Notes:

// Bitwise AND used in ASV checks in read/write fns: the '&' operator does a bit
// by bit AND operation.  For example, 7 & 4 does a bitwise AND on 111 and 100
// and returns 100 or 4.  It is not necessary to verify that, for example,
// (asv[i] & 4) == 4 since (asv[i] & 4) will be nonzero/true only if the 4 bit
// is present in asv[i] (the "== value" can be omitted so long as only the
// presence of a single bit is of interest).

// Matrix sizing: since fn gradients and hessians are of size 0 if their type is
// "none", all grad loops are run from 0 to num rows and all Hessian loops are
// run from 0 to num hessians, rather than from 0 to num_fns.  For the number
// of derivative variables, the length of responseActiveSet.derivative_vector()
// is used rather than the array sizes since the matrices are originally sized
// according to the maximal case and all entries may not be used.

// When creating functionGradients/functionHessians on the fly in read() input
// functions, grad_flag/hess_flag are used rather than inferring sizes from
// responseActiveSet since it is safer to match the sizing of the response
// object that was written (originally sized by the Response constructor) than
// to base sizing on the particular constent of an asv (which can vary from
// eval. to eval.).  For example, assigning empty gradient/Hessian matrices to
// Model::currentResponse in a restart operation would cause problems.


/** ASCII version of read needs capabilities for capturing data omissions or
    formatting errors (resulting from user error or asynch race condition) and
    analysis failures (resulting from nonconvergence, instability, etc.). */
void Response::read(std::istream& s)
{
  // if envelope, forward to letter
  if (responseRep)
    { responseRep->read(s); return; }

  // Failure capturing:
  // The read operator detects analysis failure through reading a failure 
  // string returned by the user's output_filter/analysis_driver.  This string
  // must occur at the beginning of the file, even if some portion of the 
  // results were successfully computed prior to failure.  Other mechanisms for
  // denoting analysis failure that were considered included (1) use of an 
  // empty file (disadvantage: ambiguity with asynch race condition) and (2) 
  // use of a success/fail string (disadvantage: would require addition of 
  // "success" field to non-failure output).  The current approach avoids these
  // problems by doing a read of 4 characters.  If these characters are not 
  // "fail" or "FAIL" then it resets the stream pointer to the beginning.  This
  // works for ifstreams, but may be problematic for other istreams.  Once 
  // failure is detected, a FunctionEvalFailure is thrown (to distinguish 
  // from FileReadExceptions) that will be caught after try{ execute() }.
  // NOTE: s.peek() triggering on 'f' or 'F' would be another possibility.
  // NOTE: reading the first token using "s >> fail_string;" should work for a
  //       file having less than four characters.
  size_t i, j, k;
  bool failure_detected = true;
  char fail_chars[4] = {0,0,0,0};
  std::string fail_string("fail");
  // Old version failed for fewer than 4 characters in results file:
  //s >> fail_chars[0] >> fail_chars[1] >> fail_chars[2] >> fail_chars[3];
  for (i=0; i<4; i++) {
    s >> fail_chars[i];
    //Cout << "fail_char[" << i << "] = " << fail_chars[i] << std::endl;
    if (tolower(fail_chars[i]) != fail_string[i]) { // FAIL, Fail, fail, etc.
      failure_detected = false; // No failure communicated from results file 
      s.seekg(0); // Reset stream to beginning
      break; // exit for loop
    }
  }
  if (failure_detected) {
    std::string failure_message = "Failure captured with string = ";
    failure_message += fail_chars;
    //Cerr << failure_message << std::endl;
    throw FunctionEvalFailure(failure_message);
  }

  // Destroy old values and set to zero (so that else assignments are not needed
  // below). The arrays have been properly sized by the Response constructor.
  reset();

  // Get fn. values as governed by ASV requests
  std::string token;
  boost::regex reg_exp("[\\+-]?[0-9]*\\.?[0-9]+\\.?[0-9]*[eEdD]?[\\+-]?[0-9]*|-?[Nn][Aa][Nn]|[\\+-]?[Ii][Nn][Ff]([Ii][Nn][Ii][Tt][Yy])?");
  const ShortArray& asv = responseActiveSet.request_vector();
  size_t nf = asv.size();
  for (i=0; i<nf; i++) {
    if (asv[i] & 1) { // & 1 masks off 2nd and 3rd bit
      if (s) { // get value
	s >> token;
	// On RHEL 4, strtod preserves Inf/NaN, but atof doesn't
	//Cout << "Debug read: token = " << token << '\n';
	//Cout << "Debug read: atof of token = " << atof(token) << '\n';
	//Cout << "Debug read: strtod of token = " << strtod(token, NULL)<<'\n';
	// On error, atof returns 0.0. Must verify token is a number.
	if(token == re_match(token, reg_exp))
	  functionValues[i] = std::atof(token.c_str());// handles NaN and +/-Inf
	else
	  throw ResultsFileError( "Response format error with functionValue "
				  + boost::lexical_cast<std::string>(i+1) );
      }
      else
        throw ResultsFileError( "At EOF: insufficient data for functionValue "
				+ boost::lexical_cast<std::string>(i+1) );

      if (s) { // get optional tag
	//s.ignore(256, '\n'); // simple soln., but requires consistent '\n'
        int pos = s.tellg(); // save stream pos prior to token extraction
        s >> token; // get next field (may be a tag or a number)
        // Check to see if token matches the pattern (see CtelRegExp class docs)
	// of a numerical value (including +/-Inf and NaN) or of the beginning
	// of a gradient/Hessian block.  If it does, then rewind the stream.
        if ( !token.empty() &&
	     ( token[(size_t)0]=='[' || token == re_match(token, reg_exp) ) )
          s.seekg(pos); // token is not a tag, rewind
        // else field was properly extracted as a tag
      }
    }
  }

  // Get function gradients as governed by ASV requests
  // For brackets, chars are used rather than token strings to allow optional
  // white space between brackets and values.
  char l_bracket, r_bracket; // eat white space and grab 1 character
  size_t ng = functionGradients.numCols(), nv = functionGradients.numRows();
  for (i=0; i<ng; ++i) { // prevent loop if functionGradients not sized
    if (asv[i] & 2) { // & 2 masks off 1st and 3rd bit
      if (s)
        s >> l_bracket;
      else
        throw ResultsFileError( "At EOF: insufficient data for functionGradient "
				+ boost::lexical_cast<std::string>(i+1) );

      read_col_vector_trans(s, (int)i, functionGradients); // fault tolerant

      if (s)
        s >> r_bracket;
      else {
        throw ResultsFileError( "At EOF: insufficient data for functionGradient "
				+ boost::lexical_cast<std::string>(i+1) );
      }
      if (l_bracket != '[' || r_bracket != ']') {
        throw ResultsFileError( "Response format error with functionGradient "
				+ boost::lexical_cast<std::string>(i+1) );
      }
    }
  }

  // Get function Hessians as governed by ASV requests
  char l_brackets[2], r_brackets[2]; // eat white space and grab 2 characters
  size_t nh = functionHessians.size();
  for (i=0; i<nh; i++) { // prevent loop if functionHessians not sized
    if (asv[i] & 4) { // & 4 masks off 1st and 2nd bit
      if (s)
        s >> l_brackets[0] >> l_brackets[1];
      else
        throw ResultsFileError( "At EOF: insufficient data for functionHessian "
				+ boost::lexical_cast<std::string>(i+1) );

      Dakota::read_data(s, functionHessians[i]); // fault tolerant

      if (s)
        s >> r_brackets[0] >> r_brackets[1];
      else {
        throw ResultsFileError( "At EOF: insufficient data for functionHessian "
				+ boost::lexical_cast<std::string>(i+1) );
      }
      if ((l_brackets[0] != '[' || l_brackets[1] != '[')  ||
	  (r_brackets[0] != ']' || r_brackets[1] != ']')) {
        throw ResultsFileError( "Response format error with functionHessian "
				+ boost::lexical_cast<std::string>(i+1) );
      }
    }
  }
}


/** ASCII version of write. */
void Response::write(std::ostream& s) const
{
  // if envelope, forward to letter
  if (responseRep)
    { responseRep->write(s); return; }

  const ShortArray& asv = responseActiveSet.request_vector();
  const SizetArray& dvv = responseActiveSet.derivative_vector();
  size_t i, nf = asv.size();
  bool deriv_flag = false;
  for (i=0; i<nf; ++i)
    if (asv[i] & 6)
      { deriv_flag = true; break; }

  // Write ASV/DVV information
  s << "Active set vector = { ";
  array_write_annotated(s, asv, false);
  if (deriv_flag) { // dvv != vars.continuous_variable_ids() ?,
                    // outputLevel > NORMAL_OUTPUT ?
    s << "} Deriv vars vector = { ";
    array_write_annotated(s, dvv, false);
    s << "}\n";
  }
  else
    s << "}\n";

  // Make sure a valid set of function labels exists. This has been a problem
  // since there is no way to build these labels in the default Response
  // constructor (used by lists & vectors of Response objects).
  const StringArray& fn_labels = sharedRespData.function_labels();
  if (fn_labels.size() != nf) {
    Cerr << "Error with function labels in Response::write." << std::endl;
    abort_handler(-1);
  }

  // Write the function values if present
  for (i=0; i<nf; ++i)
    if (asv[i] & 1) // & 1 masks off 2nd and 3rd bit
      s << "                     " << std::setw(write_precision+7) 
        << functionValues[i] << ' ' << fn_labels[i] << '\n';

  // Write the function gradients if present
  size_t ng = functionGradients.numCols();
  for (i=0; i<ng; ++i) {
    if (asv[i] & 2) { // & 2 masks off 1st and 3rd bit
      // NOTE: col_vec is WRITTEN-out like a row_vec for historical consistency
      write_col_vector_trans(s, (int)i, true, true, false, functionGradients);
      s << fn_labels[i] << " gradient\n";
    }
  }

  // Write the function Hessians if present
  size_t nh = functionHessians.size();
  for (i=0; i<nh; ++i) {
    if (asv[i] & 4) { // & 4 masks off 1st and 2nd bit
      Dakota::write_data(s, functionHessians[i], true, true, false);
      s << fn_labels[i] << " Hessian\n";
    }
  }
  s << std::endl;
}


void Response::read_annotated(std::istream& s)
{
  short type;
  s >> type;
  if (responseRep) { // should not occur in current usage
    if (responseRep->sharedRespData.is_null() ||
	type != responseRep->sharedRespData.response_type()) {
      if (--responseRep->referenceCount == 0)
	delete responseRep;
      responseRep = get_response(type);
    }
  }
  else // read into empty envelope: responseRep must be instantiated
    responseRep = get_response(type);

  responseRep->read_annotated_rep(s); // fwd to new/existing rep
  responseRep->sharedRespData.response_type(type);
}


void Response::write_annotated(std::ostream& s) const
{
  if (responseRep)
    responseRep->write_annotated(s);
  else {
    s << sharedRespData.response_type() << ' ';
    write_annotated_rep(s);
  }
}


/** read_annotated() is used for neutral file translation of restart files.
    Since objects are built solely from this data, annotations are used.
    This version closely mirrors the BiStream version. */
void Response::read_annotated_rep(std::istream& s)
{
  // Read sizing data
  size_t i, num_fns, num_params;
  bool grad_flag, hess_flag;
  s >> num_fns >> num_params >> grad_flag >> hess_flag;

  // Read responseActiveSet and SharedResponseData::functionLabels
  responseActiveSet.reshape(num_fns, num_params);
  s >> responseActiveSet;
  if (sharedRespData.is_null())
    sharedRespData = SharedResponseData(responseActiveSet);
  s >> sharedRespData.function_labels();

  // reshape response arrays and reset all data to zero
  reshape(num_fns, num_params, grad_flag, hess_flag);
  reset();

  // Get fn. values as governed by ASV requests
  const ShortArray& asv = responseActiveSet.request_vector();
  std::string token; // used with atof() to handle +/-inf & nan
  for (i=0; i<num_fns; ++i)
    if (asv[i] & 1) // & 1 masks off 2nd and 3rd bit
      { s >> token; functionValues[i] = std::atof(token.c_str()); }

  // Get function gradients as governed by ASV requests
  size_t nv = functionGradients.numRows();
  for (i=0; i<num_fns; ++i)
    if (asv[i] & 2) // & 2 masks off 1st and 3rd bit
      read_col_vector_trans(s, (int)i, functionGradients); // fault tolerant

  // Get function Hessians as governed by ASV requests
  for (i=0; i<num_fns; ++i)
    if (asv[i] & 4) // & 4 masks off 1st and 2nd bit
      read_lower_triangle(s, functionHessians[i]); // fault tolerant
}


/** write_annotated() is used for neutral file translation of restart files.
    Since objects need to be build solely from this data, annotations are used.
    This version closely mirrors the BoStream version, with the exception of
    the use of white space between fields. */
void Response::write_annotated_rep(std::ostream& s) const
{
  const ShortArray& asv = responseActiveSet.request_vector();
  size_t i, num_fns = asv.size() ;

  // Write Response sizing data
  s << num_fns << ' ' << responseActiveSet.derivative_vector().size() << ' '
    << !functionGradients.empty() << ' ' << !functionHessians.empty() << ' ';

  // Write responseActiveSet and function labels.  Don't separately annotate
  // arrays with sizing data since Response handles this all at once.
  responseActiveSet.write_annotated(s);
  array_write_annotated(s, sharedRespData.function_labels(), false);

  // Write the function values if present
  for (i=0; i<num_fns; ++i)
    if (asv[i] & 1) // & 1 masks off 2nd and 3rd bit
      s << functionValues[i] << ' ';

  // Write the function gradients if present
  for (i=0; i<num_fns; ++i)
    if (asv[i] & 2) // & 2 masks off 1st and 3rd bit
      write_col_vector_trans(s, (int)i, false, false, false, functionGradients);

  // Write the function Hessians if present
  for (i=0; i<num_fns; ++i)
    if (asv[i] & 4) // & 4 masks off 1st and 2nd bit
      write_lower_triangle(s, functionHessians[i], false);
}


/** read_tabular is used to read functionValues in tabular format.  It
    is currently only used by ApproximationInterfaces in reading samples
    from a file.  There is insufficient data in a tabular file to build
    complete response objects; rather, the response object must be
    constructed a priori and then its functionValues can be set. */
void Response::read_tabular(std::istream& s)
{
  // if envelope, forward to letter
  if (responseRep)
    responseRep->read_tabular(s);
  else {
    // Read the function values, regardless of ASV.  Since this is a table
    // format, there must be a field read even when data is inactive.
    size_t i, num_fns = functionValues.length();
    std::string token;
    for (i=0; i<num_fns; ++i)
      if (s)
	{ s >> token; functionValues[i] = std::atof(token.c_str()); }
      else
	throw TabularDataTruncated("At EOF: insufficient data for RealVector["
				   + boost::lexical_cast<std::string>(i) + "]");
  }
}


/** write_tabular is used for output of functionValues in a tabular
    format for convenience in post-processing/plotting of DAKOTA results. */
void Response::write_tabular(std::ostream& s) const
{
  // if envelope, forward to letter
  if (responseRep)
    responseRep->write_tabular(s);
  else {
    // Print a field for each of the function values, even if inactive (since
    // this is a table and the header associations must be preserved). Dropouts
    // (inactive data) could be printed as 0's (the inactive value), "N/A",
    // -9999, a blank field, etc.  Using either "N/A" or a blank field gives
    // the correct meaning visually, but both cause problems with data import.
    size_t i, num_fns = functionValues.length();
    const ShortArray& asv = responseActiveSet.request_vector();
    s << std::setprecision(write_precision) 
      << std::resetiosflags(std::ios::floatfield);
    for (i=0; i<num_fns; ++i)
      if (asv[i] & 1)
	s << std::setw(write_precision+4) << functionValues[i] << ' ';
      else
	s << "               "; // blank field for inactive data
      // BMA TODO: write something that can be read back in for tabular...
      //s << std::numeric_limits<double>::quiet_NaN(); // inactive data
      //s << "EMPTY"; // inactive data
    s << std::endl; // table row completed
  }
}


void Response::write_tabular_labels(std::ostream& s) const
{
  // if envelope, forward to letter
  if (responseRep)
    responseRep->write_tabular_labels(s);
  else {
    const StringArray& fn_labels = sharedRespData.function_labels();
    size_t num_fns = fn_labels.size();
    for (size_t j=0; j<num_fns; ++j)
      s << std::setw(14) << fn_labels[j] << ' ';
    s << std::endl; // table row completed
  }
}


void Response::read(MPIUnpackBuffer& s)
{
  bool has_rep;
  s >> has_rep;
  if (has_rep) { // response is not an empty handle
    short type;
    s >> type;
    if (responseRep) { // responseRep should not be defined in current usage
      if (responseRep->sharedRespData.is_null() ||
	  type != responseRep->sharedRespData.response_type()) {
	if (--responseRep->referenceCount == 0)
	  delete responseRep;
	responseRep = get_response(type);
      }
    }
    else
      responseRep = get_response(type);

    responseRep->read_rep(s); // fwd to rep
    responseRep->sharedRespData.response_type(type);
  }
  else if (responseRep) {
    if (--responseRep->referenceCount == 0)
      delete responseRep;
    responseRep = NULL;
  }
}


void Response::write(MPIPackBuffer& s) const
{
  s << !is_null();
  if (responseRep) {
    s << responseRep->sharedRespData.response_type();
    responseRep->write_rep(s);
  }
}


/** UnpackBuffer version differs from BiStream version in the omission
    of functionLabels.  Master processor retains labels and interface ids
    and communicates asv and response data only with slaves. */
void Response::read_rep(MPIUnpackBuffer& s)
{
  // Read derivative sizing data and responseActiveSet
  bool grad_flag, hess_flag;
  s >> grad_flag >> hess_flag >> responseActiveSet;

  // build shared counts and (default) functionLabels
  if (sharedRespData.is_null())
    sharedRespData = SharedResponseData(responseActiveSet); // not shared

  // reshape response arrays and reset all data to zero
  const ShortArray& asv = responseActiveSet.request_vector();
  size_t i, num_fns = asv.size();
  reshape(num_fns, responseActiveSet.derivative_vector().size(),
	  grad_flag, hess_flag);
  reset();

  // Get fn. values as governed by ASV requests
  for (i=0; i<num_fns; ++i)
    if (asv[i] & 1) // & 1 masks off 2nd and 3rd bit
      s >> functionValues[i];

  // Get function gradients as governed by ASV requests
  for (i=0; i<num_fns; ++i)
    if (asv[i] & 2) // & 2 masks off 1st and 3rd bit
      read_col_vector_trans(s, (int)i, functionGradients);

  // Get function Hessians as governed by ASV requests
  for (i=0; i<num_fns; ++i)
    if (asv[i] & 4) // & 4 masks off 1st and 2nd bit
      read_lower_triangle(s, functionHessians[i]);
}


/** MPIPackBuffer version differs from BoStream version only in the
    omission of functionLabels.  The master processor retains labels
    and ids and communicates asv and response data only with slaves. */
void Response::write_rep(MPIPackBuffer& s) const
{
  // Write sizing data and responseActiveSet
  s << !functionGradients.empty() << !functionHessians.empty()
    << responseActiveSet;

  const ShortArray& asv = responseActiveSet.request_vector();
  size_t i, num_fns = asv.size();

  // Write the function values if present
  for (i=0; i<num_fns; ++i)
    if (asv[i] & 1) // & 1 masks off 2nd and 3rd bit
      s << functionValues[i];

  // Write the function gradients if present
  for (i=0; i<num_fns; ++i)
    if (asv[i] & 2) // & 2 masks off 1st and 3rd bit
      write_col_vector_trans(s, (int)i, functionGradients);

  // Write the function Hessians if present
  for (i=0; i<num_fns; ++i)
    if (asv[i] & 4) // & 4 masks off 1st and 2nd bit
      write_lower_triangle(s, functionHessians[i]);
}


int Response::data_size()
{
  if (responseRep)
    return responseRep->data_size();
  else {
    // return the number of doubles active in response for sizing double* 
    // response_data arrays passed into read_data and write_data
    int size = 0;
    const ShortArray& asv = responseActiveSet.request_vector();
    size_t i, num_fns = functionValues.length(),
      grad_size = responseActiveSet.derivative_vector().size(),
      hess_size = grad_size*(grad_size+1)/2; // (n^2+n)/2
    for (i=0; i<num_fns; ++i) {
      if (asv[i] & 1) ++size;
      if (asv[i] & 2) size += grad_size;
      if (asv[i] & 4) size += hess_size;
    }
    return size;
  }
}


void Response::read_data(double* response_data)
{
  if (responseRep)
    responseRep->read_data(response_data);
  else {
    // read from an incoming double* array
    const ShortArray& asv = responseActiveSet.request_vector();
    size_t i, j, k, cntr = 0, num_fns = functionValues.length(),
      num_params = responseActiveSet.derivative_vector().size();
    for (i=0; i<num_fns; ++i)
      if (asv[i] & 1)
	functionValues[i] = response_data[cntr++];
    num_fns = functionGradients.numCols();
    for (i=0; i<num_fns; ++i)
      if (asv[i] & 2) {
	Real* fn_grad_i = functionGradients[i];
	for (j=0; j<num_params; ++j, ++cntr)
	  fn_grad_i[j] = response_data[cntr];
      }
    num_fns = functionHessians.size();
    for (i=0; i<num_fns; ++i)
      if (asv[i] & 4) {
	RealSymMatrix& fn_hess_i = functionHessians[i];
	for (j=0; j<num_params; ++j)
	  for (k=0; k<=j; ++k, ++cntr)
	    fn_hess_i(j,k) = response_data[cntr];
      }
  }
}


void Response::write_data(double* response_data)
{
  if (responseRep)
    responseRep->write_data(response_data);
  else {
    // write to an incoming double* array
    const ShortArray& asv = responseActiveSet.request_vector();
    size_t i, j, k, cntr = 0, num_fns = functionValues.length(),
      num_params = responseActiveSet.derivative_vector().size();
    for (i=0; i<num_fns; ++i)
      if (asv[i] & 1)
	response_data[cntr++] = functionValues[i];
    num_fns = functionGradients.numCols();
    for (i=0; i<num_fns; ++i)
      if (asv[i] & 2) {
	const Real* fn_grad_i = functionGradients[i];
	for (j=0; j<num_params; j++, ++cntr)
	  response_data[cntr] = fn_grad_i[j];
      }
    num_fns = functionHessians.size();
    for (i=0; i<num_fns; ++i)
      if (asv[i] & 4) {
	const RealSymMatrix& fn_hess_i = functionHessians[i];
	for (j=0; j<num_params; ++j)
	  for (k=0; k<=j; ++k, ++cntr)
	    response_data[cntr] = fn_hess_i(j,k);
      }
  }
}


void Response::overlay(const Response& response)
{
  if (responseRep)
    responseRep->overlay(response);
  else {
    // add incoming response to functionValues/Gradients/Hessians
    const ShortArray& asv = responseActiveSet.request_vector();
    size_t i, j, k, num_fns = asv.size(),
      num_params = responseActiveSet.derivative_vector().size();
    for (i=0; i<num_fns; ++i)
      if (asv[i] & 1)
	functionValues[i] += response.function_value(i);
    num_fns = functionGradients.numCols();
    for (i=0; i<num_fns; ++i)
      if (asv[i] & 2) {
	const Real* partial_fn_grad = response.function_gradient(i);
	Real* fn_grad = functionGradients[i];
	for (j=0; j<num_params; ++j)
	  fn_grad[j] += partial_fn_grad[j];
      }
    num_fns = functionHessians.size();
    for (i=0; i<num_fns; ++i)
      if (asv[i] & 4) {
	const RealSymMatrix& partial_fn_hess = response.function_hessian(i);
	RealSymMatrix& fn_hess = functionHessians[i];
	for (j=0; j<num_params; ++j)
	  for (k=0; k<=j; ++k)
	    fn_hess(j,k) += partial_fn_hess(j,k);
      }
  }
}


/** Copy function values/gradients/Hessians data _only_.  Prevents unwanted
    overwriting of responseActiveSet, functionLabels, etc.  Also, care is
    taken to account for differences in derivative variable matrix sizing. */
void Response::
update(const RealVector& source_fn_vals, const RealMatrix& source_fn_grads,
       const RealSymMatrixArray& source_fn_hessians,
       const ActiveSet& source_set)
{
  if (responseRep) {
    responseRep->
      update(source_fn_vals, source_fn_grads, source_fn_hessians, source_set);
    return;
  }

  // current response data
  const ShortArray& asv = responseActiveSet.request_vector();
  size_t i, j, k, num_fns = asv.size(), 
    num_params = responseActiveSet.derivative_vector().size();
  bool grad_flag = false, hess_flag = false;
  for (i=0; i<num_fns; i++) {
    if (asv[i] & 2)
      grad_flag = true;
    if (asv[i] & 4)
      hess_flag = true;
  }

  // verify that incoming data is of sufficient size
  if (source_set.request_vector().size() < num_fns) {
    Cerr << "Error: insufficient number of response functions to copy "
	 << "response results in Response::update()." << std::endl;
    abort_handler(-1);
  }
  if ( (grad_flag || hess_flag) && 
       source_set.derivative_vector().size() < num_params) {
    Cerr << "Error: insufficient number of derivative variables to copy "
	 << "response results in Response::update()." << std::endl;
    abort_handler(-1);
  }

  // for functionValues, using operator= does not allow vector size to differ.
  // for functionGradients/functionHessians, using operator= does not allow
  // num_fns or derivative variable matrix sizing to differ.  This includes the
  // extreme case in which the incoming derivative arrays may have zero size
  // (e.g., this can happen in a multilevel strategy where a captured duplicate
  // came from the evals of a previous nongradient-based algorithm).
  //functionValues    = source_fn_vals;
  //functionGradients = source_fn_grads;
  //functionHessians  = source_fn_hessians;

  for (i=0; i<num_fns; i++)
    if (asv[i] & 1) // assign each entry since size may differ
      functionValues[i] = source_fn_vals[i];

  // copy source_fn_grads and source_fn_hessians.  For now, a mapping from
  // responseActiveSet.derivative_vector() to source_set.derivative_vector()
  // is _not_ used, although this may be needed in the future.
  if (grad_flag) {
    if (source_fn_grads.numCols() < num_fns) {
      Cerr << "Error: insufficient incoming gradient size to copy response "
	   << "results required in Response::update()." << std::endl;
      abort_handler(-1);
    }
    for (i=0; i<num_fns; i++)
      if (asv[i] & 2) // assign each entry since size may differ
	for (j=0; j<num_params; j++)
	  functionGradients(j,i) = source_fn_grads(j,i);
  }
  if (hess_flag) {
    if (source_fn_hessians.size() < num_fns) {
      Cerr << "Error: insufficient incoming Hessian size to copy response "
	   << "results required in Response::update()." << std::endl;
      abort_handler(-1);
    }
    for (i=0; i<num_fns; i++)
      if (asv[i] & 4) // assign each entry since size may differ
	for (j=0; j<num_params; j++)
	  for (k=0; k<=j; k++)
	    functionHessians[i](j,k) = source_fn_hessians[i](j,k);
  }

  // Remove any data retrieved from response but not reqd by responseActiveSet
  // (id_vars_set_compare will detect duplication when the current asv is a
  // subset of the database asv and more can be retrieved than requested).
  if (responseActiveSet != source_set) // only reset if needed
    reset_inactive();
}


/** Copy function values/gradients/Hessians data _only_.  Prevents unwanted
    overwriting of responseActiveSet, functionLabels, etc.  Also, care is
    taken to account for differences in derivative variable matrix sizing. */
void Response::
update_partial(size_t start_index_target, size_t num_items,
	       const RealVector& source_fn_vals,
	       const RealMatrix& source_fn_grads,
	       const RealSymMatrixArray& source_fn_hessians,
	       const ActiveSet& source_set, size_t start_index_source)
{
  if (responseRep) {
    responseRep->update_partial(start_index_target, num_items, source_fn_vals,
				source_fn_grads, source_fn_hessians, source_set,
				start_index_source);
    return;
  }

  if (!num_items)
    return;

  // current response data
  const ShortArray& asv = responseActiveSet.request_vector();
  size_t i, j, k, num_fns = asv.size(), 
    num_params = responseActiveSet.derivative_vector().size();
  bool grad_flag = false, hess_flag = false;
  for (i=0; i<num_fns; i++) {
    if (asv[i] & 2)
      grad_flag = true;
    if (asv[i] & 4)
      hess_flag = true;
  }

  // verify that incoming data is of sufficient size
  if (start_index_target + num_items > num_fns ||
      start_index_source + num_items > source_set.request_vector().size() ) {
    Cerr << "Error: insufficient number of response functions to update partial"
	 << " response results in Response::update_partial()." << std::endl;
    abort_handler(-1);
  }
  if ( (grad_flag || hess_flag) && 
       source_set.derivative_vector().size() < num_params) {
    Cerr << "Error: insufficient number of derivative variables to update "
	 << "partial response derivative results in Response::"
	 << "update_partial()." << std::endl;
    abort_handler(-1);
  }

  for (i=0; i<num_items; i++)
    if (asv[start_index_target+i] & 1)
      functionValues[start_index_target+i]
	= source_fn_vals[start_index_source+i];

  // copy source_fn_grads and source_fn_hessians.  For now, a mapping from
  // responseActiveSet.derivative_vector() to source_set.derivative_vector()
  // is _not_ used, although this may be needed in the future.
  if (grad_flag) {
    if (source_fn_grads.numCols() < start_index_source + num_items) {
      Cerr << "Error: insufficient incoming gradient size to update partial "
	   << "response gradient results required in Response::"
	   << "update_partial()." << std::endl;
      abort_handler(-1);
    }
    for (i=0; i<num_items; i++)
      if (asv[start_index_target+i] & 2)
	for (j=0; j<num_params; j++)
	  functionGradients(j,start_index_target+i)
	    = source_fn_grads(j,start_index_source+i);
  }
  if (hess_flag) {
    if (source_fn_hessians.size() < start_index_source + num_items) {
      Cerr << "Error: insufficient incoming Hessian size to update partial "
	   << "response Hessian results required in Response::"
	   << "update_partial()." << std::endl;
      abort_handler(-1);
    }
    for (i=0; i<num_items; i++)
      if (asv[start_index_target+i] & 4)
	for (j=0; j<num_params; j++)
	  for (k=0; k<=j; k++)
	    functionHessians[start_index_target+i](j,k)
	      = source_fn_hessians[start_index_source+i](j,k);
  }

  // Remove any data retrieved from response but not reqd by responseActiveSet
  // (id_vars_set_compare will detect duplication when the current asv is a
  // subset of the database asv and more can be retrieved than requested).
  if (responseActiveSet != source_set) // only reset if needed
    reset_inactive();// active data not included in partial update are not reset
}


/** Reshape functionValues, functionGradients, and functionHessians
    according to num_fns, num_params, grad_flag, and hess_flag. */
void Response::
reshape(size_t num_fns, size_t num_params, bool grad_flag, bool hess_flag)
{
  if (responseRep)
    responseRep->reshape(num_fns, num_params, grad_flag, hess_flag);
  else {
    // resizes scalars for now (needs additional data for field reshape)
    sharedRespData.reshape(num_fns);
    reshape_rep(num_fns, num_params, grad_flag, hess_flag);
  }
}


void Response::field_lengths(const IntVector& field_lens)
{
  if (responseRep) 
    responseRep->field_lengths(field_lens);
  else {
    // resize shared data based on new field lengths
    sharedRespData.field_lengths(field_lens);

    // then resize my arrays using *new* num_functions, only
    // allocating grad, hess if already sized
    reshape_rep(sharedRespData.num_functions(), 
		responseActiveSet.derivative_vector().size(), false, false);
  }
}


void Response::shape_rep(const ActiveSet& set, bool initialize)
{
  // Set flags according to asv content.
  const ShortArray& asv = set.request_vector();
  size_t i, num_fns = asv.size(),
    num_params = set.derivative_vector().size();
  bool grad_flag = false, hess_flag = false;
  for (i=0; i<num_fns; i++) {
    if (asv[i] & 2) grad_flag = true;
    if (asv[i] & 4) hess_flag = true;
  }

  if (initialize) {
    functionValues.size(num_fns);                   // init to 0
    if (grad_flag)
      functionGradients.shape(num_params, num_fns); // init to 0
    if (hess_flag) {
      functionHessians.resize(num_fns);
      for (i=0; i<num_fns; i++)
	functionHessians[i].shape(num_params);      // init to 0
    }
  }
  else {
    functionValues.sizeUninitialized(num_fns);
    if (grad_flag)
      functionGradients.shapeUninitialized(num_params, num_fns);
    if (hess_flag) {
      functionHessians.resize(num_fns);
      for (i=0; i<num_fns; i++)
	functionHessians[i].shapeUninitialized(num_params);
    }
  }
}


void Response::
reshape_rep(size_t num_fns, size_t num_params, bool grad_flag, bool hess_flag)
{
  if (responseActiveSet.request_vector().size()    != num_fns || 
      responseActiveSet.derivative_vector().size() != num_params)
    responseActiveSet.reshape(num_fns, num_params);

  if (functionValues.length() != num_fns)
    functionValues.resize(num_fns);

  if (grad_flag) {
    if (functionGradients.numRows() != num_params || 
	functionGradients.numCols() != num_fns)
      functionGradients.reshape(num_params, num_fns);
  }
  else if (!functionGradients.empty())
    functionGradients.shape(0,0);

  if (hess_flag) {
    if (functionHessians.size() != num_fns)
      functionHessians.resize(num_fns);
    for (size_t i=0; i<num_fns; ++i) {
      if (functionHessians[i].numRows() != num_params)
	functionHessians[i].reshape(num_params);
    }
  }
  else if (!functionHessians.empty())
    functionHessians.clear();
}


/** Reset all numerical response data (not labels, ids, or active set)
    to zero. */
void Response::reset()
{
  if (responseRep)
    responseRep->reset();
  else {
    functionValues = 0.;
    functionGradients = 0.;
    size_t nh = functionHessians.size();
    for (size_t i=0; i<nh; i++)
      functionHessians[i] = 0.;
  }
}


/** Used to clear out any inactive data left over from previous evaluations. */
void Response::reset_inactive()
{
  if (responseRep)
    responseRep->reset_inactive();
  else {
    const ShortArray& asv = responseActiveSet.request_vector();
    size_t i, asv_len = asv.size(), ng = functionGradients.numCols(),
      nh = functionHessians.size();
    for (i=0; i<asv_len; ++i)
      if ( !(asv[i] & 1) ) // value bit is omitted
	functionValues[i] = 0.;
    for (int i=0; i<ng; ++i)
      if ( !(asv[i] & 2) ) { // gradient bit is omitted
	std::fill_n(functionGradients[i], functionGradients.numRows(), 0.);
      }
    for (i=0; i<nh; ++i)
      if ( !(asv[i] & 4) ) // Hessian bit is omitted
	functionHessians[i] = 0.;  // the i_th Hessian (all rows & columns)
  }
}


/// equality operator for Response
bool operator==(const Response& resp1, const Response& resp2)
{
  Response *rep1 = resp1.responseRep, *rep2 = resp2.responseRep;
  if (rep1 != NULL && rep2 != NULL) // envelope
    return (rep1->responseActiveSet == rep2->responseActiveSet &&
	    rep1->functionValues    == rep2->functionValues    &&
	    rep1->functionGradients == rep2->functionGradients &&
	    rep1->functionHessians  == rep2->functionHessians);
  else if (rep1 == NULL && rep2 == NULL) // letter
    return (resp1.responseActiveSet == resp2.responseActiveSet &&
	    resp1.functionValues    == resp2.functionValues    &&
	    resp1.functionGradients == resp2.functionGradients &&
	    resp1.functionHessians  == resp2.functionHessians);
  else // letter/envelope mismatch
    return false;
}


void Response::active_set_request_vector(const ShortArray& asrv)
{
  if (responseRep)
    return responseRep->active_set_request_vector(asrv);
  else {
    // a change in ASV length is not currently allowed
    if (responseActiveSet.request_vector().size() != asrv.size()) {
      Cerr << "Error: total number of response functions may not be changed in "
	   << "Response::active_set_request_vector(ShortArray&)." << std::endl;
      abort_handler(-1);
    }
    // assign the new request vector
    responseActiveSet.request_vector(asrv);
  }
}


void Response::active_set_derivative_vector(const SizetArray& asdv)
{
  if (responseRep)
    responseRep->active_set_derivative_vector(asdv);
  else {
    // resize functionGradients/functionHessians if needed to accomodate
    // a change in DVV size
    size_t new_deriv_vars = asdv.size();
    if (responseActiveSet.derivative_vector().size() != new_deriv_vars) {
      size_t num_fns = responseActiveSet.request_vector().size();
      //reshape(num_fns, new_deriv_vars, !functionGradients.empty(),
      //	!functionHessians.empty());
      if (!functionGradients.empty())
	functionGradients.reshape(new_deriv_vars, num_fns);
      if (!functionHessians.empty())
	for (size_t i=0; i<num_fns; i++)
	  functionHessians[i].reshape(new_deriv_vars);
    }
    // assign the new derivative vector
    responseActiveSet.derivative_vector(asdv);
  }
}

void Response::set_scalar_covariance(RealVector& scalars)
{
  if (responseRep)
    responseRep->set_scalar_covariance(scalars);
  else {
    Cerr << "\nError: set_scalar_covaraince() not defined for this response "
         << std::endl;
    abort_handler(-1);
  }
}

void Response::set_full_covariance(std::vector<RealMatrix> &matrices,
                                   std::vector<RealVector> &diagonals,
                                   RealVector &scalars,
                                   IntVector matrix_map_indices,
                                   IntVector diagonal_map_indices,
                                   IntVector scalar_map_indices )
{
  if (responseRep)
    responseRep->set_full_covariance(matrices, diagonals, scalars, matrix_map_indices, diagonal_map_indices, scalar_map_indices);
  else {
    Cerr << "\nError: set_full_covariance() not defined for this response "
         << std::endl;
    abort_handler(-1);
  }
}

Real Response::get_scalar_covariance(const int this_response)
{
  if (responseRep)
    return responseRep->get_scalar_covariance(this_response);
  else {
    Cerr << "\nError: get_scalar_covariance() not defined for this response "
         << std::endl;
    abort_handler(-1);
    return 0;
  }
}

Real Response::apply_covariance(const RealVector &residual)
{
  if (responseRep)
    return responseRep->apply_covariance(residual);
  else {
    Cerr << "\nError: apply_covariance not defined for this response "
         << std::endl;
    abort_handler(-1);
    return 0;
  }
}

void Response::apply_covariance_inv_sqrt(const RealVector& residuals, 
					 RealVector& weighted_residuals)
{
  if (responseRep)
    responseRep->apply_covariance_inv_sqrt(residuals, weighted_residuals);
  else {
    Cerr << "\nError: apply_covariance_invsqrt not defined for this response "
         << std::endl;
    abort_handler(-1);
  }
}

void Response::apply_covariance_inv_sqrt(const RealMatrix& gradients, 
					 RealMatrix& weighted_gradients)
{
  if (responseRep)
    responseRep->apply_covariance_inv_sqrt(gradients, weighted_gradients);
  else {
    Cerr << "\nError: apply_covariance_invsqrt not defined for this response "
         << std::endl;
    abort_handler(-1);
  }
}

void Response::apply_covariance_inv_sqrt(const RealSymMatrixArray& hessians,
					 RealSymMatrixArray& weighted_hessians)
{
  if (responseRep)
    responseRep->apply_covariance_inv_sqrt(hessians, weighted_hessians);
  else {
    Cerr << "\nError: apply_covariance_invsqrt not defined for this response "
         << std::endl;
    abort_handler(-1);
  }
}

void Response::get_covariance_diagonal( RealVector &diagonal) const
{
  if (responseRep)
    responseRep->get_covariance_diagonal( diagonal );
  else {
    Cerr << "\nError: get_covariance_diagonal not defined for this response "
         << std::endl;
    abort_handler(-1);
  }
}

/** Implementation of serialization load for the Response handle */
template<class Archive>
void Response::load(Archive& ar, const unsigned int version)
{
  short type;
  ar & type;
  if (responseRep) { // should not occur in current usage
    if (responseRep->sharedRespData.is_null() ||
	type != responseRep->sharedRespData.response_type()) {
      if (--responseRep->referenceCount == 0)
	delete responseRep;
      responseRep = get_response(type);
    }
  }
  else // read from restart: responseRep must be instantiated
    responseRep = get_response(type);

  responseRep->load_rep(ar, version); // fwd to new/existing rep
  responseRep->sharedRespData.response_type(type);
}


/** Implementation of serialization save for the Response handle */
template<class Archive>
void Response::save(Archive& ar, const unsigned int version) const
{
  if (responseRep)
    responseRep->save(ar, version);
  else {
    short type = sharedRespData.response_type();
    ar & type;
    save_rep(ar, version);
  }
}


/** Binary version differs from ASCII version in 2 primary ways:
    (1) it lacks formatting.
    (2) the Response has not been sized a priori.  In reading data from
        the binary restart file, a ParamResponsePair was constructed with
        its default constructor which called the Response default 
        constructor.  Therefore, we must first read sizing data and resize 
        all of the arrays. */
template<class Archive> 
void Response::load_rep(Archive& ar, const unsigned int version)
{
  // First read sharedRespData.  The shared data is serialized through
  // the SRD handle, with pointer tracking for the SRD body (rep)
  ar & sharedRespData;

  // Read responseActiveSet and flags for sizing the response
  ar & responseActiveSet;
  bool grad_flag = false, hess_flag = false;
  ar & grad_flag;
  ar & hess_flag;

  // reshape response arrays and reset all data to zero
  const ShortArray& asv = responseActiveSet.request_vector();
  size_t num_fns = asv.size();
  size_t num_params = responseActiveSet.derivative_vector().size();

  reshape(num_fns, num_params, grad_flag, hess_flag);
  reset();

  // Get fn. values as governed by ASV requests
  for (size_t i=0; i<num_fns; ++i)
    if (asv[i] & 1) // & 1 masks off 2nd and 3rd bit
      ar & functionValues[i];

  // Get function gradients as governed by ASV requests
  for (int i=0; i<num_fns; ++i)
    if (asv[i] & 2) // & 2 masks off 1st and 3rd bit
      read_sdm_col(ar, i, functionGradients);
    
  // Get function Hessians as governed by ASV requests
  for (size_t i=0; i<num_fns; ++i)
    if (asv[i] & 4) // & 4 masks off 1st and 2nd bit
      ar & functionHessians[i];
}


/** Binary version differs from ASCII version in 2 primary ways:
    (1) It lacks formatting.
    (2) In reading data from the binary restart file, ParamResponsePairs are
        constructed with their default constructor which calls the Response
        default constructor.  Therefore, we must first write sizing data so
        that Response::read(BoStream& s) can resize the arrays. */
template<class Archive> 
void Response::save_rep(Archive& ar, const unsigned int version) const
{    
  // First write sharedRespData.  The shared data is serialized through 
  // the SRD handle, with pointer tracking for the SRD body (rep)
  ar & sharedRespData;

  // Write responseActiveSet and flags for sizing the response
  ar & responseActiveSet;
  bool grad_flag = !functionGradients.empty(), 
    hess_flag = !functionHessians.empty();
  ar & grad_flag;
  ar & hess_flag;

  const ShortArray& asv = responseActiveSet.request_vector();
  size_t num_fns = asv.size();

  // Write the function values if present
  for (size_t i=0; i<num_fns; ++i)
    if (asv[i] & 1) // & 1 masks off 2nd and 3rd bit
      ar & functionValues[i];

  // Write the function gradients if present
  for (int i=0; i<num_fns; ++i)
    if (asv[i] & 2) // & 2 masks off 1st and 3rd bit
      write_sdm_col(ar, i, functionGradients);

  // Write the function Hessians if present
  for (size_t i=0; i<num_fns; ++i)
    if (asv[i] & 4) // & 4 masks off 1st and 2nd bit
      ar & functionHessians[i];
}

/// convenience fnction to write a serial dense matrix column to an Archive
template<class Archive, typename OrdinalType, typename ScalarType>
void Response::write_sdm_col
(Archive& ar, int col,
 const Teuchos::SerialDenseMatrix<OrdinalType, ScalarType>& sdm) const
{
  OrdinalType nr = sdm.numRows(); 
  const ScalarType* sdm_c = sdm[col]; // column vector 
  for (OrdinalType row=0; row<nr; ++row) 
    ar & sdm_c[row];
}

/// convenience fnction to read a serial dense matrix column from an Archive
template<class Archive, typename OrdinalType, typename ScalarType>
void Response::read_sdm_col
(Archive& ar, int col, Teuchos::SerialDenseMatrix<OrdinalType, ScalarType>& sdm)
{
  OrdinalType nr = sdm.numRows(); 
  ScalarType* sdm_c = sdm[col]; // column vector
  for (OrdinalType row=0; row<nr; ++row) 
    ar & sdm_c[row];
}

// These shouldn't be necessary, but using to avoid static linking
// issues until can find the right Boost macro ordering
template void Response:: 
load<boost::archive::binary_iarchive>(boost::archive::binary_iarchive& ar, 
				      const unsigned int version); 
template void Response:: 
save<boost::archive::binary_oarchive>(boost::archive::binary_oarchive& ar, 
				      const unsigned int version) const; 

/*
// These shouldn't be necessary, but using to avoid static linking
// issues until can find the right Boost macro ordering
template void Response::
load_rep<boost::archive::binary_iarchive>(boost::archive::binary_iarchive& ar, 
				      const unsigned int version);

template void Response::
save_rep<boost::archive::binary_oarchive>(boost::archive::binary_oarchive& ar, 
				      const unsigned int version) const;

*/

} // namespace Dakota
