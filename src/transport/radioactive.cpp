#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include "radioactive.h"
#include "physical_constants.h"
#include "sedona.h"


namespace pc = physical_constants;


// energy (Mev) an probability (# per decay) of gamma rays from a Co56 decay
int n_co56_decays   = 17;
double co56_prob[]  = { 0.360,   1.000,   0.015,   0.137,  0.022,  0.670,
                         0.043,   0.158,   0.031,   0.079,  0.166,  0.058,
                         0.030,   0.074,   0.018,   0.009,  0.000};
double co56_energy[] = { 0.511,   0.847,   0.980,   1.040,  1.180,  1.240,
                         1.360,   1.770,   2.015,   2.030,  2.600,  3.010,
                         3.200,   3.250,   3.270,   3.450,  4.000};
 
/*const double co56_positron_fraction = 0.032478;
const double co56_prob[] = {0.39, 0.00191, 0.00311, 0.999399, 0.00049, 
                              0.00073, 0.01421, 0.00111, 0.1405, 0.00055, 
                              0.00132, 0.00094, 0.02252, 0.00049, 0.6646, 
                              0.001224, 0.04283, 0.0018, 0.00074, 0.000616, 
                              0.1541, 0.0064, 0.00707, 0.03016, 0.0777, 
                              0.00377, 0.00388, 0.00118, 0.0008, 0.00059, 
                              0.1697, 0.00019, 0.01036, 0.03209, 0.07923, 
                              0.018759, 0.00949, 0.001955, 0.000167};
const double co56_engy[] = {0.511, 0.733514, 0.787743, 0.84677, 0.852732, 
                              0.89651, 0.977372, 0.996948, 1.037843, 1.088894, 
                              1.140368, 1.159944, 1.175101, 1.198888, 1.238288, 
                              1.3354, 1.360212, 1.442746, 1.462322, 1.640475, 
                              1.771357, 1.810757, 1.963741, 2.015215, 2.034791, 
                              2.113135, 2.212944, 2.276131, 2.37324, 2.52309, 
                              2.5985, 2.657527, 3.009645, 3.202029, 3.253503, 
                              3.273079, 3.451232, 3.54805, 3.6008};
*/

const int n_ni56_decays    =  7;
const double ni56_prob[] = {0.988, 0.365, 0.365, 0.495, 0.86, 0.14};
const double ni56_engy[] = {0.15838, 0.2695, 0.48044, 0.74995, 0.81185, 1.5618};




int    n_R_proc_fit = 11;
double R_proc_fit[] =  {17.608179, -2.0442059, -0.42565322, 0.39830095,  -0.0059089906,
			-0.054805836, 0.014068697, -0.00086706160, -5.7056758e-05, 2.6401842e-06,
			3.7186979e-07};



//--------------------------------------------------------------
// decay composition
//--------------------------------------------------------------
void radioactive::decay_composition
(const std::vector<int> &elems_Z, const std::vector<int> &elems_A, std::vector<double>& X, const double t)
{
  int n_el = elems_Z.size();

  // 56nickel decay
  double X_ni = 0;
  double X_co = 0;  
  for (int i=0;i<n_el;i++)
  {
    if ((elems_Z[i] == 28)&&(elems_A[i] == 56)) X_ni = X[i];
    if ((elems_Z[i] == 27)&&(elems_A[i] == 56)) X_co = X[i];
  }

  double ni_f = exp(-t/TAU_56NI);
  double co_f = TAU_56CO/(TAU_56NI-TAU_56CO)*(exp(-t/TAU_56NI) - exp(-t/TAU_56CO));
  double fe_f = 1 - ni_f - co_f;  
  double eco = exp(-t/TAU_56CO);
  for (int i=0;i<n_el;i++)
  {
    if ((elems_Z[i] == 28)&&(elems_A[i] == 56)) 
      X[i] = X_ni*ni_f;
    if ((elems_Z[i] == 27)&&(elems_A[i] == 56)) 
      X[i] = X_ni*co_f + X_co*eco;
    if ((elems_Z[i] == 26)&&(elems_A[i] == 56)) 
      X[i] += X_ni*fe_f + X_co*(1 - eco);
    
  }


  // Cr48 decay
  double X_cr = 0;
  double X_vn = 0;  
  for (int i=0;i<n_el;i++)
  {
    if ((elems_Z[i] == 24)&&(elems_A[i] == 48)) X_cr = X[i];
    if ((elems_Z[i] == 23)&&(elems_A[i] == 48)) X_vn = X[i];
  }

  double cr_f = exp(-t/TAU_48CR);
  double vn_f = TAU_48V/(TAU_48CR-TAU_48V)*(exp(-t/TAU_48CR) - exp(-t/TAU_48V));
  double ti_f = 1 - cr_f - vn_f;  
  double evn = exp(-t/TAU_48V);
  for (int i=0;i<n_el;i++)
  {
    if ((elems_Z[i] == 24)&&(elems_A[i] == 48)) 
      X[i] = X_cr*cr_f;
    if ((elems_Z[i] == 23)&&(elems_A[i] == 48)) 
      X[i] = X_cr*vn_f + X_vn*evn;
    if ((elems_Z[i] == 22)&&(elems_A[i] == 48)) 
      X[i] += X_cr*ti_f + X_vn*(1 - evn);
    
  }


}


double radioactive::rprocess_heating_rate(double t, double *gfrac)
{
  // formula from Rosswog et al 2014 MNRAS 439
  //double eps = 0.5-1.0/pc::pi*atan((t - 1.3)/0.11);; //2.0e18*(0.5 - 1.0/pc::pi*atan((t - 1.3)/0.11));

  double td = t/3600.0/24.0;

  // Fit from Lippuner and Roberts 2015 ApJ 815
  // for Ye = 0.13, s = 32, tau = 0.84 ms for
  double A1 = 8.4939E+09;
  double alpha = 1.3642E+00;
  double B1 = 8.3425E+09;
  double beta1 = 3.6280E+00;
  double B2 = 8.8616E+08;
  double beta2 = 1.0847E+01;
  double eps = A1*pow(td,-1.0*alpha) + B1*exp(-1.0*td/beta1) + B2*exp(-1.0*td/beta2);

  // thermalization efficiency from Barnes et al 2016
  double af = 0.56;
  double bf = 0.17;
  double df = 0.74;
  double x = 2*bf*pow(td,df);
  double f = 0.36*(exp(-af*td) + log(1 + x)/x);

  *gfrac = 0;
  return eps*f;
}




//--------------------------------------------------------------
// returns the energy from radioactive decay, per time 
// integrated over all species
//--------------------------------------------------------------
double radioactive::decay
(std::vector<int> elem_Z, std::vector<int> elem_A, std::vector<double> X, double t, double *gfrac, int force_rproc)
{
  int rprocess_heat = 0;

  // see if this is an r-process composition (simplistic)
  for (unsigned int k=0;k<elem_Z.size();k++)
    if ((elem_Z[k] > 57)&&(elem_A[k] > 0) || force_rproc) rprocess_heat = 1;

  if (rprocess_heat)
  {
    double total = rprocess_heating_rate(t,gfrac);
    return total;
  }

  double total  = 0;
  double gtotal = 0;
  double gf;
  for (unsigned int k=0;k<elem_Z.size();k++)
  {
    int el_Z = elem_Z[k];
    int el_A = elem_A[k];
    double val = decay_energy_rate(el_Z,el_A,t,&gf);
    val = val*X[k]/(el_A*pc::m_p);;
    total  += val;
    gtotal += val*gf;
  }
  *gfrac = gtotal/total;
  if (total == 0) *gfrac = 0;
  
  return total;
       
  // adjust the compositions here
}
  
//--------------------------------------------------------------
// returns the energy from radioactive decay, per time per volume
//--------------------------------------------------------------
double radioactive::decay_energy_rate(int Z, int A, double t, double *gfrac)
{

  double total  = 0;
  double gtotal = 0;

  // Do 56Ni decay
  if ((Z == 28)&&(A == 56))
  {
    // exponential factors to be used
    double e_ni = exp(-t/TAU_56NI);
    double e_co = exp(-t/TAU_56CO);
    // number divided by decay time)
    double ni56 = (e_ni/TAU_56NI);
    double co56 = 1.0/(TAU_56NI-TAU_56CO)*(e_ni - e_co);
    // get the energy from decays in ergs/s, using unit conversions
    double ni_E = ni56*(AVERAGE_56NI_ENERGY*pc::Mev_to_ergs);
    double co_E = co56*(AVERAGE_56CO_ENERGY*pc::Mev_to_ergs);

    gtotal = ni_E + (1 - CO_POSITRON_FRACTION)*co_E;
    total  = ni_E + co_E;
  }

  // Do 56Co decay
  if ((Z == 27)&&(A == 56))
  {
    // exponential factors to be used
    double e_co = exp(-t/TAU_56CO);
    // number divided by decay time)
    double co56 = 1.0/(TAU_56CO)*e_co;
    // get the energy from decays in ergs/s, using unit conversions
    double co_E = co56*(AVERAGE_56CO_ENERGY*pc::Mev_to_ergs);

    gtotal = (1 - CO_POSITRON_FRACTION)*co_E;
    total  = co_E;
  }
    

  // Do 48Cr decay
  if ((Z == 24)&&(A == 48))
  {
    // exponential factors to be used
    double e_cr = exp(-t/TAU_48CR);
    double e_vn = exp(-t/TAU_48V);
    // number divided by decay time)
    double cr48 = (e_cr/TAU_48CR);
    double vn48 = 1.0/(TAU_48CR-TAU_48V)*(e_cr - e_vn);
    // get the energy from decays in ergs/s, using unit conversions
    double cr_E = cr48*(AVERAGE_48CR_ENERGY*pc::Mev_to_ergs);
    double vn_E = vn48*(AVERAGE_48V_ENERGY*pc::Mev_to_ergs);

    gtotal += cr_E + vn_E;
    total  += cr_E + vn_E;
  }



 // // Do 52Fe decay
  // if (ZONE::radio[i] == 26) 
  //   {
  //     // amount of 52Fe at t=0
  //     double fe52_0 = z.rho*z.R_gas[i]/(52*M_PROTON);

  //     // exponential factors to be used
  //     double e_fe = exp(-time/TAU_52FE);
  //     double e_mn = exp(-time/TAU_52MN);
  //     // number divided by decay time)
  //     double fe52 = fe52_0*(e_fe/TAU_52FE);
  //     double mn52 = fe52_0/(TAU_52FE-TAU_52MN)*(e_fe - e_mn);
  //     // get the energy from decays in ergs/s, using unit conversions
  //     double fe_E = fe52*(AVERAGE_52FE_ENERGY*MEV_TO_ERGS);
  //     double mn_E = mn52*(AVERAGE_52MN_ENERGY*MEV_TO_ERGS);

  //     gtotal += fe_E + mn_E;
  //     total  += fe_E + mn_E;
  //   }

  //   // R-process decay
    if ((Z >= 58))
    {
       double at = log10(t);
       double rproc = 0;
       for (int j=0;j<n_R_proc_fit;j++) rproc += R_proc_fit[j]*pow(at,j);
       rproc = pow(10.0,rproc)*(A*pc::m_p);

       // fission fragments versus beta decay
       double fission_E  = 0.1*rproc;
       double beta_E     = 0.9*rproc;
      
       // account for lost neutrino energy
       beta_E = 0.75*beta_E;
      
       total  += (fission_E + beta_E);
       // half of the beta energy comes out as gamma-rays
       gtotal += 0.5*beta_E;
     }
      
  *gfrac = gtotal/total;
  if (total == 0) *gfrac = 0;
  return total;
}



// void RADIOACTIVE::Fractions(int i, double *x, double t)
// {
//   if (i == 28) 
//   {
//     x[0] = exp(-t/TAU_56NI);
//     x[1] = TAU_56CO/(TAU_56NI-TAU_56CO)*(exp(-t/TAU_56NI) - exp(-t/TAU_56CO));
//     x[2] = 1 - x[0] - x[1];
//   }

//   if (i == 26) 
//   {
//     x[0] = exp(-t/TAU_52FE);
//     x[1] = TAU_52MN/(TAU_52FE-TAU_52MN)*(exp(-t/TAU_52FE) - exp(-t/TAU_52MN));
//     x[2] = 1 - x[0] - x[1];
//   }

//   if (i == 24) 
//   {
//     x[0] = exp(-t/TAU_48CR);
//     x[1] = TAU_48V/(TAU_48CR-TAU_48V)*(exp(-t/TAU_48CR) - exp(-t/TAU_48V));
//     x[2] = 1 - x[0] - x[1];
//   }
// }



//double radioactive::sample_particle_energy(int Z, int A, double t, gsl_rng *rangen)
//{

  // // Ni56
  // if ((Z == 28)&&(A == 56))
  // {
  
  //   // The Ratio Of Energy Coming Out In Nickel
  //   double E_Ni = exp(-t/TAU_56NI);
  //   double E_Co = exp(-t/TAU_56CO);
  //   double Ni_E = AVERAGE_56NI_ENERGY*(E_Ni/TAU_56NI);
  //   double Co_E = AVERAGE_56CO_ENERGY/(TAU_56NI - TAU_56CO)*(E_Ni - E_Co);
  //   double nico_ratio = Ni_E/(Ni_E + Co_E);
  
  //   // pick emission wavelength
  //   double x_val;
  //   int d;
  //   double z1 = gsl_rng_uniform(rangen);
  //   if (z1 < nico_ratio)
  //   {
  //     while (true)
  //     {
  // 	double z2 = gsl_rng_uniform(rangen);
  // 	double z3 = gsl_rng_uniform(rangen);
  // 	d = (int)(n_ni56_decays*z2);
  // 	if (z3 < ni56_prob[d]) break;
  //     }
  //     x_val = ni56_energy[d];
  //   }
  //   else
  //   {
  //   while (true)
  //   {
  //     double z2 = gsl_rng_uniform(rangen);
  //     double z3 = gsl_rng_uniform(rangen);
  //     d = (int)(n_co56_decays*z2);
  //     if (z3 < co56_prob[d]) break;
  //   }
  //   x_val = co56_energy[d];
  //   }
  // }
  
  // return x_val;
  //}




