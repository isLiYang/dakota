/*  _______________________________________________________________________

    DAKOTA: Design Analysis Kit for Optimization and Terascale Applications
    Copyright 2014 Sandia Corporation.
    This software is distributed under the GNU Lesser General Public License.
    For more information, see the README file in the top Dakota directory.
    _______________________________________________________________________ */

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cctype>

enum var_t { W, T, R, E, X, Y };


int main(int argc, char** argv)
{
  using namespace std;

  // This test problem is an OUU example from Applied Research Associates
  // (42nd AIAA SDM conference, April 2001).

  ifstream fin(argv[1]);
  if (!fin) {
    cerr << "\nError: failure opening " << argv[1] << endl;
    exit(-1);
  }
  size_t i, j, num_vars, num_fns, num_deriv_vars;
  string vars_text, fns_text, dvv_text;

  // define the string to enumeration map
  map<string, var_t> var_t_map;
  var_t_map["w"] = W; var_t_map["t"] = T;
  var_t_map["r"] = R; var_t_map["e"] = E;
  var_t_map["x"] = X; var_t_map["y"] = Y;

  // Get the parameter vector and ignore the labels
  fin >> num_vars >> vars_text;
  map<var_t, double> vars;
  vector<var_t> labels(num_vars);
  double var_i; string label_i; var_t v_i;
  map<string, var_t>::iterator v_iter;
  for (i=0; i<num_vars; i++) {
    fin >> var_i >> label_i;
    transform(label_i.begin(), label_i.end(), label_i.begin(),
	      (int(*)(int))tolower);
    v_iter = var_t_map.find(label_i);
    if (v_iter == var_t_map.end()) {
      cerr << "Error: label \"" << label_i << "\" not supported in analysis "
	   << "driver." << endl;
      exit(-1);
    }
    else
      v_i = v_iter->second;
    vars[v_i] = var_i;
    labels[i] = v_i;
  }

  // Get the ASV vector and ignore the labels
  fin >> num_fns >> fns_text;
  vector<short> ASV(num_fns);
  for (i=0; i<num_fns; i++) {
    fin >> ASV[i];
    fin.ignore(256, '\n');
  }

  // Get the DVV vector and ignore the labels
  fin >> num_deriv_vars >> dvv_text;
  vector<var_t> DVV(num_deriv_vars);
  unsigned int dvv_i;
  for (i=0; i<num_deriv_vars; i++) {
    fin >> dvv_i;
    fin.ignore(256, '\n');
    DVV[i] = labels[dvv_i-1];
  }

  if (num_vars != 4 && num_vars != 6) {
    cerr << "Error: Wrong number of variables in cantilever test fn." << endl;
    exit(-1);
  }
  if (num_fns != 3) {
    cerr << "Error: wrong number of response functions in cantilever test fn."
         << endl;
    exit(-1);
  }

  // Compute the cross-sectional area, stress, and displacement of the
  // cantilever beam.  This simulator is unusual in that it supports both
  // the case of design variable insertion and the case of design variable
  // augmentation.  It does not support mixed insertion/augmentation.  In
  // the 6 variable case, w,t,R,E,X,Y are all passed in; in the 4 variable
  // case, w,t assume local values.
  map<var_t, double>::iterator m_iter = vars.find(W);
  double w = (m_iter == vars.end()) ? 2.5 : m_iter->second; // beam width
  m_iter = vars.find(T);
  double t = (m_iter == vars.end()) ? 2.5 : m_iter->second; // beam thickness
  double r = vars[R]; // yield strength
  double e = vars[E]; // Young's modulus
  double x = vars[X]; // horizontal load
  double y = vars[Y]; // vertical load

  // UQ limit state <= 0: don't scale stress by random variable r
  //double g_stress = stress - r;
  //double g_disp   = displ  - D0;

  // Compute the results and output them directly to argv[2] (the NO_FILTER
  // option is used).  Response tags are optional; output them for ease of
  // results readability.
  double D0 = 2.2535, L = 100., area = w*t, w_sq = w*w, t_sq = t*t,
    r_sq = r*r, x_sq = x*x, y_sq = y*y;
  double stress = 600.*y/w/t_sq + 600.*x/w_sq/t;
  double D1 = 4.*pow(L,3)/e/area, D2 = pow(y/t_sq, 2)+pow(x/w_sq, 2);
  double D3 = D1/sqrt(D2),        displ = D1*sqrt(D2);

  ofstream fout(argv[2]); // do not instantiate until ready to write results
  if (!fout) {
    cerr << "\nError: failure creating " << argv[2] << endl;
    exit(-1);
  }
  fout.precision(15); // 16 total digits
  fout.setf(ios::scientific);
  fout.setf(ios::right);

  // **** f:
  if (ASV[0] & 1)
    fout << "                     " << area << '\n';

  // **** c1:
  if (ASV[1] & 1)
    fout << "                     " << stress - r << '\n';

  // **** c2:
  if (ASV[2] & 1)
    fout << "                     " << displ - D0 << '\n';

  // **** df/dx:
  if (ASV[0] & 2) {
    fout << "[ ";
    for (i=0; i<num_deriv_vars; i++)
      switch (DVV[i]) {
      case W:  fout << t << ' '; break; // design var derivative
      case T:  fout << w << ' '; break; // design var derivative
      default: fout << "0. ";    break; // uncertain var derivative
      }
    fout << "]\n";
  }

  // **** dc1/dx:
  if (ASV[1] & 2) {
    fout << "[ ";
    for (i=0; i<num_deriv_vars; i++)
      switch (DVV[i]) {
      case W: fout << -600.*(y/t + 2.*x/w)/w_sq/t << ' '; break; // des var
      case T: fout << -600.*(2.*y/t + x/w)/w/t_sq << ' '; break; // des var
      case R: fout << "-1. ";             break; // uncertain var deriv
      case E: fout << "0. ";              break; // uncertain var deriv
      case X: fout << 600./w_sq/t << ' '; break; // uncertain var deriv
      case Y: fout << 600./w/t_sq << ' '; break; // uncertain var deriv
      }
    fout << "]\n";
  }

  // **** dc2/dx:
  if (ASV[2] & 2) {
    fout << "[ ";
    for (i=0; i<num_deriv_vars; i++)
      switch (DVV[i]) {
      case W: fout << -D3*2.*x_sq/w_sq/w_sq/w - displ/w << ' '; break; // dv
      case T: fout << -D3*2.*y_sq/t_sq/t_sq/t - displ/t << ' '; break; // dv
      case R: fout << "0. ";                  break; // unc var deriv
      case E: fout << -displ/E        << ' '; break; // unc var deriv
      case X: fout <<  D3*x/w_sq/w_sq << ' '; break; // unc var deriv
      case Y: fout <<  D3*y/t_sq/t_sq << ' '; break; // unc var deriv
      }
    fout << "]\n";
  }

  /* Alternative modification: take E out of displ denominator to remove
     singularity in tail (at 20 std deviations).  In PCE/SC testing, this
     had minimal impact and did not justify the nonstandard form.

  double D0 = 2.2535, L = 100., area = w*t, w_sq = w*w, t_sq = t*t,
    r_sq = r*r, x_sq = x*x, y_sq = y*y;
  double stress = 600.*y/w/t_sq + 600.*x/w_sq/t;
  double D1 = 4.*pow(L,3)/area, D2 = pow(y/t_sq, 2)+pow(x/w_sq, 2);
  double D3 = D1/sqrt(D2),      D4 = D1*sqrt(D2);

  // **** c2:
  if (ASV[2] & 1)
    fout << "                     " << D4 - D0*e << '\n';

  // **** dc2/dx:
  if (ASV[2] & 2) {
    fout << "[ ";
    for (i=0; i<num_deriv_vars; i++)
      switch (DVV[i]) {
      case W: fout << -D3*2.*x_sq/w_sq/w_sq/w - D4/w << ' '; break; // des var
      case T: fout << -D3*2.*y_sq/t_sq/t_sq/t - D4/t << ' '; break; // des var
      case R: fout << "0. ";                  break; // unc var deriv
      case E: fout << -D0             << ' '; break; // unc var deriv
      case X: fout <<  D3*x/w_sq/w_sq << ' '; break; // unc var deriv
      case Y: fout <<  D3*y/t_sq/t_sq << ' '; break; // unc var deriv
      }
    fout << "]\n";
  }
  */

  fout.flush();
  fout.close();  
  return 0;
}
